# 0011. Look up the socket functions at runtime instead of importing them

- **Date:** 2026-11-12
- **Status:** Accepted

## Context
The client needs a handful of `nn::socket` functions from the game's `sdk` module. They used to be declared in `nn_socket.hpp` and imported the normal way, so the loader matched them by their C++ mangled names. Those names depend on the exact parameter types, which change between SDK versions (plain `int` and a global `sockaddr` in old SDKs, `nn::socket::Family`/`SockAddr`/`MsgFlag` in newer ones), and our signatures were a guess that had never run.

If a guessed name doesn't exist, the loader doesn't fail: it leaves the import at address 0 (exlaunch's `print_unresolved_symbol` is a stub, so nothing is logged). The network thread would then jump to 0 about five seconds after boot and crash the game with no hint why. That is the most likely way for the first in-game test to fail. Marking the declarations weak doesn't help either: the module is linked with `-z nodynamic-undefined-weak`, so a weak import is set to 0 at link time and would never work.

## Decision
- The client no longer imports any `nn::socket` function. `socket_api.cpp` walks every loaded game module (not our own), reads its ELF dynamic symbol table (`DT_HASH`, `DT_SYMTAB`, `DT_STRTAB`, the same tables the Switch loader uses) and looks each function up under every mangled name it has had. The first match wins and is logged with the module it came from.
- `Socket`, `Connect`, `Send` and `Recv` are required. If one is missing, the module logs `[albw] networking off: the game doesn't provide ...`, lists every `nn::socket` function the game does export (up to 60), and stops networking. The game keeps running. `Initialize` (pool form) and `GetLastErrno` are optional.
- `InetAton` and `InetHtons` are no longer used: the IP address and port are converted by our own code (`ParseIpv4`, `Htons`), which is unit tested.
- The symbol table reader (`elf_symbols.hpp`) is pure C++ and tested on a fake module image in `client/tests/test_elf_symbols.cpp`, including a corrupt hash chain.

## Consequences
- A wrong socket guess now shows up in the test report as a clear verdict ("NETWORK OFF") with the real names next to it, so it can be fixed in one round instead of guessing again.
- Adding another SDK spelling means adding a string to a list in `socket_api.cpp`.
- If the game has no socket library at all, the log says so (0 exports listed). That would need a different route, such as talking to the system's `bsd:u` service directly.
- The lookup runs once on the network thread, after the game has booted, so it costs nothing per frame.
- Calling through a function pointer found this way skips the loader's type matching. If a name matched but its real parameters differed from ours in a way that changes the calling convention, it would misbehave instead of failing cleanly. The candidates listed only differ in enums vs `int` and in struct type names, which the AArch64 calling convention treats the same.
