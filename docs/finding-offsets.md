# Finding game offsets

Goal for milestone 2: fill in [`game_offsets.hpp`](../client/source/program/game/game_offsets.hpp) so the module sends your real position instead of the test pattern.

> **We target 1.0.0** ([decision 0009](decisions/0009-target-botw-1-0-0.md)). [research/decomp-mapping.md](research/decomp-mapping.md) has strong candidates for the player's position, but for **1.6.0**: the *method* (the `PlayerInfo` pointer chain, no hook needed) carries over, the addresses don't. Use §4 below to port them to 1.0.0, then verify with § "How to verify in-game" there.

**Record everything you find in [`research/offsets-log.md`](research/offsets-log.md)**, including dead ends. That log is how we re-find things after mistakes and how anyone else picks up the work.

## Tools
- Ryujinx with BotW 1.0.0 (no update: right-click > *Manage Title Updates* > *No Update*)
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

## 4. Porting known 1.6.0 values to 1.0.0
Our target is 1.0.0 ([decision 0009](decisions/0009-target-botw-1-0-0.md)), but almost all public research is for 1.5.0 (the decomp) or 1.6.0. We found no published 1.0.0 addresses (dead ends are in the [offsets log](research/offsets-log.md)), so they have to come from your own copy. What's needed for M2:

| Value | 1.6.0 | 1.0.0 |
|---|---|---|
| `PlayerInfo::sInstance` (global) | `main+0x2CA1140` | unknown: different on every version |
| `PlayerInfo::mPlayerActor` (field) | `+0x60` | probably `+0x60`, to confirm |
| `Actor::mMtx` (field), so X/Y/Z at `+0x3A4/+0x3B4/+0x3C4` | `+0x398` | probably `+0x398`, to confirm: 1.0.0 predates the DLC, which may have added actor fields |

**Static (Ghidra).** In Ryujinx, right-click BotW > *Extract Data* > *ExeFS* and save it to a folder **outside this repo** (it's game code; never commit or upload it). Load `main` in Ghidra and use the method at the end of [decomp-mapping.md](research/decomp-mapping.md#finding-a-decomp-function-in-160), which works for any version: string references, neighbouring functions, `ADRP`/`LDR` pairs for globals. Confirm the field offsets in the functions you land in (e.g. `getPlayerPos` should read `x0 + 0x88`).

**Why not a Cheat Engine pointer scan?** Under Ryujinx the game's pointers hold *guest* addresses, while Cheat Engine sees *host* addresses, so a pointer scan for Link's position finds nothing useful. §1 still works for finding the floats themselves. A module-side scan (the module runs inside the game, where pointers are real) is the planned way to find the global without Ghidra; see the roadmap.

Log what you find, with the 1.0.0 build ID (`2094F152836683A3632C6153FD84AB80`), in the offsets log.

## When it crashes
- Note what you changed and the crash in the offsets log.
- Ryujinx's log shows the faulting PC; subtract the module base to get the offset and check it in Ghidra.
- If the module fails to load at all with an unresolved symbol, see the note at the top of `client/source/program/net/nn_socket.hpp`.
