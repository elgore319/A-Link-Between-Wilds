#include "player_reader.hpp"

#include "lib.hpp"
#include "lib/util/modules.hpp"
#include "program/game/game_offsets.hpp"

namespace albw::game {

    namespace {
        /* True if [addr, addr + size) is mapped and readable in our process. */
        bool IsReadable(uintptr_t addr, size_t size) {
            if (addr == 0 || addr + size < addr)
                return false;
            uintptr_t cur = addr;
            const uintptr_t end = addr + size;
            /* A range can span two regions (e.g. adjacent heap blocks); check each. */
            while (cur < end) {
                MemoryInfo info = {};
                u32 page_info = 0;
                if (svcQueryMemory(&info, &page_info, cur) != 0)
                    return false;
                if (info.type == MemType_Unmapped || (info.perm & Perm_R) == 0)
                    return false;
                uintptr_t region_end = info.addr + info.size;
                if (region_end <= cur)
                    return false;
                cur = region_end;
            }
            return true;
        }

        bool ReadPtr(uintptr_t addr, uintptr_t& out) {
            if ((addr & 7) != 0 || !IsReadable(addr, sizeof(uintptr_t)))
                return false;
            __builtin_memcpy(&out, reinterpret_cast<const void*>(addr), sizeof(out));
            return out != 0;
        }

        /* Follows main -> PlayerInfo. */
        bool GetPlayerInfo(uintptr_t& info) {
            using namespace offsets;
            if constexpr (!HavePlayerPointerChain()) {
                return false;
            } else {
                return ReadPtr(exl::util::modules::GetTargetOffset(PlayerInfoInstance), info);
            }
        }
    }

    bool ReadPlayer(PlayerSample& out) {
        using namespace offsets;
        uintptr_t info, actor;
        if (!GetPlayerInfo(info) || !ReadPtr(info + PlayerInfoPlayerActor, actor))
            return false;

        const uintptr_t mtx_addr = actor + ActorMtx;
        if ((mtx_addr & 3) != 0 || !IsReadable(mtx_addr, sizeof(float) * 12))
            return false;
        float mtx[12];
        __builtin_memcpy(mtx, reinterpret_cast<const void*>(mtx_addr), sizeof(mtx));
        return SampleFromMtx34(mtx, out);
    }

    bool ReadPlayerInfoPos(float out[3]) {
        using namespace offsets;
        uintptr_t info;
        if (!GetPlayerInfo(info))
            return false;
        const uintptr_t pos_addr = info + PlayerInfoPlayerPos;
        if (!IsReadable(pos_addr, sizeof(float) * 3))
            return false;
        __builtin_memcpy(out, reinterpret_cast<const void*>(pos_addr), sizeof(float) * 3);
        return true;
    }
}
