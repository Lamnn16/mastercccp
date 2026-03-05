cmake_minimum_required(VERSION 3.20)

# Each lesson is a standalone PC executable
add_executable(p6_scheduler
    01_scheduler/scheduler.c)

add_executable(p6_semaphores
    02_semaphores/semaphores.c)

add_executable(p6_mailboxes
    03_mailboxes/mailboxes.c)

add_executable(p6_priority_inv
    04_priority_inversion/priority_inversion.c)

foreach(tgt p6_scheduler p6_semaphores p6_mailboxes p6_priority_inv)
    target_include_directories(${tgt} PRIVATE
        ${CMAKE_SOURCE_DIR}/common/include)
    target_compile_options(${tgt} PRIVATE
        -Wall -Wextra -Wconversion -Wsign-conversion
        -std=c17)
endforeach()
