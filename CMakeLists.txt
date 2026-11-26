cmake_minimum_required(VERSION 3.28)

project(MFPlay
    VERSION 1.4.0
    DESCRIPTION "Remote Play client for the PS5 with NVIDIA RTX enhancements"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# No C++ modules here; skip CMake's dependency scan (and its build of std).
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

option(MFPLAY_BUILD_TESTS "Build the unit tests" ON)

# The NDK's libc++ 18 keeps std::jthread and std::stop_token (header-only)
# behind its experimental switch.
if(ANDROID)
    add_compile_definitions(_LIBCPP_ENABLE_EXPERIMENTAL)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(MFPlayWarnings)
include(ChiakiLib)

add_subdirectory(src)

if(MFPLAY_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
