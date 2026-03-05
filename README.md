# Mastercc — Master C/C++ for Embedded Software

## Who This Is For
- You are **advanced in C** but have **no C++ experience**
- You are targeting **STM32 (ARM Cortex-M)** embedded systems
- You want to build **real skills through code**, not just read theory

---

## Learning Path

```
Phase 0 → Environment verification
Phase 1 → Advanced C review (pointers, memory, volatile, bit ops, function ptrs)
Phase 2 → C++ from scratch, embedded-safe subset (RAII, templates, constexpr)
Phase 3 → Embedded C patterns (register access, interrupts, memory-mapped I/O)
Phase 4 → Embedded C++ patterns (HAL abstraction, drivers, state machines)
Phase 5 → Protocol implementations (UART, SPI, I2C, CRC — PC-simulated)
Phase 6 → RTOS concepts (scheduler, semaphores, queues — hand-built)
Phase 7 → STM32/ARM Cortex-M specifics (NVIC, startup, linker, clock tree)
Phase 8 → Capstone projects (ring buffer, event bus, CLI, sensor driver, mini-RTOS)
```

Each module lives in its own directory and builds independently.  
Every file is **heavily commented** explaining *why*, not just *what*.

---

## Prerequisites — Install These First

### 1. MinGW-w64 (GCC for Windows) — for PC-native exercises
```powershell
# Option A: via winget (easiest)
winget install --id=GnuWin32.Make -e
winget install MSYS2.MSYS2

# Then inside MSYS2 MINGW64 shell:
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-gdb

# Add to PATH: C:\msys64\mingw64\bin
```

### 2. ARM GCC Cross-Compiler — for STM32 modules
```powershell
winget install Arm.GnuArmEmbeddedToolchain
# or download from: https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads
# Add to PATH: C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\<version>\bin
```

### 3. CMake
```powershell
winget install Kitware.CMake
```

### 4. VS Code Extensions (install once)
- `ms-vscode.cpptools`         — IntelliSense, debugging
- `ms-vscode.cmake-tools`      — CMake integration
- `marus25.cortex-debug`       — STM32 debugging (Phase 7+)

### 5. Verify your setup
```powershell
cd "C:\Users\nlng\Desktop\Work\WORKING_PROJECT\C projects\Mastercc"
cmake --version
gcc --version
arm-none-eabi-gcc --version
```

---

## Building

### Build all PC exercises (phases 1–6, 8)
```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

### Run a specific exercise
```powershell
.\build\01_cpp_fundamentals\01_classes\classes_demo.exe
```

### Build STM32 modules (Phase 7, requires arm-none-eabi-gcc)
```powershell
cmake -S 07_stm32_specific -B build_stm32 -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake
cmake --build build_stm32
```

---

## Module Map

| Dir | Phase | Topic |
|-----|-------|-------|
| `00_environment/` | 0 | Toolchain sanity-check |
| `01_advanced_c/` | 1 | Pointers, memory layout, volatile, bit-ops, preprocessor |
| `02_cpp_fundamentals/` | 2 | Classes, RAII, templates, constexpr, no-heap rules |
| `03_embedded_c/` | 3 | Register macros, ISR patterns, memory-mapped I/O sim |
| `04_embedded_cpp/` | 4 | HAL abstraction, policy-based drivers, state machines |
| `05_protocols/` | 5 | Bit-bang UART/SPI/I2C, CRC8/16/32, framing |
| `06_rtos_concepts/` | 6 | Cooperative scheduler, semaphores, mailboxes |
| `07_stm32_specific/` | 7 | Cortex-M arch, NVIC, startup.s, linker script |
| `08_projects/` | 8 | Capstone: ring buffer, event bus, CLI, mini-RTOS |

---

## Key C++ Rules for Embedded (tatoo these)

```
NO dynamic memory on the MCU heap (no new/delete, no std::vector)
NO exceptions (compile with -fno-exceptions)
NO RTTI      (compile with -fno-rtti)
YES constexpr, templates, inline, RAII, namespaces, references
YES std::array, std::span, std::optional<T> (stack-allocated)
```

These rules are enforced via compiler flags in every CMakeLists.txt.

---

## Recommended Study Order

1. Read the module README first
2. Read the `.hpp`/`.h` header — understand the interface
3. Read the `.cpp`/`.c` implementation — understand the *why* in comments
4. Run the executable, observe output
5. Complete the `// EXERCISE:` challenges in each file
6. Check your solution against the `_solution` file (if present)

---

## Notes on Style

- C++ files use `.cpp` / `.hpp`
- C files use `.c` / `.h`
- All identifiers use `snake_case` (embedded C convention)
- Types use `uint8_t`, `uint32_t` etc. (never `int` for hardware)
- Comments explain *intent* and *hardware context*, not syntax
