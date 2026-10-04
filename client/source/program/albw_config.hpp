#pragma once

/*
 * A Link Between Wilds - client settings.
 *
 * For now these are compiled in. Edit, rebuild, redeploy. A later step is to read
 * them from a file on the SD card / Ryujinx's sdcard folder so players don't need
 * to build the mod themselves.
 */

#include "albw_protocol.h"

namespace albw::config {

    /* IPv4 address of the machine running server/albw_server.py.
       When the server runs on the same PC as Ryujinx, 127.0.0.1 works. */
    constexpr const char ServerIp[] = "127.0.0.1";

    /* Must match the server's --port. */
    constexpr unsigned short ServerPort = ALBW_DEFAULT_PORT;

    /* Shown to other players. Up to 16 bytes. */
    constexpr const char PlayerName[] = "Link";

    /* How often to send our state, in packets per second. */
    constexpr int SendRateHz = 20;

    /* While game offsets aren't filled in yet (see game_offsets.hpp), send a fake
       player walking in a circle so the network path can be tested end to end. */
    constexpr bool SendTestPatternWithoutOffsets = true;
}
