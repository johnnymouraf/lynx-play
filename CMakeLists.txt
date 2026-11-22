cmake_minimum_required(VERSION 3.28)

project(LynxPlay
    VERSION 0.1.0
    DESCRIPTION "Remote Play client for the PS5 with NVIDIA RTX enhancements"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LYNX_BUILD_TESTS "Build the unit tests" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(LynxWarnings)
include(ChiakiLib)

add_subdirectory(src)

if(LYNX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
