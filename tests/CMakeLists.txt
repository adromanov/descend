# Test configuration for descend library

# Include doctest headers
include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/doctest)

# Single test executable with all test sources
add_executable(descend_tests
    test_main.cpp
    test_stages.cpp
    test_type_semantics.cpp
    test_args.cpp
    test_expand.cpp
    test_transform_one_arg.cpp
    test_finalize.cpp
)

target_link_libraries(descend_tests PRIVATE descend::descend)
target_compile_definitions(descend_tests PRIVATE DESCEND_ENABLE_TYPE_DEBUG=1)

# Configure clang-tidy for static analysis
if(ENABLE_CLANG_TIDY)
    if(NOT CLANG_TIDY_EXE)
        find_program(CLANG_TIDY_EXE
            NAMES ${CLANG_TIDY} clang-tidy-21 clang-tidy-20 clang-tidy-19 clang-tidy-18 clang-tidy
            DOC "Path to clang-tidy executable")
    endif()

    if(CLANG_TIDY_EXE)
        set_target_properties(descend_tests PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
    endif()
endif()

# Register with CTest
add_test(NAME descend_tests COMMAND descend_tests)
