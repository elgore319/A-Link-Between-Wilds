/*
 * Host-side tests for client/source/program/net/elf_symbols.hpp.
 *
 *     g++ -std=c++20 -Wall -Wextra -Werror -I../source test_elf_symbols.cpp -o test_elf_symbols && ./test_elf_symbols
 *
 * (CI runs this; see the "client-tests" job in .github/workflows/ci.yml.)
 * Builds a small fake module image (dynamic section, DT_HASH, symbols, strings)
 * laid out the way the Switch loader sees one, then looks names up in it.
 */
#include "program/net/elf_symbols.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace albw;

namespace {
    int g_failed = 0;
    int g_checks = 0;

    #define CHECK(cond) do { \
        g_checks++; \
        if (!(cond)) { g_failed++; std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } \
    } while (0)

    struct FakeSym { std::string name; uint64_t value; uint16_t shndx; };

    /* A fake module: one byte buffer holding everything, with offsets from its start. */
    struct FakeModule {
        std::vector<uint8_t> image;
        size_t dyn_off = 0;

        uintptr_t Base() const { return reinterpret_cast<uintptr_t>(image.data()); }
        const elf::Dyn* Dynamic() const { return reinterpret_cast<const elf::Dyn*>(image.data() + dyn_off); }

        template<typename T> size_t Append(const T* data, size_t count) {
            while (image.size() % 8) image.push_back(0);
            size_t off = image.size();
            const uint8_t* p = reinterpret_cast<const uint8_t*>(data);
            image.insert(image.end(), p, p + sizeof(T) * count);
            return off;
        }

        FakeModule(const std::vector<FakeSym>& syms, uint32_t nbucket, bool with_strsz = true) {
            /* Symbol 0 is the null symbol, as in every ELF. */
            std::string strtab(1, '\0');
            std::vector<elf::Sym> symtab(1 + syms.size());
            for (size_t i = 0; i < syms.size(); i++) {
                symtab[i + 1] = {};
                symtab[i + 1].name = static_cast<uint32_t>(strtab.size());
                symtab[i + 1].value = syms[i].value;
                symtab[i + 1].shndx = syms[i].shndx;
                strtab += syms[i].name;
                strtab.push_back('\0');
            }
            const uint32_t nchain = static_cast<uint32_t>(symtab.size());
            std::vector<uint32_t> hash(2 + nbucket + nchain, 0);
            hash[0] = nbucket; hash[1] = nchain;
            uint32_t* bucket = &hash[2];
            uint32_t* chain = &hash[2 + nbucket];
            for (uint32_t i = 1; i < nchain; i++) {
                uint32_t b = elf::Hash(syms[i - 1].name.c_str()) % nbucket;
                chain[i] = bucket[b];   /* push to the front of the bucket's chain */
                bucket[b] = i;
            }

            image.assign(64, 0xEE);   /* some leading bytes so offsets aren't 0 */
            size_t str_off  = Append(strtab.data(), strtab.size());
            size_t sym_off  = Append(symtab.data(), symtab.size());
            size_t hash_off = Append(hash.data(), hash.size());
            std::vector<elf::Dyn> dyn = {
                { elf::DtHash,   hash_off },
                { elf::DtStrtab, str_off },
                { elf::DtSymtab, sym_off },
            };
            if (with_strsz) dyn.push_back({ elf::DtStrsz, strtab.size() });
            dyn.push_back({ elf::DtNull, 0 });
            dyn_off = Append(dyn.data(), dyn.size());
        }
    };

    const std::vector<FakeSym> SdkLike = {
        { "_ZN2nn6socket6SocketEiii",              0x1000, 1 },
        { "_ZN2nn6socket7ConnectEiPK8sockaddrj",   0x1100, 1 },
        { "_ZN2nn6socket4SendEiPKvmi",             0x1200, 1 },
        { "_ZN2nn6socket4RecvEiPvmi",              0x1300, 1 },
        { "_ZN2nn2fs8OpenFileEPNS0_10FileHandleEPKci", 0x2000, 1 },
        { "_ZN2nn6socket12GetLastErrnoEv",         0,      0 },  /* imported, not defined */
        { "memcpy",                                0x3000, 1 },
    };

    void TestFindsDefinedSymbols() {
        FakeModule m(SdkLike, 3);
        elf::SymbolTable t(m.Base(), m.Dynamic());
        CHECK(t.Valid());
        CHECK(t.Find("_ZN2nn6socket6SocketEiii") == m.Base() + 0x1000);
        CHECK(t.Find("_ZN2nn6socket4RecvEiPvmi") == m.Base() + 0x1300);
        CHECK(t.Find("memcpy") == m.Base() + 0x3000);
    }

    void TestMissingAndUndefined() {
        FakeModule m(SdkLike, 3);
        elf::SymbolTable t(m.Base(), m.Dynamic());
        CHECK(t.Find("_ZN2nn6socket6SocketENS0_6FamilyENS0_4TypeENS0_8ProtocolE") == 0);
        CHECK(t.Find("_ZN2nn6socket12GetLastErrnoEv") == 0);   /* undefined import */
        CHECK(t.Find("") == 0);
        CHECK(t.Find(nullptr) == 0);
    }

    void TestOneBucket() {
        /* Every symbol in one long chain still works. */
        FakeModule m(SdkLike, 1);
        elf::SymbolTable t(m.Base(), m.Dynamic());
        for (const auto& s : SdkLike)
            if (s.shndx) CHECK(t.Find(s.name.c_str()) == m.Base() + s.value);
    }

    void TestNoStrsz() {
        FakeModule m(SdkLike, 4, false);
        elf::SymbolTable t(m.Base(), m.Dynamic());
        CHECK(t.Find("_ZN2nn6socket4SendEiPKvmi") == m.Base() + 0x1200);
    }

    void TestPrefixListing() {
        FakeModule m(SdkLike, 3);
        elf::SymbolTable t(m.Base(), m.Dynamic());
        std::vector<std::string> seen;
        int n = t.ForEachWithPrefix("_ZN2nn6socket", 100, [&](const char* name) { seen.push_back(name); });
        CHECK(n == 4);                 /* the undefined GetLastErrno is skipped */
        CHECK(seen.size() == 4);
        seen.clear();
        n = t.ForEachWithPrefix("_ZN2nn6socket", 2, [&](const char* name) { seen.push_back(name); });
        CHECK(n == 4);                 /* total still counted ... */
        CHECK(seen.size() == 2);       /* ... but only `max` reported */
    }

    void TestInvalidTables() {
        elf::SymbolTable none;
        CHECK(!none.Valid());
        CHECK(none.Find("x") == 0);
        elf::SymbolTable null_dyn(0x1000, nullptr);
        CHECK(!null_dyn.Valid());
        elf::Dyn only_null[] = { { elf::DtNull, 0 } };
        elf::SymbolTable empty(0, only_null);
        CHECK(!empty.Valid());
    }

    void TestCycleDoesNotHang() {
        /* A corrupt chain that loops back on itself must end, not spin forever. */
        FakeModule m(SdkLike, 1);
        elf::SymbolTable t(m.Base(), m.Dynamic());
        auto* dyn = m.Dynamic();
        auto* hash = reinterpret_cast<uint32_t*>(m.Base() + dyn[0].val);
        uint32_t nbucket = hash[0];
        uint32_t* chain = &hash[2 + nbucket];
        uint32_t head = hash[2];
        chain[chain[head]] = head;     /* second link points back to the first */
        CHECK(t.Find("not_there") == 0);
    }

    void TestHash() {
        /* Reference values from the SysV ABI hash (same as the Switch rtld's). */
        CHECK(elf::Hash("") == 0);
        CHECK(elf::Hash("printf") == 0x077905a6);
        CHECK(elf::Hash("exit") == 0x0006cf04);
    }

    void TestParseIpv4() {
        uint32_t v = 0;
        CHECK(net::ParseIpv4("127.0.0.1", &v) && v == 0x0100007Fu);
        CHECK(net::ParseIpv4("192.168.1.20", &v) && v == 0x1401A8C0u);
        CHECK(net::ParseIpv4("0.0.0.0", &v) && v == 0);
        CHECK(net::ParseIpv4("255.255.255.255", &v) && v == 0xFFFFFFFFu);
        CHECK(!net::ParseIpv4("256.0.0.1", &v));
        CHECK(!net::ParseIpv4("1.2.3", &v));
        CHECK(!net::ParseIpv4("1.2.3.4.5", &v));
        CHECK(!net::ParseIpv4("1.2.3.4 ", &v));
        CHECK(!net::ParseIpv4("a.b.c.d", &v));
        CHECK(!net::ParseIpv4("1..2.3", &v));
        CHECK(!net::ParseIpv4("0001.2.3.4", &v));
        CHECK(!net::ParseIpv4("", &v));
        CHECK(!net::ParseIpv4(nullptr, &v));
        CHECK(net::Htons(55420) == 0x7CD8);
    }
}

int main() {
    TestFindsDefinedSymbols();
    TestMissingAndUndefined();
    TestOneBucket();
    TestNoStrsz();
    TestPrefixListing();
    TestInvalidTables();
    TestCycleDoesNotHang();
    TestHash();
    TestParseIpv4();
    std::printf("%d/%d checks passed\n", g_checks - g_failed, g_checks);
    return g_failed ? 1 : 0;
}
