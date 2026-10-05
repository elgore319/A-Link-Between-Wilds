# Using the BotW decomp to find 1.6.0 offsets

**Date:** 2026-10-05. **Status:** research. Nothing here is confirmed in-game yet. Every value is logged as 🔍 in [offsets-log.md](offsets-log.md) until someone verifies it.

## Summary
- The community decompilation ([zeldaret/botw](https://github.com/zeldaret/botw)) names most of BotW's code, but it targets **Switch 1.5.0**. We target 1.6.0 ([decision 0002](../decisions/0002-target-botw-1-6-0.md)).
- For milestone 2 we may not need to find a per-frame function at all. The game keeps a global **`ksys::act::PlayerInfo`** singleton that points to the player actor, and the actor holds its world transform. A Pistonight symbol listing gives the singleton's 1.6.0 address, and a published 1.6.0 mod already reads the player's matrix through that same chain. Its field offsets match the decomp exactly.
- For milestone 3, the decomp shows how the game spawns actors (`ActorCreator::requestCreateActor`, with position in an `InstParamPack`). We only have the 1.5.0 addresses for these. The method for finding them in 1.6.0 is below.

## Sources
We cite facts (addresses, offsets, structure layouts) from these projects and don't copy their code. Both mods are GPLv3; our repo is GPLv2 ([decision 0005](../decisions/0005-gpl-v2-license.md)), so **don't paste code from them**.

| Source | Commit used | What it gives us |
|---|---|---|
| [zeldaret/botw](https://github.com/zeldaret/botw), the decomp | `b9c6e58` | 1.5.0 function names/addresses (`data/uking_functions.csv`, `data/data_symbols.csv`), class layouts with size checks |
| [Pistonight/botw-symbols](https://github.com/Pistonight/botw-symbols) | `e5f3b54` | `listing_160.csv`: 1.6.0 addresses for some decomp symbols |
| [Pistonight/botw-save-state](https://github.com/Pistonight/botw-save-state), a working 1.6.0 exlaunch mod | `cff5bf2` | `src/impl/raw_ptr.hpp`: pointer chains into 1.6.0 memory that are used in practice |

**Address convention.** Decomp addresses have the form `0x71xxxxxxxx`, with the main module based at `0x7100000000`. Subtract that to get `main+0x...`, the form we use in `game_offsets.hpp`. The botw-symbols and save-state values are already `main+` offsets.

## Milestone 2: the player's position

### The structures (decomp, 1.5.0)
`ksys::act::PlayerInfo` (`src/KingSystem/ActorSystem/actPlayerInfo.h`) is a sead singleton:

| Field | Offset | Type | Notes |
|---|---|---|---|
| vtable | `0x00` | | |
| singleton disposer | `0x08` | | from `SEAD_SINGLETON_DISPOSER` |
| `mInfo1`…`mInfo7` | `0x28` | | empty debug leftovers, 8 bytes each |
| **`mPlayerActor`** | **`0x60`** | `PlayerBase*` | null when there's no player (title screen, some loads) |
| `mPlayerLink` | `0x68` | `BaseProcLink` | |
| `mHorseLink` | `0x78` | `BaseProcLink` | |
| **`mPlayerPos`** | **`0x88`** | `Vector3f` | |
| `mPlayerPosForPostCalc` | `0x94` | `Vector3f` | |
| `mPreviousPositions` | `0xB8` | ring buffer of 60 `Vector3f` | possibly the last second of positions |
| (size) | `0x3B0` | | |

How these were computed: we compiled the decomp's header with the field access opened up and took `offsetof`. The computed size, `0x3B0`, equals the decomp's own `KSYS_CHECK_SIZE_NX150(PlayerInfo, 0x3B0)`, so the layout is right for 1.5.0.

`ksys::act::Actor` (`actActor.h`, size `0x840`) keeps **`mMtx`, a `sead::Matrix34f`, at `0x398`**. That's 3 rows × 4 floats, row-major, with the translation in column 3:

| Value | Actor offset |
|---|---|
| X = `m[0][3]` | `0x398 + 0x0C` = **`0x3A4`** |
| Y = `m[1][3]` (height) | `0x398 + 0x1C` = **`0x3B4`** |
| Z = `m[2][3]` | `0x398 + 0x2C` = **`0x3C4`** |
| Facing | rotation part `m[0..2][0..2]`. Yaw = `atan2(m[0][2], m[2][2])`, to be confirmed in-game. |

This also matches what [finding-offsets.md](../finding-offsets.md) predicted: X/Y/Z 16 bytes apart in a 3×4 matrix.

### The 1.6.0 addresses
| What | 1.5.0 (decomp) | 1.6.0 | 1.6.0 source |
|---|---|---|---|
| `PlayerInfo::sInstance` (global pointer) | `main+0x25CDB60` | **`main+0x2CA1140`** | botw-symbols `listing_160.csv` |
| `PlayerInfo::mPlayerActor` | `+0x60` | `+0x60` | save-state uses `[main+0x2CA1140] + 0x60` |
| `Actor::mMtx` | `+0x398` | `+0x398` | save-state names `[[main+0x2CA1140]+0x60] + 0x398` its "main position matrix" |

Two independent projects agree with the decomp's layout on 1.6.0, so the struct offsets apparently didn't change between versions. The global's address did, as expected.

### Proposal: read it from a pointer, not a hook
With these values the module can get the player's position without hooking any function:

```
PlayerInfo*  info  = *(PlayerInfo**)(main + 0x2CA1140);
Actor*       actor = info ? *(Actor**)(info + 0x60) : nullptr;
float x = *(float*)(actor + 0x3A4), y = ... + 0x3B4, z = ... + 0x3C4;
```

That's a change to the plan in [decision 0004](../decisions/0004-network-thread-and-inline-hooks.md) (a read-only inline hook on the player's update), so it needs its own decision record when implemented. Things to weigh:
- **For:** no unknown function to find. The save-state mod shows the chain works on 1.6.0.
- **Against:** if the network thread reads it, the reads aren't synchronized with the game. The player actor can be destroyed during loading screens or death, leaving a stale pointer for a moment, and a read can catch a half-written matrix. Options: validate the pointers each read and treat null as "not in world"; or keep a hook on *any* cheap per-frame game-thread function and do the read there. The "per-frame function" then doesn't have to be the player's.

### How to verify in-game (do this before trusting any of it)
1. In Cheat Engine attached to Ryujinx, find the main module base (see [finding-offsets.md](../finding-offsets.md) §2) and add a pointer entry: base `main+0x2CA1140`, offsets `0x60`, `0x3B4`. It should show Link's height (Y). Climb or glide and watch it change.
2. Add the same chain with `0x3A4` and `0x3C4`. Walking should change X/Z.
3. Freeze Y while standing on flat ground. If Link snaps or teleports, it's the authoritative transform rather than a copy. Physics may fight it, since the Havok position is separate.
4. Check `[main+0x2CA1140] + 0x88` (`mPlayerPos`) as well. If it tracks the matrix translation, it's an alternative that needs one less pointer hop.
5. Go to the title screen and back, and through a shrine load. Note whether `+0x60` becomes 0 or keeps pointing at freed memory.
6. Log the results in [offsets-log.md](offsets-log.md) (✅ or ❌, how verified).

## Milestone 3: spawning stand-in actors (1.5.0 only so far)
| Function | 1.5.0 | Notes |
|---|---|---|
| `ActorCreator::requestCreateActor(name, heap, handle, params, map_object, lane)` | `main+0x11DC7A4` | async spawn by actor name (e.g. an NPC or `GameRomPlayer` clone); returns false if spawns are blocked |
| `ActorCreator::createActor(name, heap, params, sleep_after_init, block_other)` | `main+0x11DC668` | synchronous version |
| `ActorCreator::createInstance(heap)` | `main+0x11DBAE4` | writes `ActorCreator::sInstance`; read its `ADRP`/`STR` to find the global |
| `ActorCreator::eraseActor(actor)` | `main+0x11DD57C` | |
| `InstParamPack::Buffer::add(...)` | `main+0xDCA2D4` | spawn parameters by key: `@P` position, `@R` rotation, `@M` matrix, `@S` scale |

None of these have known 1.6.0 addresses yet. M3 also needs a way to *move* a spawned actor every frame (most likely by writing its `mMtx`, or via `Actor::setProperties`, found in `actActor.h`). That's for later.

## Finding a decomp function in 1.6.0
We don't have the 1.5.0 binary to diff against, so use features of the code that survive between versions:

1. **String references.** Many functions use unique strings. `PlayerInfo::getPlayerPos` logs `"getPlayerPos"` and `getPlayerPosForPostCalc` logs `"getPlayerPosForPostCalc"`. (The logging function they call is an empty stub in release builds, at 4 bytes in the decomp, but the call and its string argument are still there.) In Ghidra (1.6.0 `main`), *Search → For Strings*, open the string, then *References* to land in the function. Check that it reads `x0 + 0x88` (or `+0x94`), which confirms the field layout on 1.6.0.
2. **Neighbours.** Functions from one source file stay together and in the same order. The decomp lists `PlayerInfo`'s functions consecutively (1.5.0 `main+0x852810`…`0x854C98`). Once one is found in 1.6.0, the rest sit nearby, in the same order, with similar sizes (sizes are in `uking_functions.csv`).
3. **Globals from code.** A function that touches a singleton loads it with `ADRP xN, page` then `LDR`/`STR`/`ADD xN, [xN, #off]`, and the address is `page + off`. `createInstance` is the cleanest place to read this because it *stores* the global.
4. **Cross-check with botw-symbols.** `listing_160.csv` has only a few dozen named 1.6.0 symbols (the rest are `sub_` placeholders), but check it first. Search for the mangled name from the decomp (`_ZN4ksys3act...`) before doing any of the above.
5. Log each step and result in [offsets-log.md](offsets-log.md), including dead ends.
