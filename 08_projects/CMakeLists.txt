cmake_minimum_required(VERSION 3.20)

# ─────────────────────────────────────────────────────────────────────────────
# Module 08 — Capstone Projects
#
# Each project is production-quality code that combines concepts from earlier
# modules. Study, extend, and adapt these for your real STM32 projects.
# ─────────────────────────────────────────────────────────────────────────────

# ── Project 1: Production-grade ring buffer with test harness ──
add_executable(p8_ring_buffer
    01_ring_buffer/ring_buffer.c
    01_ring_buffer/ring_buffer_test.c)

# ── Project 2: Interrupt-safe event bus ──
add_executable(p8_event_bus
    02_event_bus/event_bus.c
    02_event_bus/event_bus_app.c)

# ── Project 3: UART command-line interface ──
add_executable(p8_cli
    03_cli_uart/cli.c
    03_cli_uart/cli_app.c)

# ── Project 4: BMP280 sensor driver (I2C) ──
add_executable(p8_sensor_driver
    04_sensor_driver/bmp280.c
    04_sensor_driver/bmp280_app.c)

# ── Project 5: Mini cooperative RTOS ──
add_executable(p8_mini_rtos
    05_mini_rtos/rtos.c
    05_mini_rtos/rtos_app.c)

foreach(tgt
    p8_ring_buffer p8_event_bus p8_cli p8_sensor_driver p8_mini_rtos)
    target_include_directories(${tgt} PRIVATE
        ${CMAKE_SOURCE_DIR}/common/include
        ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_options(${tgt} PRIVATE
        -Wall -Wextra -Wconversion -Wsign-conversion
        -std=c17 -O2)
endforeach()
