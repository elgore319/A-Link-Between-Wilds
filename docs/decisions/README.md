# Decision records

Every significant technical choice gets a short record here: what we decided, why, and what we gave up. When something breaks months from now, this is where you find out *why* it was built that way before changing it.

- One file per decision, numbered: `NNNN-short-title.md`. Copy `template.md`.
- Never rewrite an accepted record. If a decision changes, write a new one and mark the old one **Superseded by NNNN**.

| # | Decision | Status |
|---|----------|--------|
| [0001](0001-exlaunch-for-code-injection.md) | Use exlaunch for code injection | Accepted |
| [0002](0002-target-botw-1-6-0.md) | Target BotW 1.6.0 only | Accepted |
| [0003](0003-udp-relay-server.md) | Dumb UDP relay server in Python | Accepted |
| [0004](0004-network-thread-and-inline-hooks.md) | Network on its own thread; read-only inline hooks | Accepted |
| [0005](0005-gpl-v2-license.md) | GPLv2 for the whole repo | Accepted |
| [0006](0006-config-file-on-sd-card.md) | Player settings from an INI file on the SD card | Accepted |
| [0008](0008-friend-proof-setup.md) | Setup must be doable by someone who has never modded anything | Accepted |
