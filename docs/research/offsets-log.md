# Offsets log (BotW Switch 1.0.0)

We target **1.0.0, no update** since 2026-10-26 ([decision 0009](../decisions/0009-target-botw-1-0-0.md)); before that, 1.6.0. Every entry says which version its values are for. 1.5.0/1.6.0 values are references for porting, not values for `game_offsets.hpp`.

Newest entries at the top. Log dead ends too, marked ❌, so nobody repeats them. When a value goes into `game_offsets.hpp`, mark it ✅ and link the commit/PR.

Entry template:

```
## YYYY-MM-DD: <what>
- Value: main+0x... / field +0x...
- Status: ✅ in use | 🔍 candidate | ❌ ruled out
- How found: <steps, tools, what you saw>
- Verified by: <how you confirmed it, e.g. froze value and Link moved>
- Notes:
```

---

## 2026-10-26: what 1.0.0 needs for M2
- Value (1.0.0): `PlayerInfo::sInstance` unknown; `PlayerInfo::mPlayerActor` +0x60 and `Actor::mMtx` +0x398 assumed from 1.5.0/1.6.0.
- Status: 🔍 candidate (fields), unknown (global)
- How found: the field offsets agree on 1.5.0 (decomp size check) and 1.6.0 (botw-save-state), so they likely hold on 1.0.0 too. The global moves with every build, so the 1.6.0 address can't be reused.
- Verified by: not yet. Needs Lee's 1.0.0 `main` in Ghidra (method in [finding-offsets.md §4](../finding-offsets.md#4-porting-known-160-values-to-100)), or a module-side scan (roadmap M2).
- Notes: 1.0.0 predates the DLC (1.3.0/1.4.0 added AI code and subsystems), so `Actor`'s layout is the main risk.

## 2026-10-26: no public 1.0.0 addresses
- Value: none
- Status: ❌ ruled out (as a source)
- How found: Pistonight/botw-symbols (`0abeaa5`, 2025-05-11) only has 1.5.0 and 1.6.0 listings (`listing_160.csv`, `toolkit_150/160.csv`, `symbols/150`, `symbols/160`). The decomp is 1.5.0 only. zeldamods.org's Executable page uses 1.5.0 addresses and says the 1.0.0 executable is stripped. Web searches for 1.0.0 offsets/pointer chains found nothing usable.
- Verified by: n/a
- Notes: don't spend more time searching; port from our own dump instead.

## 2026-10-26: 1.0.0 build ID
- Value: main NSO build ID `2094F152836683A3632C6153FD84AB80` (1.6.0: `8E9978D50BDD20B4C8395A106C27FFDE`)
- Status: 🔍 candidate (in use by the startup version check, `game/build_id.hpp`)
- How found: zeldamods.org/wiki/Versions, Switch table.
- Verified by: not yet. The first in-game run logs `[albw] game version ... (build id ...)`; if it says "not 1.0.0" on Lee's base game, the ID or the note search is wrong.
- Notes: the module finds it as the `NT_GNU_BUILD_ID` ELF note in main's `.rodata`, the same bytes the NSO header carries.

## 2026-10-05: M3 spawn functions (1.5.0 only)
- Value: 1.5.0 `ActorCreator::requestCreateActor` main+0x11DC7A4, `createActor` main+0x11DC668, `createInstance` main+0x11DBAE4, `eraseActor` main+0x11DD57C, `InstParamPack::Buffer::add` main+0xDCA2D4. 1.6.0: unknown.
- Status: 🔍 candidate (needs porting to 1.6.0)
- How found: decomp `data/uking_functions.csv` (zeldaret/botw `b9c6e58`). Details and porting method: [decomp-mapping.md](decomp-mapping.md#milestone-3-spawning-stand-in-actors-150-only-so-far).
- Verified by: not yet.
- Notes: position/rotation are passed as `@P`/`@R` entries in an `InstParamPack`.

## 2026-10-05: player position via PlayerInfo singleton (no hook needed?)
- Value (1.6.0): `PlayerInfo::sInstance` main+0x2CA1140; `PlayerInfo::mPlayerActor` field +0x60; `Actor::mMtx` field +0x398, so X/Y/Z at actor +0x3A4 / +0x3B4 / +0x3C4. Also `PlayerInfo::mPlayerPos` at +0x88.
- Status: 🔍 candidate
- How found: struct layout from the decomp (1.5.0, size check 0x3B0 matches); 1.6.0 global from Pistonight/botw-symbols `listing_160.csv` (`e5f3b54`); the full chain `[main+0x2CA1140]+0x60 → +0x398` is used as the "main position matrix" by Pistonight/botw-save-state (`cff5bf2`), a working 1.6.0 mod. Full write-up: [decomp-mapping.md](decomp-mapping.md).
- Verified by: not yet. Steps in decomp-mapping.md, "How to verify in-game".
- Notes: 1.5.0 address of the global is main+0x25CDB60 (decomp `data_symbols.csv`). Reading this from the network thread would replace the planned player-update hook; needs a decision record (supersedes part of 0004) when implemented. Watch for a stale `mPlayerActor` during loads.

