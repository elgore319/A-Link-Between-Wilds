# exlaunch
A framework for injecting C/C++ code into Nintendo Switch applications/applet/sysmodules.

> [!NOTE]
> This project is a work in progress. If you have issues, reach out to `shad0w0.` on Discord.

# Credit
- Atmosphère: A great reference and guide.
- oss-rtld: Included for (pending) interop with rtld in applications (License [here](https://github.com/shadowninja108/exlaunch/blob/main/source/lib/reloc/rtld/LICENSE.txt)).

---

Vendored into A Link Between Wilds from https://github.com/shadowninja108/exlaunch at commit f9f4b0dd07b68f97958cb9c79228bbca22ca80d5 (2026-08-26). Our code lives in `source/program/`; everything else is upstream exlaunch, lightly configured in `config.mk` and `source/program/setting.hpp`.
