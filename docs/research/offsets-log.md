# Offsets log (BotW Switch 1.6.0)

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
- Notes: **In code since the feature/player-position PR** (`game_offsets.hpp`, decision 0007) but still unverified. The module logs `[albw] player pos (...) | PlayerInfo pos (...)` every 5 s; compare it with Cheat Engine or with where Link is. 1.5.0 address of the global is main+0x25CDB60 (decomp `data_symbols.csv`). Reading this from the network thread would replace the planned player-update hook; needs a decision record (supersedes part of 0004) when implemented. Watch for a stale `mPlayerActor` during loads.

