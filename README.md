# A Link Between Wilds

Online co-op for *The Legend of Zelda: Breath of the Wild* on Nintendo Switch. Up to four players, one Hyrule.

> **Status: early development.** The network layer works end to end, and the module now reads your real position (offsets from the community decomp, not yet verified in-game). Other players aren't drawn yet, so nothing is playable.

## How it works

```
 ┌───────────── Switch / Ryujinx ─────────────┐          ┌──── PC / home server ────┐
 │ BotW ──hook──> albw module (exlaunch, C++) │ ◄─UDP──► │ albw_server.py (relay)   │ ◄─UDP──► other players
 └────────────────────────────────────────────┘          └──────────────────────────┘
```

- **`client/`**: a C++ module injected into the game with [exlaunch](https://github.com/shadowninja108/exlaunch). It reads your player's state every frame and streams it to the server, and receives everyone else's.
- **`server/`**: a small Python UDP relay. Handles joining, forwards player states, drops players who time out.
- **`protocol/`**: the packet format, shared by both sides.
- **`tools/fake_client.py`**: a pretend player that runs in circles, for testing without a second copy of the game.

## Requirements

- Breath of the Wild **v1.6.0**, dumped from your own Switch. This project never includes or links to game files or keys.
- Ryujinx (or a Switch running Atmosphère, kept offline and on emuMMC)
- [devkitPro](https://devkitpro.org/wiki/Getting_Started) with devkitA64, to build the module
- Python 3.10+ for the server

## Quick start

**On Windows, the fast way:** download the `albw-exefs` artifact from the latest CI run into Downloads, then double-click `tools\windows\test-m1.bat`. It installs the module into Ryujinx, turns on guest logs, starts the server, watches the logs while you launch the game, and writes a pass/fail report to `docs/test-reports/`. Run `test-m1.ps1 -?` for options (including launching the game automatically).

The manual steps:

**1. Run the server** on your PC or home server:

```bash
python3 server/albw_server.py
```

**2. Build the module:**

```bash
cd client
make
```

This produces `client/deploy/subsdk9` and `client/deploy/main.npdm`. (CI also builds these on every push; grab them from the run's artifacts if you don't have devkitPro set up.)

**3. Install it in Ryujinx:** right-click BotW → *Open Mods Directory*, create a folder (e.g. `albw/exefs/`), and copy both files into `exefs`. Or set `RYU_PATH` in `client/config.mk` and run `make deploy-ryu`.

**4. Launch the game.** About 5 seconds after boot, the server should log `player 0 'Link' joined`. With *Logging → Guest logs* enabled in Ryujinx, the module's `[albw]` messages show up in Ryujinx's log.

**5. Add a second player:**

```bash
python3 tools/fake_client.py --name Linkle
```

The fake client prints the position the module sends. Walk around and the numbers should follow you. (If the offsets turn out wrong, set `ReadPlayerFromMemory = false` in `albw_config.hpp` to get the old circle-walking test pattern back.)

Server address and player name come from `albw/config.ini` on the SD card (Ryujinx: the `sdcard` folder in its data folder). Copy [`client/sdcard/albw/config.ini`](client/sdcard/albw/config.ini) there and edit it; without it the module uses `127.0.0.1` and `Link`. See [docs/configuration.md](docs/configuration.md).

## Roadmap

See [docs/roadmap.md](docs/roadmap.md). In short:

1. ✅ Network layer: server, protocol, module joins and streams state
2. ⏳ Find the player update function and position fields ([guide](docs/finding-offsets.md))
3. ⬜ Draw other players in the world
4. ⬜ Animations, then more (enemies, items, world state...)

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Work happens on branches with pull requests into `main`; CI must pass.

## Legal

This is a fan project, not affiliated with or endorsed by Nintendo. You need your own legally obtained copy of the game. Don't use custom firmware online with Nintendo's servers.

Licensed under the [GNU GPL v2](LICENSE), as required by exlaunch.
