cmake_minimum_required(VERSION 3.20)

add_executable(p3_registers 01_register_access/register_access.c)
add_executable(p3_interrupts 02_interrupt_patterns/interrupt_patterns.c)
add_executable(p3_mmio_sim 03_mmio_simulation/mmio_sim.c)
add_executable(p3_dma 04_dma_concepts/dma_concepts.c)

foreach(target p3_registers p3_interrupts p3_mmio_sim p3_dma)
    target_include_directories(${target} PRIVATE
        ${CMAKE_SOURCE_DIR}/common/include)
endforeach()
