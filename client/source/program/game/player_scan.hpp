#pragma once

/*
 * A Link Between Wilds - finding the player in memory on a build nobody has mapped.
 *
 * On 1.5.0/1.6.0 the player's transform is reached through a global pointer:
 *
 *     [main + sInstance] -> PlayerInfo
 *     [PlayerInfo + 0x60] -> player Actor            (mPlayerActor)
 *     Actor + 0x398       -> sead::Matrix34f          (mMtx, translation in column 3)
 *     PlayerInfo + 0x88   -> Vector3f copy of the same position (mPlayerPos)
 *
 * On 1.0.0 the global's address is unknown and the field offsets are only
 * probably the same (docs/research/offsets-log.md). This scanner walks every
 * 8-byte slot of the main module's writable data and looks for that shape:
 *
 *   1. Exact: the 1.6.0 field offsets, with or without the +0x88 copy agreeing.
 *   2. Shifted (only when 1 finds nothing): actor pointer anywhere in
 *      0x40..0x80, matrix anywhere in 0x300..0x480, and a position copy
 *      somewhere in the first 0x200 bytes of the PlayerInfo-like object. The
 *      copy is required here, or every manager pointing at any actor matches.
 *
 * One scan can't tell Link from an NPC, so Tracker remembers each chain over
 * several scans and only reports one that moved a long way: the player runs
 * in a straight line, NPCs mostly don't.
 *
 * No Switch/SDK dependencies: memory goes through a `Mem` type with
 * `bool Read(uintptr_t addr, void* out, size_t size)` that returns false for
 * unmapped memory, so the same code runs against a fake address space in
 * client/tests/test_player_scan.cpp. The Switch side is player_scan.cpp.
 */

#include <stddef.h>
#include <stdint.h>

namespace albw::game::scan {

    struct Vec3 { float x, y, z; };

    /* How a chain was found. Lower is more trustworthy. */
    enum class Kind : uint8_t {
        ExactWithCopy = 0,  /* 1.6.0 offsets, PlayerInfo's +0x88 agrees */
        Exact = 1,          /* 1.6.0 offsets, no agreeing copy */
        Shifted = 2,        /* other offsets, with an agreeing copy */
    };

    /* A pointer chain to a transform: main + slot -> obj; obj + actor_field -> actor; actor + mtx_field. */
    struct Chain {
        uint32_t slot;          /* offset of the global from the main module's start */
        uint16_t actor_field;   /* PlayerInfo::mPlayerActor candidate */
        uint16_t mtx_field;     /* Actor::mMtx candidate */
        int16_t  copy_field;    /* PlayerInfo::mPlayerPos candidate, -1 if none */
        Kind     kind;

        bool SameAs(const Chain& o) const {
            return slot == o.slot && actor_field == o.actor_field && mtx_field == o.mtx_field;
        }
    };

    struct Hit {
        Chain chain;
        Vec3  pos;
    };

    /* 1.6.0's layout, tried first. */
    constexpr uint16_t ExactActorField = 0x60;
    constexpr uint16_t ExactMtxField   = 0x398;
    constexpr int16_t  ExactCopyField  = 0x88;

    /* Search window for the shifted pass. */
    constexpr uint16_t ShiftActorMin = 0x40, ShiftActorMax = 0x80;    /* step 8 */
    constexpr uint16_t ShiftMtxMin   = 0x300, ShiftMtxMax  = 0x480;   /* step 4 */
    constexpr int16_t  ShiftCopyMax  = 0x200;                          /* step 4 */

    /* Hyrule is about 12000 x 10000 units around the origin. */
    constexpr float MaxAbsCoord = 20000.0f;
    /* How close a position copy must be to the matrix's translation. */
    constexpr float CopyTolerance = 5.0f;

    inline float Abs(float v) { return v < 0 ? -v : v; }
    inline bool IsFinite(float v) { return v == v && (v - v) == 0.0f; }

    /*
     * True if m (3 rows x 4 floats, row-major) looks like a real, unscaled actor
     * transform: finite, rotation rows unit length and at right angles, a
     * translation inside the map and not exactly the origin.
     */
    inline bool LooksLikeTransform(const float m[12]) {
        for (int i = 0; i < 12; i++) {
            if (!IsFinite(m[i])) return false;
        }
        float x = m[3], y = m[7], z = m[11];
        if (Abs(x) > MaxAbsCoord || Abs(y) > MaxAbsCoord || Abs(z) > MaxAbsCoord)
            return false;
        if (x == 0.0f && y == 0.0f && z == 0.0f)
            return false;
        for (int r = 0; r < 3; r++) {
            const float* a = m + r * 4;
            float len2 = a[0] * a[0] + a[1] * a[1] + a[2] * a[2];
            if (len2 < 0.9f || len2 > 1.1f) return false;
            for (int s = r + 1; s < 3; s++) {
                const float* b = m + s * 4;
                if (Abs(a[0] * b[0] + a[1] * b[1] + a[2] * b[2]) > 0.1f) return false;
            }
        }
        return true;
    }

