#pragma once

/*
 * Look up exported functions in another module of the game (normally "sdk")
 * by reading its ELF dynamic symbol table directly (decision 0011).
 *
 * Why: if the module imported nn::socket functions the normal way and one of
 * the guessed signatures was wrong, the loader would leave that import at 0 and
 * the game would crash the first time we called it, with nothing in the log.
 * Looking the names up ourselves lets us try several spellings, log exactly
 * which one is missing, and keep the game running with networking off.
 *
 * Pure code with no Switch dependencies, so client/tests/test_elf_symbols.cpp
 * can check it on a PC.
 */

#include <stddef.h>
#include <stdint.h>

namespace albw::elf {

    /* Minimal 64-bit ELF types (same layout as <elf.h>), kept local so the host
       tests don't depend on the system's elf.h. */
    struct Dyn {
        int64_t  tag;
        uint64_t val;
    };

    struct Sym {
        uint32_t name;
        uint8_t  info;
        uint8_t  other;
        uint16_t shndx;
        uint64_t value;
        uint64_t size;
    };
    static_assert(sizeof(Dyn) == 16);
    static_assert(sizeof(Sym) == 24);

    constexpr int64_t DtNull   = 0;
    constexpr int64_t DtHash   = 4;
    constexpr int64_t DtStrtab = 5;
    constexpr int64_t DtSymtab = 6;
    constexpr int64_t DtStrsz  = 10;

    constexpr uint16_t ShnUndef  = 0;
    constexpr uint16_t ShnCommon = 0xFFF2;

    /* The classic SysV ELF hash, as used by DT_HASH and the Switch's rtld. */
    inline uint32_t Hash(const char* name) {
        uint32_t h = 0;
        while (*name) {
            h = (h << 4) + static_cast<uint8_t>(*name++);
            uint32_t g = h & 0xF0000000u;
            if (g) h ^= g >> 24;
            h &= ~g;
        }
        return h;
    }

    inline bool StrEq(const char* a, const char* b) {
        while (*a && *a == *b) { a++; b++; }
        return *a == *b;
    }

    inline bool StartsWith(const char* s, const char* prefix) {
        while (*prefix) {
            if (*s++ != *prefix++) return false;
        }
        return true;
    }

    /* One module's dynamic symbol table. Addresses in the dynamic section are
       offsets from the module's base, as on the Switch. */
    class SymbolTable {
        public:
            SymbolTable() = default;

            SymbolTable(uintptr_t base, const Dyn* dynamic) : m_base(base) {
                if (dynamic == nullptr) return;
                /* A real dynamic section has a few dozen entries; the cap only
                   guards against walking off into garbage. */
                for (int i = 0; i < 256 && dynamic[i].tag != DtNull; i++) {
                    const uint64_t v = dynamic[i].val;
                    switch (dynamic[i].tag) {
                        case DtHash: {
                            auto* table = reinterpret_cast<const uint32_t*>(base + v);
                            m_nbucket = table[0];
                            m_nchain  = table[1];
                            m_bucket  = table + 2;
                            m_chain   = table + 2 + m_nbucket;
                            break;
                        }
                        case DtStrtab: m_strtab = reinterpret_cast<const char*>(base + v); break;
                        case DtSymtab: m_symtab = reinterpret_cast<const Sym*>(base + v); break;
                        case DtStrsz:  m_strsz  = v; break;
                        default: break;
                    }
                }
            }

            bool Valid() const {
                return m_bucket && m_chain && m_strtab && m_symtab && m_nbucket > 0 && m_nchain > 0;
            }

            /* Address of the defined symbol `name`, or 0 if this module doesn't export it. */
            uintptr_t Find(const char* name) const {
                if (!Valid() || name == nullptr) return 0;
                /* The chain can't be longer than the symbol count; bounding the walk
                   means a corrupt table ends the loop instead of hanging. */
                uint32_t steps = 0;
                for (uint32_t i = m_bucket[Hash(name) % m_nbucket]; i != 0 && i < m_nchain && steps <= m_nchain; i = m_chain[i], steps++) {
                    const Sym& s = m_symtab[i];
                    if (!IsDefined(s) || !NameOk(s)) continue;
                    if (StrEq(name, m_strtab + s.name))
                        return m_base + s.value;
                }
                return 0;
            }

            /* Calls fn(name) for every defined symbol whose name starts with `prefix`,
               up to `max` of them. Returns how many matched in total. */
            template<typename Fn>
            int ForEachWithPrefix(const char* prefix, int max, Fn&& fn) const {
                if (!Valid()) return 0;
                int found = 0;
                for (uint32_t i = 1; i < m_nchain; i++) {
                    const Sym& s = m_symtab[i];
                    if (!IsDefined(s) || !NameOk(s)) continue;
                    const char* name = m_strtab + s.name;
                    if (!StartsWith(name, prefix)) continue;
                    if (found < max) fn(name);
                    found++;
                }
                return found;
            }

        private:
            static bool IsDefined(const Sym& s) {
                return s.shndx != ShnUndef && s.shndx != ShnCommon && s.value != 0;
            }

            bool NameOk(const Sym& s) const {
                return m_strsz == 0 || s.name < m_strsz;
            }

            uintptr_t       m_base = 0;
            const uint32_t* m_bucket = nullptr;
            const uint32_t* m_chain = nullptr;
            uint32_t        m_nbucket = 0;
            uint32_t        m_nchain = 0;
            const char*     m_strtab = nullptr;
            const Sym*      m_symtab = nullptr;
            uint64_t        m_strsz = 0;
    };
}

namespace albw::net {

    /* Parses a dotted IPv4 address ("192.168.1.20") into network byte order.
       Done here instead of with nn::socket::InetAton so that's one less SDK
       function the module depends on. Returns false for anything malformed. */
    inline bool ParseIpv4(const char* s, uint32_t* out_be) {
        if (s == nullptr) return false;
        uint8_t parts[4];
        for (int p = 0; p < 4; p++) {
            if (*s < '0' || *s > '9') return false;
            uint32_t v = 0;
            int digits = 0;
            while (*s >= '0' && *s <= '9') {
                v = v * 10 + static_cast<uint32_t>(*s - '0');
                if (++digits > 3 || v > 255) return false;
                s++;
            }
            parts[p] = static_cast<uint8_t>(v);
            if (p < 3) {
                if (*s != '.') return false;
                s++;
            }
        }
        if (*s != '\0') return false;
        /* Network byte order is the bytes in written order; on the little-endian
           Switch that's this value. */
        *out_be = static_cast<uint32_t>(parts[0]) | (static_cast<uint32_t>(parts[1]) << 8) |
                  (static_cast<uint32_t>(parts[2]) << 16) | (static_cast<uint32_t>(parts[3]) << 24);
        return true;
    }

    /* Host-to-network order for a port (the Switch is little-endian). */
    constexpr uint16_t Htons(uint16_t v) {
        return static_cast<uint16_t>((v << 8) | (v >> 8));
    }
}
