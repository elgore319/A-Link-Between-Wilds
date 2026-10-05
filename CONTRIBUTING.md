# Contributing

## Who we build for
Players are people who've never modded a game. The goal is **one installer, a name and a join code** for players, and **one click** for the host ([decision 0008](docs/decisions/0008-friend-proof-setup.md)). For every change, ask: *how does a player use this without a terminal, a text editor or a hidden folder?* If the answer is "they don't, yet", say so in the PR.

## Workflow
1. **Open or pick an issue** describing the change.
2. **Branch from `main`**: `feature/<short-name>`, `fix/<short-name>`, `docs/<short-name>` or `research/<short-name>`.
3. **Commit in small, focused steps** using [Conventional Commits](https://www.conventionalcommits.org/): `feat(client): ...`, `fix(server): ...`, `docs: ...`, `test: ...`, `chore: ...`, `ci: ...`. Explain *why* in the body when it isn't obvious.
4. **Open a pull request** using the template. CI must pass. Reference the issue (`Closes #12`).
5. **Squash or rebase merge**, never merge commits on `main`. Delete the branch after.

`main` should always build and pass tests.

## Paper trail (required)
This project is reverse engineering-heavy, so undocumented knowledge gets lost. With every change:
- **Changelog:** add a line under `[Unreleased]` in `CHANGELOG.md`.
- **Offsets/structures found or ruled out:** log in `docs/research/offsets-log.md`, with how they were found.
- **Design decisions** (new dependency, protocol change, architecture change): add a record in `docs/decisions/`.
- **Protocol changes:** update `protocol/albw_protocol.h`, `server/protocol.py` and `docs/protocol.md` together and bump `ALBW_PROTOCOL_VERSION`.
- **Bugs:** file an issue with the bug template even if you fix it right away, so the symptoms are searchable later.

## Layout
| Path | What |
|---|---|
| `client/` | Switch module. Our code is in `client/source/program/` (host tests in `client/tests/`, example SD card files in `client/sdcard/`); the rest is vendored exlaunch (see `client/EXLAUNCH.md`) and shouldn't be edited except to update it. |
| `server/` | Python relay server and tests |
| `protocol/` | Packet definitions shared with the client |
| `tools/` | Dev utilities |
| `docs/` | Everything written down |

## Running tests
```bash
cd server && python3 -m unittest -v

# Module code that doesn't depend on the Switch SDK (currently the config parser):
cd client/tests
g++ -std=c++20 -Wall -Wextra -Werror -I../source -I../../protocol test_config_parser.cpp -o test_config_parser && ./test_config_parser
```

## Never commit
Game files, dumps, keys, or anything copyrighted by Nintendo. `.gitignore` blocks the common ones; double-check `git status` anyway.
