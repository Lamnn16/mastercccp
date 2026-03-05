cmake_minimum_required(VERSION 3.20)

add_executable(p5_uart_framing 01_uart_framing/uart_framing.c)
add_executable(p5_spi_sim 02_spi_simulation/spi_sim.c)
add_executable(p5_i2c_sim 03_i2c_simulation/i2c_sim.c)
add_executable(p5_crc 04_crc_checksums/crc.c)

foreach(target p5_uart_framing p5_spi_sim p5_i2c_sim p5_crc)
    target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/common/include)
endforeach()
