cmake_minimum_required(VERSION 3.20)

add_executable(p4_hal_abstraction 01_hal_abstraction/hal_abstraction.cpp)
add_executable(p4_driver_patterns 02_driver_patterns/driver_patterns.cpp)
add_executable(p4_state_machines 03_state_machines/state_machines.cpp)
add_executable(p4_observer 04_observer/observer.cpp)

foreach(target p4_hal_abstraction p4_driver_patterns p4_state_machines p4_observer)
    target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/common/include)
    target_compile_options(${target} PRIVATE -fno-exceptions -fno-rtti -Wall -Wextra)
endforeach()
