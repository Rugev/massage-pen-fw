# Debug build

Run `./build.sh` from the project root. It requires CMake >= 3.22, Ninja and
GNU Arm Embedded tools on PATH, and writes outputs to `build/Debug`.

- [Root CMake](../../CMakeLists.txt) includes generated CMake and `User/`.
- [User CMake](../../User/CMakeLists.txt) includes `User/Inc`; add new
  `Src/<file>.c` entries explicitly to its source list.
- [Presets](../../CMakePresets.json) and [toolchain](../../cmake/gcc-arm-none-eabi.cmake)
  define build configuration; read those files rather than duplicating settings.

Only Debug builds are used. Generated CMake is read-only.
