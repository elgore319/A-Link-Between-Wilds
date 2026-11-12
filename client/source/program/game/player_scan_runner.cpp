#include "player_scan_runner.hpp"

#include "lib.hpp"
#include <cstdio>
#include "lib/util/sys/modules.hpp"
#include <nn/os.hpp>
#include "program/loggers.hpp"
#include "program/game/player_scan.hpp"

namespace albw::game {

    namespace {
        using namespace scan;

        constexpr s64 FirstDelayMs   = 20000;   /* let the game boot before the first pass */
        constexpr s64 IntervalMs     = 4000;
        constexpr int IdleLogEvery   = 8;       /* rounds between "nothing yet" lines */
        constexpr size_t MaxHits     = 32;
        /* Writable memory after main's .data that may still be its .bss. */
        constexpr size_t MaxExtraData = 0x4000000;

        constexpr size_t ThreadStackSize = 0x4000;
        alignas(0x1000) u8 s_thread_stack[ThreadStackSize];
        nn::os::ThreadType s_thread;

        void Sleep(s64 ms) { nn::os::SleepThread(nn::TimeSpan::FromMilliSeconds(ms)); }

        /*
         * Reads from our own process, refusing anything unmapped or unreadable.
         * Remembers the last few regions svcQueryMemory reported, since nearly
         * all reads land in a handful of large heap regions.
         */
        class LiveMem {
            public:
                bool Read(uintptr_t addr, void* out, size_t size) {
                    if (size == 0 || addr + size < addr || !Readable(addr, size))
                        return false;
                    __builtin_memcpy(out, reinterpret_cast<const void*>(addr), size);
                    return true;
                }

            private:
                struct Region { uintptr_t start, end; bool ok; };
                static constexpr int CacheSize = 16;
                Region m_cache[CacheSize] = {};
                int m_next = 0;

                const Region* Lookup(uintptr_t addr) {
                    for (const Region& r : m_cache) {
                        if (r.start <= addr && addr < r.end) return &r;
                    }
                    MemoryInfo info = {};
                    u32 page_info = 0;
                    if (svcQueryMemory(&info, &page_info, addr) != 0 || info.addr + info.size <= addr)
                        return nullptr;
                    bool ok = (info.perm & Perm_R) != 0 && info.type != MemType_Unmapped &&
                              info.type != MemType_Io && info.type != MemType_Reserved;
                    Region& slot = m_cache[m_next];
                    m_next = (m_next + 1) % CacheSize;
                    slot = { info.addr, info.addr + info.size, ok };
                    return &slot;
                }

                bool Readable(uintptr_t addr, size_t size) {
                    uintptr_t cur = addr, end = addr + size;
                    while (cur < end) {
                        const Region* r = Lookup(cur);
                        if (!r || !r->ok) return false;
                        cur = r->end;
                    }
                    return true;
                }
        };

        LiveMem s_mem;
        Tracker s_tracker;
        Hit s_hits[MaxHits];

        /* main's .data, plus any writable regions right after it (.bss can be mapped separately). */
        void GetSlotRange(uintptr_t& begin, uintptr_t& end) {
            const auto& data = exl::util::GetMainModuleInfo().m_Data;
            begin = data.m_Start;
            end = data.GetEnd();
            while (end - data.GetEnd() < MaxExtraData) {
                MemoryInfo info = {};
                u32 page_info = 0;
                if (svcQueryMemory(&info, &page_info, end) != 0 || info.addr != end)
                    break;
                if (info.perm != Perm_Rw || (info.type != MemType_CodeMutable && info.type != MemType_ModuleCodeMutable))
                    break;
                end = info.addr + info.size;
            }
        }

        const char* KindName(Kind k) {
            switch (k) {
                case Kind::ExactWithCopy: return "exact+copy";
                case Kind::Exact:         return "exact";
                case Kind::Shifted:       return "shifted";
            }
            return "?";
        }

        /* Positions are logged as whole units; that's plenty to see Link move. */
        int Round1(float v) { return (int)(v < 0 ? v - 0.5f : v + 0.5f); }

        void ThreadMain(void*) {
            const uintptr_t main_start = exl::util::GetMainModuleInfo().m_Total.m_Start;
            uintptr_t begin, end;
            GetSlotRange(begin, end);
            Logging.Log("[albw] scan: will search main+0x%lx..main+0x%lx for the player. Load your save, then run in a straight line for 30 seconds.",
                        begin - main_start, end - main_start);

            Sleep(FirstDelayMs);
            while (true) {
                RoundStats stats;
                size_t n = ScanRound(s_mem, main_start, begin, end, s_hits, MaxHits, stats);
                s_tracker.AddRound(s_hits, n);

                Tracker::Entry best;
                if (s_tracker.Best(best)) {
                    const Chain& c = best.chain;
                    char copy[16] = "none";
                    if (c.copy_field >= 0)
                        std::snprintf(copy, sizeof(copy), "0x%x", c.copy_field);
                    Logging.Log("[albw] scan RESULT: PlayerInfoInstance=0x%x PlayerInfoPlayerActor=0x%x ActorMtx=0x%x PlayerInfoPlayerPos=%s kind=%s travel=%d rounds=%d candidates=%d",
                                c.slot, c.actor_field, c.mtx_field, copy, KindName(c.kind), Round1(best.travel), best.rounds,
                                (int)s_tracker.QualifyingCount());
                    Logging.Log("[albw] scan done. Send the RESULT line (or the report test-m1 wrote) to Claude.");
                    return;
                }

                Tracker::Entry lead;
                if (n > 0 && s_tracker.Leader(lead)) {
                    Logging.Log("[albw] scan round %d: %d chain(s), best main+0x%x +0x%x +0x%x (%s) at %d,%d,%d, moved %d of %d needed",
                                s_tracker.Round(), (int)stats.hits, lead.chain.slot, lead.chain.actor_field, lead.chain.mtx_field,
                                KindName(lead.chain.kind), Round1(lead.last.x), Round1(lead.last.y), Round1(lead.last.z),
                                Round1(lead.travel), (int)Tracker::MinTravel);
                } else if (s_tracker.Round() % IdleLogEvery == 1) {
                    Logging.Log("[albw] scan round %d: nothing yet (%d slots, %d pointers). Are you in the world, past the title screen?",
                                s_tracker.Round(), (int)stats.slots, (int)stats.pointers);
                }
                Sleep(IntervalMs);
            }
        }
    }

    void StartPlayerScan() {
        /* Priority 24 is below the network thread (16), so a long pass never delays packets. */
        Result rc = nn::os::CreateThread(&s_thread, ThreadMain, nullptr, s_thread_stack, ThreadStackSize, 24, 2);
        if (rc != 0) {
            Logging.Log("[albw] scan: CreateThread failed: 0x%x", rc);
            return;
        }
        nn::os::StartThread(&s_thread);
    }
}
