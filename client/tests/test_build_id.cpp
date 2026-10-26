/*
 * Host-side tests for client/source/program/game/build_id.hpp.
 *
 *     g++ -std=c++20 -Wall -Wextra -Werror -I../source test_build_id.cpp -o test_build_id && ./test_build_id
 *
 * (CI runs this; see the "client-tests" job in .github/workflows/ci.yml.)
 */
#include "program/game/build_id.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

using namespace albw::game;

namespace {
    int g_failed = 0;
    int g_checks = 0;

    #define CHECK(cond) do { \
        g_checks++; \
        if (!(cond)) { g_failed++; std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } \
    } while (0)

    void PutU32(std::vector<uint8_t>& v, uint32_t x) {
        for (int i = 0; i < 4; i++) v.push_back((x >> (8 * i)) & 0xFF);
    }

    /* Some filler, then a GNU build ID note with `len` bytes of `id`. */
    std::vector<uint8_t> Rodata(const uint8_t* id, uint32_t len, size_t filler = 0x40) {
        std::vector<uint8_t> v(filler, 0xAB);
        PutU32(v, 4); PutU32(v, len); PutU32(v, 3);
        v.push_back('G'); v.push_back('N'); v.push_back('U'); v.push_back(0);
        v.insert(v.end(), id, id + len);
        v.resize(v.size() + 0x20, 0xCD);
        return v;
    }

    void TestFinds100() {
        auto v = Rodata(Botw100BuildId, 16);
        uint8_t out[BuildIdSize];
        CHECK(FindGnuBuildId(v.data(), v.size(), out));
        CHECK(SameBuildId(out, Botw100BuildId));
        CHECK(!SameBuildId(out, Botw160BuildId));
        char hex[33];
        FormatBuildId(out, hex);
        CHECK(std::strcmp(hex, "2094F152836683A3632C6153FD84AB80") == 0);
    }

    void TestFinds160() {
        auto v = Rodata(Botw160BuildId, 16, 0x1000);
        uint8_t out[BuildIdSize];
        CHECK(FindGnuBuildId(v.data(), v.size(), out));
        CHECK(SameBuildId(out, Botw160BuildId));
        char hex[33];
        FormatBuildId(out, hex);
        CHECK(std::strcmp(hex, "8E9978D50BDD20B4C8395A106C27FFDE") == 0);
    }

    void TestSha1LengthIsZeroPadded() {
        uint8_t id[20];
        for (int i = 0; i < 20; i++) id[i] = (uint8_t)(i + 1);
        auto v = Rodata(id, 20);
        uint8_t out[BuildIdSize];
        CHECK(FindGnuBuildId(v.data(), v.size(), out));
        CHECK(out[19] == 20 && out[20] == 0 && out[31] == 0);
    }

    void TestNoNote() {
        std::vector<uint8_t> v(0x200, 0);
        uint8_t out[BuildIdSize];
        CHECK(!FindGnuBuildId(v.data(), v.size(), out));
        CHECK(!FindGnuBuildId(v.data(), 0, out));
    }

    void TestRejectsLookalikes() {
        /* Wrong type (1 = NT_GNU_ABI_TAG) and wrong name are skipped. */
        std::vector<uint8_t> v;
        PutU32(v, 4); PutU32(v, 16); PutU32(v, 1);
        v.push_back('G'); v.push_back('N'); v.push_back('U'); v.push_back(0);
        v.resize(v.size() + 16, 0x11);
        PutU32(v, 4); PutU32(v, 16); PutU32(v, 3);
        v.push_back('G'); v.push_back('N'); v.push_back('X'); v.push_back(0);
        v.resize(v.size() + 16, 0x22);
        uint8_t out[BuildIdSize];
        CHECK(!FindGnuBuildId(v.data(), v.size(), out));

        /* A real note after them is still found. */
        auto real = Rodata(Botw100BuildId, 16, 0);
        v.insert(v.end(), real.begin(), real.end());
        CHECK(FindGnuBuildId(v.data(), v.size(), out));
        CHECK(SameBuildId(out, Botw100BuildId));
    }

    void TestTruncatedNote() {
        auto v = Rodata(Botw100BuildId, 16, 0);
        uint8_t out[BuildIdSize];
        /* Header present but the ID runs past the end. */
        CHECK(!FindGnuBuildId(v.data(), 16 + 8, out));
    }
}

int main() {
    TestFinds100();
    TestFinds160();
    TestSha1LengthIsZeroPadded();
    TestNoNote();
    TestRejectsLookalikes();
    TestTruncatedNote();
    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
