# AGENTS.md

## Architecture
This is a strictly separated ROHM nX-U8/100 MCU emulator managed via CMake.
* **Core (Backend):** A C++ library encompassing the CPU instruction set, memory mapping, registers, and hardware peripheral emulation.
* **Frontend:** A C++ executable utilizing **wxWidgets** for the native graphical user interface and **SDL3** for high-performance video rendering, audio output, and low-latency input handling.

## Directory structure
Maintain strict decoupling between the emulation core and the platform-specific frontend.
* `/src/` - Source files (`.cpp`) for the MCU emulation logic. **No UI, OS-specific, wxWidgets, or SDL3 code allowed here.**
* `/include/emxu8/` - Public API defining the emulator state, ROM loading, and clock stepping functions.
* `/app/` - UI components (wxWidgets), render/audio loops (SDL3), and input mapping.
  * `/app/gui/base/` contains base layouts for GUI windows. **DO NOT** modify the scripts in this directory unless explicitly told to.
* `/CMakeLists.txt` - The root build script orchestrating both modules.

## Build system
All AI agents must adhere to the standard out-of-source CMake build pattern.
1. **Configure:** `cmake -B build -S .`
2. **Build:** `cmake --build build`

### CMake dependencies
* The core must be defined as `add_library(emxu8)`.
* The frontend must be defined as `add_executable(emxu8_frontend)`.
* The frontend must link to the core using `target_link_libraries(emxu8_frontend PRIVATE emu_core)`. When adding new source files, append them to the `set(FRONTEND_SOURCES ...)` command.
* The frontend `CMakeLists.txt` must explicitly resolve UI/Media dependencies:
  * `find_package(wxWidgets REQUIRED COMPONENTS core base)`
  * `find_package(SDL3 REQUIRED)`
* Do not use `file(GLOB)`. Explicitly list all source files.

## Coding standards
* Code must comply with C++23 standards.
* The core emulation loop must be meticulously optimized. Avoid dynamic memory allocation (`new`/`malloc`) or throwing exceptions during the hot path (instruction fetch/decode/execute). 
* The emulator core should be fully determinable. Avoid global state; encapsulate the MCU state within a distinct, instantiable class or struct.
* Keep the wxWidgets event loop and the SDL3 rendering/audio threads safely synchronized. Avoid blocking the main GUI thread with the emulation clock loop.

## Git
Do not commit anything; only make changes locally. All AI changes are manually reviewed and committed by a human.

## Agent behavior
1. The frontend uses wxWidgets and SDL3 exclusively. **DO NOT** suggest, write code for, or attempt to integrate ImGui, Qt, SFML, or any other UI/media framework.
2. When implementing opcodes, reference the nX-U8/100 Core Instruction Manual, located at `/docs/FJUZ0317A0-U8-INST-05.md`. Do not guess bitmasks or cycle counts. Never produce actual U8 assembly.
3. Never attempt to print to console (except for debugging purposes) or handle keyboard input directly from inside the `/src/` directory. All I/O must be routed through the frontend via SDL3.
4. Provide exact file modifications.