    inline bool Near(const Vec3& a, const Vec3& b, float tol) {
        return Abs(a.x - b.x) <= tol && Abs(a.y - b.y) <= tol && Abs(a.z - b.z) <= tol;
    }

    template<typename Mem>
    bool ReadPtr(Mem& mem, uintptr_t addr, uintptr_t& out) {
        if ((addr & 7) != 0 || !mem.Read(addr, &out, sizeof(out)))
            return false;
        /* User-space pointers on Switch fit in 39 bits and objects are 8-aligned. */
        return out != 0 && (out & 7) == 0 && out < (uintptr_t(1) << 39);
    }

    template<typename Mem>
    bool ReadTransform(Mem& mem, uintptr_t addr, Vec3& pos) {
        float m[12];
        if (!mem.Read(addr, m, sizeof(m)) || !LooksLikeTransform(m))
            return false;
        pos = { m[3], m[7], m[11] };
        return true;
    }

    template<typename Mem>
    bool ReadVec3(Mem& mem, uintptr_t addr, Vec3& v) {
        float f[3];
        if (!mem.Read(addr, f, sizeof(f)))
            return false;
        v = { f[0], f[1], f[2] };
        return true;
    }

    /* Finds a copy of `pos` among the first ShiftCopyMax bytes of obj, or returns -1. */
    template<typename Mem>
    int16_t FindCopy(Mem& mem, uintptr_t obj, const Vec3& pos) {
        float f[ShiftCopyMax / 4 + 2];
        if (!mem.Read(obj, f, sizeof(f)))
            return -1;
        for (int i = 0; i + 2 < (int)(sizeof(f) / 4); i++) {
            if (Near({ f[i], f[i + 1], f[i + 2] }, pos, CopyTolerance))
                return (int16_t)(i * 4);
        }
        return -1;
    }

    struct RoundStats {
        size_t slots = 0;      /* 8-byte slots looked at */
        size_t pointers = 0;   /* slots holding a readable pointer */
        size_t hits = 0;       /* chains found, including ones that didn't fit in `out` */
    };

    /*
     * One pass over [slots_begin, slots_end). Writes up to max_out hits and
     * returns how many were written. Shifted chains are only searched for when
     * no exact chain turns up.
     */
    template<typename Mem>
    size_t ScanRound(Mem& mem, uintptr_t module_start, uintptr_t slots_begin, uintptr_t slots_end,
                     Hit* out, size_t max_out, RoundStats& stats) {
        stats = {};
        size_t n = 0;
        auto emit = [&](const Hit& h) {
            stats.hits++;
            if (n < max_out) out[n++] = h;
        };

        for (int pass = 0; pass < 2; pass++) {
            if (pass == 1 && stats.hits != 0)
                break;
            for (uintptr_t slot = (slots_begin + 7) & ~uintptr_t(7); slot + 8 <= slots_end; slot += 8) {
                if (pass == 0) stats.slots++;
                uintptr_t obj;
                if (!ReadPtr(mem, slot, obj))
                    continue;
                if (pass == 0) stats.pointers++;
                uint32_t slot_off = (uint32_t)(slot - module_start);

                if (pass == 0) {
                    uintptr_t actor;
                    Vec3 pos;
                    if (!ReadPtr(mem, obj + ExactActorField, actor) || !ReadTransform(mem, actor + ExactMtxField, pos))
                        continue;
                    Vec3 copy;
                    bool has_copy = ReadVec3(mem, obj + ExactCopyField, copy) && Near(copy, pos, CopyTolerance);
                    emit({ { slot_off, ExactActorField, ExactMtxField, has_copy ? ExactCopyField : (int16_t)-1,
                             has_copy ? Kind::ExactWithCopy : Kind::Exact }, pos });
                    continue;
                }

                for (uint16_t af = ShiftActorMin; af <= ShiftActorMax; af += 8) {
                    uintptr_t actor;
                    if (!ReadPtr(mem, obj + af, actor))
                        continue;
                    for (uint16_t mf = ShiftMtxMin; mf <= ShiftMtxMax; mf += 4) {
                        Vec3 pos;
                        if (!ReadTransform(mem, actor + mf, pos))
                            continue;
                        int16_t cf = FindCopy(mem, obj, pos);
                        if (cf < 0)
                            continue;
                        emit({ { slot_off, af, mf, cf, Kind::Shifted }, pos });
                    }
                }
            }
        }
        return n;
    }

