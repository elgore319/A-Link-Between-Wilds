/*
 * Host-side tests for client/source/program/game/player_scan.hpp, against a
 * fake address space with a planted player and decoys.
 *
 *     g++ -std=c++20 -Wall -Wextra -Werror -I../source test_player_scan.cpp -o test_player_scan && ./test_player_scan
 *
 * (CI runs this; see the "client-tests" job in .github/workflows/ci.yml.)
 */
#include "program/game/player_scan.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace albw::game::scan;

namespace {
    int g_failed = 0;
    int g_checks = 0;

    #define CHECK(cond) do { \
        g_checks++; \
        if (!(cond)) { g_failed++; std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } \
    } while (0)

    /* A few mapped regions; everything else is unmapped. */
    struct FakeMem {
        struct Region { uintptr_t base; std::vector<uint8_t> bytes; };
        std::vector<Region> regions;
        size_t reads = 0;

        Region& Map(uintptr_t base, size_t size) {
            regions.push_back({ base, std::vector<uint8_t>(size, 0) });
            return regions.back();
        }

        uint8_t* At(uintptr_t addr, size_t size) {
            for (auto& r : regions) {
                if (addr >= r.base && addr + size <= r.base + r.bytes.size())
                    return r.bytes.data() + (addr - r.base);
            }
            return nullptr;
        }

        bool Read(uintptr_t addr, void* out, size_t size) {
            reads++;
            uint8_t* p = At(addr, size);
            if (!p) return false;
            std::memcpy(out, p, size);
            return true;
        }

        void Put64(uintptr_t addr, uint64_t v) { std::memcpy(At(addr, 8), &v, 8); }
        void PutVec3(uintptr_t addr, Vec3 v) { float f[3] = { v.x, v.y, v.z }; std::memcpy(At(addr, 12), f, 12); }

        /* A transform rotated by `yaw` about Y, at `pos`. */
        void PutMtx(uintptr_t addr, float yaw, Vec3 pos, float scale = 1.0f) {
            float c = std::cos(yaw) * scale, s = std::sin(yaw) * scale;
            float m[12] = { c, 0, s, pos.x,
                            0, scale, 0, pos.y,
                            -s, 0, c, pos.z };
            std::memcpy(At(addr, sizeof(m)), m, sizeof(m));
        }
    };

    constexpr uintptr_t Main     = 0x8000000;
    constexpr uintptr_t Data     = Main + 0x2000000;
    constexpr size_t    DataSize = 0x4000;
    constexpr uintptr_t Heap     = 0x40000000;

    /* Main's data region plus a heap region. */
    FakeMem World() {
        FakeMem m;
        m.Map(Data, DataSize);
        m.Map(Heap, 0x100000);
        return m;
    }

    /* Plants PlayerInfo at `info` -> actor at `actor`, with the given layout. */
    void PlantPlayer(FakeMem& m, uintptr_t slot, uintptr_t info, uintptr_t actor, Vec3 pos,
                     uint16_t af = ExactActorField, uint16_t mf = ExactMtxField, int16_t cf = ExactCopyField) {
        m.Put64(slot, info);
        m.Put64(info + af, actor);
        m.PutMtx(actor + mf, 0.7f, pos);
        if (cf >= 0) m.PutVec3(info + cf, pos);
    }

    size_t Scan(FakeMem& m, Hit* hits, size_t max, RoundStats& st) {
        return ScanRound(m, Main, Data, Data + DataSize, hits, max, st);
    }

    void TestTransformCheck() {
        float good[12] = { 1, 0, 0, 100, 0, 1, 0, 50, 0, 0, 1, -200 };
        CHECK(LooksLikeTransform(good));

        float origin[12] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 };
        CHECK(!LooksLikeTransform(origin));

        float far[12] = { 1, 0, 0, 30000, 0, 1, 0, 0, 0, 0, 1, 0 };
        CHECK(!LooksLikeTransform(far));

        float scaled[12] = { 2, 0, 0, 1, 0, 2, 0, 1, 0, 0, 2, 1 };
        CHECK(!LooksLikeTransform(scaled));

        float skewed[12] = { 1, 0, 0, 1, 0.7071f, 0.7071f, 0, 1, 0, 0, 1, 1 };
        CHECK(!LooksLikeTransform(skewed));

        float nan[12] = { 1, 0, 0, NAN, 0, 1, 0, 1, 0, 0, 1, 1 };
        CHECK(!LooksLikeTransform(nan));

        float zeros[12] = {};
        CHECK(!LooksLikeTransform(zeros));
    }

    void TestFindsExactWithCopy() {
        FakeMem m = World();
        PlantPlayer(m, Data + 0x1230, Heap + 0x1000, Heap + 0x8000, { 1234.5f, 210.0f, -3456.25f });
        Hit hits[8];
        RoundStats st;
        size_t n = Scan(m, hits, 8, st);
        CHECK(n == 1);
        CHECK(st.hits == 1);
        CHECK(st.slots == DataSize / 8);
        CHECK(hits[0].chain.slot == 0x2001230);
        CHECK(hits[0].chain.actor_field == 0x60);
        CHECK(hits[0].chain.mtx_field == 0x398);
        CHECK(hits[0].chain.copy_field == 0x88);
        CHECK(hits[0].chain.kind == Kind::ExactWithCopy);
        CHECK(hits[0].pos.x == 1234.5f && hits[0].pos.y == 210.0f && hits[0].pos.z == -3456.25f);
    }

    void TestExactWithoutCopy() {
        FakeMem m = World();
        PlantPlayer(m, Data + 0x10, Heap + 0x1000, Heap + 0x8000, { 10, 20, 30 }, ExactActorField, ExactMtxField, -1);
        Hit hits[8];
        RoundStats st;
        CHECK(Scan(m, hits, 8, st) == 1);
        CHECK(hits[0].chain.kind == Kind::Exact);
        CHECK(hits[0].chain.copy_field == -1);
    }

    void TestDecoysAndBadPointers() {
        FakeMem m = World();
        /* Pointer to unmapped memory. */
        m.Put64(Data + 0x08, 0x70000000);
        /* Misaligned pointer. */
        m.Put64(Data + 0x10, Heap + 0x1003);
        /* Object whose +0x60 points at an actor with a garbage matrix. */
        m.Put64(Data + 0x18, Heap + 0x2000);
        m.Put64(Heap + 0x2000 + 0x60, Heap + 0x9000);
        m.PutMtx(Heap + 0x9000 + 0x398, 0.3f, { 1, 2, 3 }, 3.0f);
        /* Object right at the end of the heap: reading +0x60 runs off the map. */
        m.Put64(Data + 0x20, Heap + 0x100000 - 0x10);
        /* Huge value that isn't a user-space pointer. */
        m.Put64(Data + 0x28, 0xFFFFFF8000000000ull);
        Hit hits[8];
        RoundStats st;
        CHECK(Scan(m, hits, 8, st) == 0);
        CHECK(st.hits == 0);
        CHECK(st.pointers == 3);   /* 0x18, 0x20 and the unmapped one pass the cheap checks */
    }

    void TestShiftedLayout() {
        FakeMem m = World();
        PlantPlayer(m, Data + 0x400, Heap + 0x1000, Heap + 0x8000, { -500, 80, 900 }, 0x68, 0x3A8, 0x90);
        Hit hits[8];
        RoundStats st;
        size_t n = Scan(m, hits, 8, st);
        CHECK(n == 1);
        CHECK(hits[0].chain.kind == Kind::Shifted);
        CHECK(hits[0].chain.slot == 0x2000400);
        CHECK(hits[0].chain.actor_field == 0x68);
        CHECK(hits[0].chain.mtx_field == 0x3A8);
        CHECK(hits[0].chain.copy_field == 0x90);
    }

    void TestShiftedNeedsCopy() {
        FakeMem m = World();
        PlantPlayer(m, Data + 0x400, Heap + 0x1000, Heap + 0x8000, { -500, 80, 900 }, 0x68, 0x3A8, -1);
        Hit hits[8];
        RoundStats st;
        CHECK(Scan(m, hits, 8, st) == 0);
    }

    void TestExactSkipsShiftedPass() {
        FakeMem m = World();
        PlantPlayer(m, Data + 0x100, Heap + 0x1000, Heap + 0x8000, { 1, 2, 3 });
        PlantPlayer(m, Data + 0x200, Heap + 0x3000, Heap + 0xA000, { 4, 5, 6 }, 0x68, 0x3A8, 0x90);
        Hit hits[8];
        RoundStats st;
        CHECK(Scan(m, hits, 8, st) == 1);
        CHECK(hits[0].chain.slot == 0x2000100);
    }

    void TestOutputCap() {
        FakeMem m = World();
        for (int i = 0; i < 5; i++)
            PlantPlayer(m, Data + 0x100 + i * 8, Heap + 0x1000 + i * 0x1000, Heap + 0x8000 + i * 0x1000, { 1.0f + i, 2, 3 });
        Hit hits[3];
        RoundStats st;
        CHECK(Scan(m, hits, 3, st) == 3);
        CHECK(st.hits == 5);
    }

    Hit MakeHit(uint32_t slot, Kind kind, Vec3 pos) {
        return { { slot, ExactActorField, ExactMtxField, kind == Kind::Exact ? (int16_t)-1 : ExactCopyField, kind }, pos };
    }

    void TestTrackerPicksTheRunner() {
        Tracker t;
        Tracker::Entry e;
        for (int r = 0; r < 6; r++) {
            Hit hits[3] = {
                MakeHit(0x100, Kind::ExactWithCopy, { 100.0f + 20 * r, 0, 0 }),   /* Link, running */
                MakeHit(0x200, Kind::Exact, { 50, 0, 50.0f + 1 * r }),            /* NPC, strolling */
                MakeHit(0x300, Kind::Exact, { 7, 7, 7 }),                         /* statue */
            };
            t.AddRound(hits, 3);
            /* 3 rounds and 50 units are needed: Link has moved 20*r after round r+1. */
            bool want = r >= 3;
            CHECK(t.Best(e) == want);
            if (want) {
                CHECK(e.chain.slot == 0x100);
                CHECK(e.rounds == r + 1);
            }
            CHECK(t.Leader(e) && e.chain.slot == 0x100);
        }
        CHECK(t.QualifyingCount() == 1);
    }

    void TestTrackerNeedsLatestRound() {
        Tracker t;
        for (int r = 0; r < 4; r++) {
            Hit h = MakeHit(0x100, Kind::Exact, { 100.0f * r, 0, 0 });
            t.AddRound(&h, 1);
        }
        Tracker::Entry e;
        CHECK(t.Best(e));
        /* Player gone (loading screen): it no longer counts. */
        t.AddRound(nullptr, 0);
        CHECK(!t.Best(e));
        CHECK(!t.Leader(e));
    }

    void TestTrackerPrefersTrustedKind() {
        Tracker t;
        for (int r = 0; r < 4; r++) {
            Hit hits[2] = {
                MakeHit(0x100, Kind::Exact, { 0, 0, 1.0f + 100 * r }),
                MakeHit(0x200, Kind::ExactWithCopy, { 0, 0, 1.0f + 60 * r }),
            };
            t.AddRound(hits, 2);
        }
        Tracker::Entry e;
        CHECK(t.Best(e));
        CHECK(e.chain.slot == 0x200);
        CHECK(t.QualifyingCount() == 2);
    }

    void TestTrackerUpgradesKind() {
        Tracker t;
        for (int r = 0; r < 4; r++) {
            Hit h = MakeHit(0x100, r == 2 ? Kind::ExactWithCopy : Kind::Exact, { 1.0f + 100 * r, 0, 0 });
            t.AddRound(&h, 1);
        }
        Tracker::Entry e;
        CHECK(t.Best(e));
        CHECK(e.chain.kind == Kind::ExactWithCopy);
        CHECK(e.chain.copy_field == ExactCopyField);
    }

    void TestTrackerCapacity() {
        Tracker t;
        /* Fill it with chains that are only seen once. */
        std::vector<Hit> junk;
        for (uint32_t i = 0; i < Tracker::Capacity; i++)
            junk.push_back(MakeHit(0x1000 + i * 8, Kind::Exact, { 1, 1, 1 }));
        t.AddRound(junk.data(), junk.size());
        CHECK(t.Count() == Tracker::Capacity);
        /* The runner still gets tracked, by evicting stale junk. */
        for (int r = 0; r < 4; r++) {
            Hit h = MakeHit(0x100, Kind::Exact, { 1.0f + 100 * r, 0, 0 });
            t.AddRound(&h, 1);
        }
        Tracker::Entry e;
        CHECK(t.Best(e) && e.chain.slot == 0x100);
        CHECK(t.Dropped() == 0);

        /* More new chains in one round than fit: the extras are dropped, not crashed on. */
        Tracker t2;
        std::vector<Hit> many;
        for (uint32_t i = 0; i < Tracker::Capacity + 5; i++)
            many.push_back(MakeHit(0x1000 + i * 8, Kind::Exact, { 1, 1, 1 }));
        t2.AddRound(many.data(), many.size());
        CHECK(t2.Count() == Tracker::Capacity);
        CHECK(t2.Dropped() == 5);
    }

    /* End to end: the scanner plus tracker on a world where Link runs and an NPC walks. */
    void TestEndToEnd() {
        FakeMem m = World();
        Tracker t;
        Tracker::Entry e;
        bool found = false;
        int found_round = -1;
        for (int r = 0; r < 10 && !found; r++) {
            PlantPlayer(m, Data + 0x2A0, Heap + 0x1000, Heap + 0x8000, { -1200.0f + 25 * r, 150, 800 });
            PlantPlayer(m, Data + 0x3B0, Heap + 0x3000, Heap + 0xA000, { 300, 10, 300.0f + 2 * r });
            Hit hits[8];
            RoundStats st;
            size_t n = Scan(m, hits, 8, st);
            CHECK(n == 2);
            t.AddRound(hits, n);
            if (t.Best(e)) { found = true; found_round = r; }
        }
        CHECK(found);
        CHECK(found_round == 2);   /* 3rd scan: 50 units moved */
        CHECK(e.chain.slot == 0x20002A0 && e.chain.kind == Kind::ExactWithCopy);
    }
}

int main() {
    TestTransformCheck();
    TestFindsExactWithCopy();
    TestExactWithoutCopy();
    TestDecoysAndBadPointers();
    TestShiftedLayout();
    TestShiftedNeedsCopy();
    TestExactSkipsShiftedPass();
    TestOutputCap();
    TestTrackerPicksTheRunner();
    TestTrackerNeedsLatestRound();
    TestTrackerPrefersTrustedKind();
    TestTrackerUpgradesKind();
    TestTrackerCapacity();
    TestEndToEnd();
    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
