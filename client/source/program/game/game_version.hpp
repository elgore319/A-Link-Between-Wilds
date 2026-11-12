#pragma once

/*
 * A Link Between Wilds - checking the game build before touching its memory.
 *
 * Offsets in game_offsets.hpp are for BotW Switch 1.0.0 only (decision 0009).
 * On any other build they point at unrelated code and data, so anything that
 * hooks or reads the game must check IsSupportedGame() first. Networking does
 * not depend on the game build and runs either way.
 */

namespace albw::game {

    enum class GameVersion {
        Unknown,      /* no build ID found, or one we don't recognise */
        V1_0_0,       /* supported */
        V1_6_0,       /* known, not supported: the update is installed */
    };

    /* Reads the main executable's build ID and logs the result. Call once from exl_main. */
    GameVersion DetectGameVersion();

    /* Result of DetectGameVersion(); Unknown before it has run. */
    GameVersion GetGameVersion();

    inline bool IsSupportedGame() { return GetGameVersion() == GameVersion::V1_0_0; }
}