    /*
     * Remembers chains across scans and decides when one is the player.
     * A chain counts once it was found in at least MinRounds scans, was found
     * in the latest one, and its position has moved at least MinTravel units
     * from where it was first seen.
     */
    class Tracker {
        public:
            static constexpr size_t Capacity  = 64;
            static constexpr int    MinRounds = 3;
            static constexpr float  MinTravel = 50.0f;

            struct Entry {
                Chain chain;
                Vec3  first;
                Vec3  last;
                float travel;      /* furthest it has been from `first` (largest per-axis difference) */
                int   rounds;      /* scans it was found in */
                int   last_round;
            };

            void AddRound(const Hit* hits, size_t count) {
                m_round++;
                for (size_t i = 0; i < count; i++) {
                    Entry* e = Find(hits[i].chain);
                    if (!e) {
                        e = Insert(hits[i]);
                        if (!e) { m_dropped++; continue; }
                    }
                    if (e->last_round == m_round)
                        continue;   /* same chain twice in one scan (shifted pass) */
                    /* Keep the most trustworthy kind seen: the +0x88 copy can lag a frame. */
                    if ((uint8_t)hits[i].chain.kind < (uint8_t)e->chain.kind)
                        e->chain = hits[i].chain;
                    e->last = hits[i].pos;
                    e->rounds++;
                    e->last_round = m_round;
                    float d = Distance(e->first, e->last);
                    if (d > e->travel) e->travel = d;
                }
            }

            /*
             * The best chain that qualifies: most trustworthy kind first, then
             * the one that travelled furthest. False if none qualifies yet.
             */
            bool Best(Entry& out) const {
                const Entry* best = nullptr;
                for (size_t i = 0; i < m_count; i++) {
                    const Entry& e = m_entries[i];
                    if (!Qualifies(e)) continue;
                    if (!best || (uint8_t)e.chain.kind < (uint8_t)best->chain.kind ||
                        (e.chain.kind == best->chain.kind && e.travel > best->travel))
                        best = &e;
                }
                if (!best) return false;
                out = *best;
                return true;
            }

            /* The entry that has moved furthest so far, qualified or not (for progress logs). */
            bool Leader(Entry& out) const {
                const Entry* best = nullptr;
                for (size_t i = 0; i < m_count; i++) {
                    if (m_entries[i].last_round != m_round) continue;
                    if (!best || m_entries[i].travel > best->travel) best = &m_entries[i];
                }
                if (!best) return false;
                out = *best;
                return true;
            }

            /* How many tracked chains qualify. More than one is fine when they all
               lead to Link (e.g. a camera that also follows him). */
            size_t QualifyingCount() const {
                size_t n = 0;
                for (size_t i = 0; i < m_count; i++) n += Qualifies(m_entries[i]) ? 1 : 0;
                return n;
            }

            size_t Count() const { return m_count; }
            size_t Dropped() const { return m_dropped; }
            int    Round() const { return m_round; }

        private:
            static float Distance(const Vec3& a, const Vec3& b) {
                float dx = Abs(a.x - b.x), dy = Abs(a.y - b.y), dz = Abs(a.z - b.z);
                float m = dx > dy ? dx : dy;
                return m > dz ? m : dz;
            }

            bool Qualifies(const Entry& e) const {
                return e.rounds >= MinRounds && e.last_round == m_round && e.travel >= MinTravel;
            }

            Entry* Find(const Chain& c) {
                for (size_t i = 0; i < m_count; i++) {
                    if (m_entries[i].chain.SameAs(c)) return &m_entries[i];
                }
                return nullptr;
            }

            Entry* Insert(const Hit& h) {
                if (m_count == Capacity) {
                    /* Reuse the stalest entry that isn't from this round. */
                    size_t victim = Capacity;
                    for (size_t i = 0; i < m_count; i++) {
                        if (m_entries[i].last_round == m_round) continue;
                        if (victim == Capacity || m_entries[i].last_round < m_entries[victim].last_round)
                            victim = i;
                    }
                    if (victim == Capacity) return nullptr;
                    m_entries[victim] = { h.chain, h.pos, h.pos, 0.0f, 0, 0 };
                    return &m_entries[victim];
                }
                m_entries[m_count] = { h.chain, h.pos, h.pos, 0.0f, 0, 0 };
                return &m_entries[m_count++];
            }

            Entry  m_entries[Capacity] = {};
            size_t m_count = 0;
            size_t m_dropped = 0;
            int    m_round = 0;
    };
}
