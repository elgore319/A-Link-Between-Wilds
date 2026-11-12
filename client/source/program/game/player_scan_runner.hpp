#pragma once

/*
 * A Link Between Wilds - running the player scan (player_scan.hpp) in the game.
 *
 * Starts a low-priority thread that scans every few seconds and logs progress
 * as "[albw] scan ..." lines. When a chain qualifies it logs one
 * "[albw] scan RESULT ..." line with the values for game_offsets.hpp, then
 * stops. Read-only: it never writes game memory.
 */

namespace albw::game {

    /* Call once from exl_main, after DetectGameVersion(). */
    void StartPlayerScan();
}
