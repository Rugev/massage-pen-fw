# Debug build

Run `./Massage_pen_FW/build.sh` from the project root. It requires CMake >= 3.22, Ninja and
GNU Arm Embedded tools on PATH, and writes outputs to `Massage_pen_FW/build/Debug`.

- [Firmware CMake](../../Massage_pen_FW/CMakeLists.txt) includes generated CMake and `Massage_pen_FW/User/`.
- [User CMake](../../Massage_pen_FW/User/CMakeLists.txt) includes `Massage_pen_FW/User/Inc`; add new
  `Src/<file>.c` entries explicitly to its source list.
- [Presets](../../Massage_pen_FW/CMakePresets.json) and [toolchain](../../Massage_pen_FW/cmake/gcc-arm-none-eabi.cmake)
  define build configuration; read those files rather than duplicating settings.

Only Debug builds are used. Generated CMake is read-only.

Host runtime checks are available through [`tests/runtime/run.sh`](../../tests/runtime/run.sh).
