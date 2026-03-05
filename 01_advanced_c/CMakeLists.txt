cmake_minimum_required(VERSION 3.20)

# Each lesson builds its own standalone executable so you can run them
# independently. Use `cmake --build build --target <name>` to build one.
add_executable(p1_pointers
    01_pointers/pointers.c)

add_executable(p1_memory_layout
    02_memory_layout/memory_layout.c)

add_executable(p1_volatile
    03_volatile_const/volatile_const.c)

add_executable(p1_bitops
    04_bit_manipulation/bit_manipulation.c)

add_executable(p1_function_ptrs
    05_function_pointers/function_pointers.c)

add_executable(p1_preprocessor
    06_preprocessor/preprocessor.c)

add_executable(p1_struct_alignment
    07_struct_alignment/struct_alignment.c)

# All lessons share the common include
foreach(target
    p1_pointers p1_memory_layout p1_volatile
    p1_bitops p1_function_ptrs p1_preprocessor p1_struct_alignment)
    target_include_directories(${target} PRIVATE
        ${CMAKE_SOURCE_DIR}/common/include)
endforeach()
