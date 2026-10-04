# Firmware agent instructions

These rules apply throughout the repository. Firmware is in `Massage_pen_FW/`.

- Read `architecture.md`, then only the topic documents needed for the task.
- Keep documentation minimal: agreed requirements and code references only.
  Do not duplicate header constants or generated configuration, add lengthy
  explanations, or record unagreed design decisions. Leave undecided architecture
  and interfaces open; link to source files for details.
- Put user headers in `Massage_pen_FW/User/Inc` and C sources in `Massage_pen_FW/User/Src`. Keep `main.c`
  minimal, using it to start the application.
- Do not modify `Massage_pen_FW/Drivers/` or `Massage_pen_FW/Core/`, except `Massage_pen_FW/Core/Src/main.c`.
- Edit `main.c` only inside existing matching `/* USER CODE BEGIN ... */` and
  `/* USER CODE END ... */` blocks. Preserve markers and existing line endings.
- `Massage_pen_FW/Massage_pen_FW.ioc` is read-only. Do not hand-edit generated CMake,
  startup assembly, or the linker script for application features. Report
  required changes to protected files to the user.
- Use pin/port definitions from `Massage_pen_FW/Core/Inc/main.h`. Refer to `.ioc` and generated
  code for peripheral configuration; do not duplicate it in documentation.
- Add new sources explicitly to `Massage_pen_FW/User/CMakeLists.txt`; `Massage_pen_FW/User/Inc` is already
  included. Do not edit generated source lists.
- Pass HAL handles explicitly from `main.c` when initialization is implemented.
- Keep hardware values and tunable constants as `#define` macros in the relevant
  user `.h` files; use existing generated definitions for pins and ports.
- Follow the integer-math rule in `architecture.md`.
- Build only Debug with `./Massage_pen_FW/build.sh`. Keep build artifacts out of version control.
- Verify changes appropriately and report what was actually checked. Preserve
  unrelated user changes and do not invent hardware constants or settings.
