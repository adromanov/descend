cmake_minimum_required(VERSION 3.14)

project(descend VERSION 0.1.0 LANGUAGES CXX)

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (Debug, Release, RelWithDebInfo, MinSizeRel)" FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Common warning flags for all build types
set(COMMON_WARNING_FLAGS "-Wall -Wextra -Wconversion -Wshadow -Wpedantic -Werror")

# Coverage flags (enable with -DENABLE_COVERAGE=ON)
option(ENABLE_COVERAGE "Enable code coverage instrumentation" OFF)

# Sanitizer flags (enabled by default, disable with -DENABLE_SANITIZERS=OFF)
option(ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" ON)

# Clang-tidy static analysis (enabled by default, disable with -DENABLE_CLANG_TIDY=OFF)
option(ENABLE_CLANG_TIDY "Enable clang-tidy static analysis" ON)

# Debug build flags
if(ENABLE_SANITIZERS)
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer ${COMMON_WARNING_FLAGS}" CACHE STRING "Debug flags" FORCE)
else()
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 ${COMMON_WARNING_FLAGS}" CACHE STRING "Debug flags" FORCE)
endif()

# Release build flags
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG ${COMMON_WARNING_FLAGS}" CACHE STRING "Release flags" FORCE)

# Release with debug info
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g -O2 -DNDEBUG ${COMMON_WARNING_FLAGS}" CACHE STRING "RelWithDebInfo flags" FORCE)

# Minimum size release
set(CMAKE_CXX_FLAGS_MINSIZEREL "-Os -DNDEBUG ${COMMON_WARNING_FLAGS}" CACHE STRING "MinSizeRel flags" FORCE)

if(ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Sanitizers enabled for Debug build")
    add_link_options(-fsanitize=address,undefined)
endif()

if(ENABLE_COVERAGE)
    message(STATUS "Code coverage enabled")
    add_compile_options(--coverage)
    add_link_options(--coverage)
endif()

# Header-only library
add_library(descend INTERFACE)
add_library(descend::descend ALIAS descend)

target_include_directories(descend INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(descend INTERFACE cxx_std_20)

# Example executable (only build when this is the main project)
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    add_executable(descend_example examples/basic_usage.cpp)
    target_link_libraries(descend_example PRIVATE descend::descend)

    # Configure clang-tidy for static analysis
    if(ENABLE_CLANG_TIDY)
        find_program(CLANG_TIDY_EXE
            NAMES ${CLANG_TIDY} clang-tidy-21 clang-tidy-20 clang-tidy-19 clang-tidy-18 clang-tidy
            DOC "Path to clang-tidy executable")

        if(CLANG_TIDY_EXE)
            message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
            set_target_properties(descend_example PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
        else()
            message(WARNING "clang-tidy not found. Static analysis disabled.\n"
                "  You can specify the path with -DCLANG_TIDY=<path>")
        endif()
    else()
        message(STATUS "clang-tidy disabled (ENABLE_CLANG_TIDY=OFF)")
    endif()

    # Testing
    enable_testing()
    add_subdirectory(tests)
endif()
