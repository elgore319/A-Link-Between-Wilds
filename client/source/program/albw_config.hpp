#pragma once

/*
 * A Link Between Wilds - client settings.
 *
 * Server address, port and player name are read at startup from
 * sd:/albw/config.ini (see docs/configuration.md). The values here are the
 * built-in defaults, used when that file is missing or a line in it is invalid.
 * Everything else in this file is compiled in.
 */

#include "albw_protocol.h"

namespace albw::config {

    /* ---- Defaults, overridable from config.ini ------------------------------ */

    /* IPv4 address of the machine running server/albw_server.py.
       When the server runs on the same PC as Ryujinx, 127.0.0.1 works.
       config.ini key: server_ip */
    constexpr const char ServerIp[] = "127.0.0.1";

    /* Must match the server's --port. config.ini key: server_port */
    constexpr unsigned short ServerPort = ALBW_DEFAULT_PORT;

    /* Shown to other players. Up to 16 bytes. config.ini key: player_name */
    constexpr const char PlayerName[] = "Link";

    /* ---- Build-time only ----------------------------------------------------- */

    /* Read sd:/albw/config.ini at startup. Set to false to build a module that
       never touches the filesystem (it then also doesn't link against nn::fs),
       e.g. if MountSdCardForDebug turns out to be missing or blocked. */
    constexpr bool UseConfigFile = true;

    /* How often to send our state, in packets per second. */
    constexpr int SendRateHz = 20;

    /* Read the player's position from game memory via the PlayerInfo pointer
       chain (game_offsets.hpp, decision 0007). Set to false to go back to the
       test pattern if the chain turns out to be wrong. */
    constexpr bool ReadPlayerFromMemory = true;

    /* While game offsets aren't filled in yet (see game_offsets.hpp), send a fake
       player walking in a circle so the network path can be tested end to end. */
    constexpr bool SendTestPatternWithoutOffsets = true;
}
