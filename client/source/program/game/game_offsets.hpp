#pragma once

#include <common.hpp>

/*
 * Offsets into BotW's main executable, Switch version 1.6.0.
 *
 * All values are offsets from the start of the main module (what Ghidra shows
 * as the address minus the image base), NOT raw addresses from Cheat Engine,
 * which change every launch. docs/finding-offsets.md walks through finding them.
 *
 * Anything left at 0 is "not found yet" and the code that needs it stays off.
 */
namespace albw::game::offsets {

    /* A function that runs once per frame with the player object in x0, e.g. the
       player actor's update/calc function. Hooked inline (we only read
       registers, the game's code runs untouched). */
    constexpr uintptr_t PlayerUpdate = 0;

    /* Field offsets inside the player object passed in x0 above. */
    /* Separate offsets on purpose: BotW actors keep their transform in a 3x4
       matrix, where X/Y/Z are the last column (not next to each other). */
    constexpr uintptr_t PlayerPosX = 0;   /* float */
    constexpr uintptr_t PlayerPosY = 0;   /* float */
    constexpr uintptr_t PlayerPosZ = 0;   /* float */
    constexpr uintptr_t PlayerRotY = 0;   /* float, radians; optional for now */

    inline constexpr bool HavePlayerHook() { return PlayerUpdate != 0; }
    inline constexpr bool HavePlayerFields() { return PlayerPosX != 0; }
}
