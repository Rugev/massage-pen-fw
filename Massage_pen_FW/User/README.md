# User code

Headers go in `Inc/`; C sources go in `Src/`. Add sources explicitly to
[User CMake](CMakeLists.txt); the include directory is already registered.
Files are skeletons with no APIs or behavior yet. Future startup integration
belongs in existing USER CODE blocks in `Core/Src/main.c`, with explicit HAL
handle passing. Keep values as `#define` macros in the relevant headers.

Read [agent rules](../AGENTS.md) and the [context index](../architecture.md).
Build from the project root with `./build.sh` (Debug only).
