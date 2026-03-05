cmake_minimum_required(VERSION 3.20)

# C++ lessons — note the .cpp extension, C++17 standard enforced globally
add_executable(p2_classes_objects
    01_classes_objects/classes.cpp)

add_executable(p2_raii
    02_raii/raii.cpp)

add_executable(p2_templates
    03_templates/templates.cpp)

add_executable(p2_constexpr
    04_constexpr/constexpr_demo.cpp)

add_executable(p2_namespaces
    05_namespaces/namespaces.cpp)

add_executable(p2_inheritance
    06_inheritance/inheritance.cpp)

foreach(target
    p2_classes_objects p2_raii p2_templates
    p2_constexpr p2_namespaces p2_inheritance)
    target_include_directories(${target} PRIVATE
        ${CMAKE_SOURCE_DIR}/common/include)

    # Embedded-safe C++ flags — no heap via new/delete in these lessons
    target_compile_options(${target} PRIVATE
        -fno-exceptions
        -fno-rtti
        -Wall -Wextra -Wconversion)
endforeach()
