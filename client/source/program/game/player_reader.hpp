#pragma once

#include "program/game/player_math.hpp"

namespace albw::game {

    /*
     * Reads the local player's transform by following the PlayerInfo pointer
     * chain in game_offsets.hpp. Safe to call from any thread at any time:
     * every pointer is checked against the process's memory map before it is
     * dereferenced, and the result is sanity-checked (SampleFromMtx34).
     *
     * Returns false when there is no usable player: title screen, loading,
     * or the chain being wrong. A pointer to unmapped memory is caught before
     * it's read; a pointer into reused-but-mapped memory gives garbage, which
     * the sanity checks are there to reject.
     */
    bool ReadPlayer(PlayerSample& out);

    /* For verification logs: PlayerInfo::mPlayerPos, or false if unreadable. */
    bool ReadPlayerInfoPos(float out[3]);
}
