cmake_minimum_required(VERSION 3.20)
project(Mastercc LANGUAGES C CXX)

# ─── Global Settings ──────────────────────────────────────────────────────────
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # enables clangd + VS Code IntelliSense

# ─── Embedded-Safe Compiler Flags ─────────────────────────────────────────────
# These flags mirror what you'd use on a real STM32 firmware build.
# -fno-exceptions   : disable C++ exceptions (no exception tables in ROM)
# -fno-rtti         : disable run-time type info (saves .rodata)
# -Wall / -Wextra   : treat all warnings seriously — embedded code MUST be clean
# -Wconversion      : catch implicit type narrowing (critical for uint8_t math)
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
)

# Debug build by default (includes -g symbols for gdb)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# ─── Common include path shared across all modules ────────────────────────────
include_directories(${CMAKE_SOURCE_DIR}/common/include)

# ─── Sub-modules (each builds its own executables) ────────────────────────────
add_subdirectory(00_environment)
add_subdirectory(01_advanced_c)
add_subdirectory(02_cpp_fundamentals)
add_subdirectory(03_embedded_c)
add_subdirectory(04_embedded_cpp)
add_subdirectory(05_protocols)
add_subdirectory(06_rtos_concepts)
add_subdirectory(08_projects)
add_subdirectory(09_unit_testing)
add_subdirectory(10_c_vs_cpp_embedded)

# 07_stm32_specific uses a separate ARM toolchain — see cmake/arm-none-eabi.cmake
