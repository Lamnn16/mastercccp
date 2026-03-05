cmake_minimum_required(VERSION 3.20)

# ─────────────────────────────────────────────────────────────────────────────
# Module 07 — STM32-Specific (ARM Cortex-M4)
#
# This module requires the ARM cross-compiler and is built SEPARATELY:
#
# cmake -S . -B ../build_stm32 \
# -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-none-eabi.cmake \
# -DCMAKE_BUILD_TYPE=Debug
# cmake --build ../build_stm32
#
# The output .elf files can be flashed with:
# openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
# -c "program 07_stm32_specific/build/cortex_m_arch.elf verify reset exit"
# ─────────────────────────────────────────────────────────────────────────────
project(module07_stm32 C ASM)

set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker/stm32f411re.ld)
set(STARTUP ${CMAKE_CURRENT_SOURCE_DIR}/startup/startup_stm32f411.s)
set(COMMON_INC ${CMAKE_SOURCE_DIR}/../common/include) # adjust if needed

# Common flags for all STM32 targets
set(STM32_COMPILE_FLAGS
    -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
    -mno-thumb-interwork
    -ffunction-sections -fdata-sections
    -DSTM32F411xE -DUSE_HAL_DRIVER
    -Wall -Wextra -Wconversion -Wsign-conversion
    -O0 -g3 -gdwarf-2)

set(STM32_LINK_FLAGS
    -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
    -T${LINKER_SCRIPT}
    -Wl,--gc-sections
    -Wl,-Map=output.map
    -specs=nosys.specs -specs=nano.specs
    --static)

# Helper macro to create an STM32 target
macro(add_stm32_target TARGET_NAME SOURCE)
    add_executable(${TARGET_NAME} ${SOURCE} ${STARTUP})
    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${COMMON_INC})
    target_compile_options(${TARGET_NAME} PRIVATE ${STM32_COMPILE_FLAGS})
    target_link_options(${TARGET_NAME} PRIVATE ${STM32_LINK_FLAGS})

    # Print size after build
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND arm-none-eabi-size --format=berkeley $<TARGET_FILE:${TARGET_NAME}>
        COMMAND arm-none-eabi-objcopy -O ihex $<TARGET_FILE:${TARGET_NAME}>
        $<TARGET_FILE_DIR:${TARGET_NAME}>/${TARGET_NAME}.hex
        COMMAND arm-none-eabi-objcopy -O binary $<TARGET_FILE:${TARGET_NAME}>
        $<TARGET_FILE_DIR:${TARGET_NAME}>/${TARGET_NAME}.bin)
endmacro()

add_stm32_target(p7_cortex_arch 01_cortex_m_arch/cortex_m_arch.c)
add_stm32_target(p7_clock_cfg 02_clock_config/clock_config.c)

# GPIO + UART drivers (combined driver project)
add_executable(p7_gpio_uart
    03_gpio_driver/gpio.c
    04_uart_driver/uart.c
    04_uart_driver/uart_app.c
    ${STARTUP})
target_include_directories(p7_gpio_uart PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/03_gpio_driver
    ${CMAKE_CURRENT_SOURCE_DIR}/04_uart_driver
    ${COMMON_INC})
target_compile_options(p7_gpio_uart PRIVATE ${STM32_COMPILE_FLAGS})
target_link_options(p7_gpio_uart PRIVATE ${STM32_LINK_FLAGS})
add_custom_command(TARGET p7_gpio_uart POST_BUILD
    COMMAND arm-none-eabi-size --format=berkeley $<TARGET_FILE:p7_gpio_uart>
    COMMAND arm-none-eabi-objcopy -O ihex $<TARGET_FILE:p7_gpio_uart>
    $<TARGET_FILE_DIR:p7_gpio_uart>/p7_gpio_uart.hex)
