# Firmware agent instructions

- Read `architecture.md`, then only the topic documents needed for the task.
- Keep documentation minimal: agreed requirements and code references only.
  Leave undecided architecture and interfaces open.
- Put user headers in `User/Inc` and C sources in `User/Src`. Keep `main.c`
  minimal, using it to start the application.
- Do not modify `Drivers/` or `Core/`, except `Core/Src/main.c`.
- Edit `main.c` only inside existing matching `/* USER CODE BEGIN ... */` and
  `/* USER CODE END ... */` blocks. Preserve markers and existing line endings.
- `Massage_pen_FW.ioc` is read-only. Do not hand-edit generated CMake,
  startup assembly, or the linker script for application features. Report
  required changes to protected files to the user.
- Use pin/port definitions from `Core/Inc/main.h`. Refer to `.ioc` and generated
  code for peripheral configuration; do not duplicate it in documentation.
- Add new sources explicitly to `User/CMakeLists.txt`; `User/Inc` is already
  included. Do not edit generated source lists.
- Pass HAL handles explicitly from `main.c` when initialization is implemented.
- Keep hardware values and tunable constants as `#define` macros in the relevant
  user `.h` files; use existing generated definitions for pins and ports.
- Prefer integer/fixed-point firmware math; use mV and mdegC for voltage and
  temperature. Floating-point calculations may be used in host generation scripts.
- Build only Debug with `./build.sh`. Keep build artifacts out of version control.
- Verify changes appropriately and report what was actually checked. Preserve
  unrelated user changes and do not invent hardware constants or settings.
