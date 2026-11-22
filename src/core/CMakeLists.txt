find_package(spdlog CONFIG REQUIRED)

add_library(lynx_core STATIC
    Log.cpp
    Log.h
    Paths.cpp
    Paths.h)
add_library(Lynx::core ALIAS lynx_core)

target_include_directories(lynx_core PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(lynx_core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_link_libraries(lynx_core
    PUBLIC spdlog::spdlog
    PRIVATE shell32 ole32)
lynx_set_warnings(lynx_core)
