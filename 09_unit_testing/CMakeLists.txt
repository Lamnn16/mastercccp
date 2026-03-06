add_executable(p9_unit_testing
    unit_testing.c
)
target_include_directories(p9_unit_testing
    PRIVATE ${CMAKE_SOURCE_DIR}/common/include
)

# Unit testing should build at the same optimisation level as production
# so that the compiler cannot optimise away the bugs you're trying to catch.
target_compile_options(p9_unit_testing PRIVATE -O0)
