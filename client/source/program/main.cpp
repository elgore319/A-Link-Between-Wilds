/*
 * A Link Between Wilds - Switch module entry point.
 *
 * Loaded into Breath of the Wild by exlaunch (as subsdk9). Milestones:
 *   1. Start the network thread and join the server.              <- done
 *   2. Send our real position, read via the PlayerInfo pointer chain. <- awaiting in-game test
 *   3. Draw the other players.                                    <- next
 */
#include "lib.hpp"
#include "program/loggers.hpp"
#include "program/albw_config.hpp"
#include "program/game/game_offsets.hpp"
#include "program/net/client.hpp"

namespace {
    u32 s_frame = 0;

    template<typename T>
    T ReadField(uintptr_t base, uintptr_t offset) {
        T value;
        __builtin_memcpy(&value, reinterpret_cast<const void*>(base + offset), sizeof(T));
        return value;
    }
}

/*
 * Inline hook: runs just before the hooked instruction, sees the CPU registers,
 * and returns. The game's own code is untouched, which makes this the safest
 * kind of hook to start with: a wrong offset can't corrupt the call itself.
 */
HOOK_DEFINE_INLINE(PlayerUpdateHook) {
    static void Callback(exl::hook::InlineCtx* ctx) {
        uintptr_t player = ctx->X[0];
        s_frame++;
        if (player == 0)
            return;

        if constexpr (albw::game::offsets::HavePlayerFields()) {
            using namespace albw::game::offsets;
            float pos[3] = {
                ReadField<float>(player, PlayerPosX),
                ReadField<float>(player, PlayerPosY),
                ReadField<float>(player, PlayerPosZ),
            };
            float rot = PlayerRotY != 0 ? ReadField<float>(player, PlayerRotY) : 0.0f;
            albw::net::GetClient().SetLocalState(pos, rot, s_frame);
        } else if (s_frame % 60 == 0) {
            /* Hook found but fields not yet: log the object pointer so you can
               inspect it in a memory viewer and find the position floats. */
            Logging.Log("[albw] player object @ 0x%lx (frame %u)", player, s_frame);
        }
    }
};

extern "C" void exl_main(void* x0, void* x1) {
    (void)x0; (void)x1;
    exl::hook::Initialize();
    Logging.Log("[albw] A Link Between Wilds loaded");

    auto& client = albw::net::GetClient();

    /* Where our player's state comes from, in order of preference (decision 0007):
       1. reading the PlayerInfo pointer chain from the network thread,
       2. an inline hook on a per-frame player function (kept for later, e.g. if
          reads need to be synchronized with the game thread),
       3. a generated test pattern. */
    if constexpr (albw::config::ReadPlayerFromMemory && albw::game::offsets::HavePlayerPointerChain()) {
        Logging.Log("[albw] reading player from PlayerInfo (main+0x%lx)", albw::game::offsets::PlayerInfoInstance);
        client.EnablePlayerReader();
    } else if constexpr (albw::game::offsets::HavePlayerHook()) {
        PlayerUpdateHook::InstallAtOffset(albw::game::offsets::PlayerUpdate);
        Logging.Log("[albw] player hook installed at main+0x%lx", albw::game::offsets::PlayerUpdate);
    } else if constexpr (albw::config::SendTestPatternWithoutOffsets) {
        Logging.Log("[albw] no player offsets yet, sending test pattern");
        client.EnableTestPattern();
    }

    client.Start();
}

extern "C" NORETURN void exl_exception_entry() {
    /* Only used for applets/sysmodules, not games. */
    EXL_ABORT("Default exception handler called!");
}
