# 0010. Find the 1.0.0 player pointer with an in-game scan

- **Date:** 2026-11-12
- **Status:** Accepted

## Context
M2 needs `PlayerInfo::sInstance` on 1.0.0 ([0009](0009-target-botw-1-0-0.md)). Nobody has published it, and finding it by hand means extracting the game's code and working through it in Ghidra. Cheat Engine pointer scans don't work under Ryujinx: the game's pointers hold guest addresses, while Cheat Engine sees host addresses. Our module runs inside the game, where pointers are real, so it can do the search itself. Lee chose this over the Ghidra route.

## Decision
- `client/source/program/game/player_scan.hpp` walks every 8-byte slot of main's writable data (plus any writable regions right after it, in case `.bss` is mapped separately) and looks for the 1.6.0 shape: global → object, object `+0x60` → actor, actor `+0x398` → a valid transform, object `+0x88` → a copy of the same position. If nothing matches that exactly, it tries nearby offsets (actor pointer `0x40`–`0x80`, matrix `0x300`–`0x480`), and then requires the position copy so it doesn't match every manager that points at an actor.
- A low-priority thread (`player_scan_runner.cpp`) scans every 4 seconds. A tracker keeps each chain across scans and only reports one that was found in 3+ scans, is still there, and moved 50+ units. The player is told to run in a straight line. NPCs and objects don't move that far, so this is what picks Link out.
- It logs one `[albw] scan RESULT: ...` line with the values for `game_offsets.hpp`, then stops. `find-link.bat` (`test-m1.ps1 -Scan`) shows progress live and writes the result into a report.
- Every read goes through `svcQueryMemory` (cached per region), so a bad pointer is skipped instead of crashing. It never writes game memory. It runs on 1.0.0 and also on 1.6.0, where the answer is already known (`main+0x2CA1140`) and so checks the scanner itself. It doesn't run on unknown builds. This is a narrow exception to 0009's "only read the game on 1.0.0": checked, read-only reads used for research.
- Compiled in behind `albw::config::RunPlayerScan`. Turn it off once the offsets are in `game_offsets.hpp`.

## Consequences
- Lee only has to play for a minute and send the report. No Ghidra, no extracting game files.
- If 1.0.0's layout is further from 1.6.0 than the shifted window covers, the scan reports "NOT FOUND". The fallback is the Ghidra route ([finding-offsets.md §4](../finding-offsets.md#4-porting-known-160-values-to-100)), or a wider window.
- While it runs, a full pass costs some CPU on core 2 every 4 seconds. It runs below the network thread's priority and stops after a result.
- The scan logic is pure C++ with a fake address space in `client/tests/test_player_scan.cpp`, so it's tested in CI. The in-game part (memory queries, thread) is only checked by running it.
