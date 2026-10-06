# User code

Headers go in `Inc/`; C sources go in `Src/`. Add sources explicitly to
[User CMake](CMakeLists.txt); the include directory is already registered.
Startup in `main.c` passes HAL handles to [runtime](Inc/runtime.h);
[firmware_hw](Inc/firmware_hw.h) supplies the board adapters.
Optional reviewed profiles are passed at startup; NULL keeps policy gates active.
Keep values as `#define` macros in the relevant headers.

[Storage](Src/storage.c) currently returns dummy levels; flash persistence is not
implemented. [Watchdog](Src/watchdog.c) is a stub; MCU timer selection remains open.

Read [agent rules](../../AGENTS.md) and the [context index](../../architecture.md).
Build from the repository root with `./Massage_pen_FW/build.sh` (Debug only).
