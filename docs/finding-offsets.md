# Finding game offsets

Goal for milestone 2: fill in [`game_offsets.hpp`](../client/source/program/game/game_offsets.hpp) so the module sends your real position instead of the test pattern.

> **Start here:** [research/decomp-mapping.md](research/decomp-mapping.md) has strong 1.6.0 candidates for the player's position from the community decomp and other 1.6.0 mods, and they may not need a hook at all. Verify those first (§ "How to verify in-game"). The steps below are for when they don't pan out, or for finding things nobody has mapped yet.

**Record everything you find in [`research/offsets-log.md`](research/offsets-log.md)**, including dead ends. That log is how we re-find things after mistakes and how anyone else picks up the work.

## Tools
- Ryujinx with BotW 1.6.0
- Cheat Engine (attach to the Ryujinx process)
- Ghidra with a Switch loader (e.g. the community "Ghidra-Switch-Loader" extension) and BotW's `main` NSO from your own dump
- Existing community research: BotW decomp / modding wikis (ZeldaMods), and Wii U findings from Cemu, which point to *which* functions/structures to look for even though addresses differ

## 1. Find the position floats (Cheat Engine)
1. Stand still. Scan for an unknown initial value, type Float.
2. Climb or glide up/down, scan "increased"/"decreased". Repeat until a handful remain. (Y is height in BotW.)
3. Freeze/edit a candidate. If Link moves, it's the real position, not a copy.
4. X and Z are usually nearby. BotW actors store a 3×4 transform matrix where the position is the last column, so X/Y/Z are typically 16 bytes apart rather than adjacent.

## 2. Turn it into something stable
Cheat Engine addresses change every launch. We need: *which function* touches the player every frame, and *where in the player object* the floats live.

1. In Cheat Engine, "Find out what writes to this address." Note the instruction address.
2. Convert it to an offset into the game's main module. Ryujinx maps guest memory at a varying host base; subtract the base of the main module region (the start of the executable code in the process) and cross-check in Ghidra that the instruction at that offset is the same (same opcode bytes).
3. In Ghidra, go to that function. Work up the call chain to a function called once per frame with the player object as its first argument (`x0`). Candidates: the player actor's calc/update functions.
4. Field offset = address of the float minus the object pointer in `x0`.

## 3. Plug it in
1. Set `PlayerUpdate` to the function offset (an instruction near the start where `x0` is still the player is fine for an inline hook).
2. Leave the field offsets at 0 at first. The module will log `player object @ 0x...` every 60 frames; check in a memory viewer that this pointer is stable and your position floats sit at a fixed distance from it.
3. Fill in `PlayerPosX/Y/Z` (and `PlayerRotY` if found), rebuild, and watch `tools/fake_client.py` print your real coordinates as you move.

## When it crashes
- Note what you changed and the crash in the offsets log.
- Ryujinx's log shows the faulting PC; subtract the module base to get the offset and check it in Ghidra.
- If the module fails to load at all with an unresolved symbol, see the note at the top of `client/source/program/net/nn_socket.hpp`.
