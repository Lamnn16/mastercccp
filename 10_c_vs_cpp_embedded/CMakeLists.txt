add_executable(p10_c_vs_cpp
    c_vs_cpp.cpp
)
target_include_directories(p10_c_vs_cpp
    PRIVATE ${CMAKE_SOURCE_DIR}/common/include
)
target_compile_options(p10_c_vs_cpp PRIVATE -O2)
