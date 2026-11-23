find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

add_library(lynx_core STATIC
    Files.cpp
    Files.h
    FramePacer.cpp
    FramePacer.h
    KeyboardBindings.cpp
    KeyboardBindings.h
    Latency.cpp
    Latency.h
    Log.cpp
    Log.h
    Paths.cpp
    Paths.h
    RateCounter.cpp
    RateCounter.h
    Settings.cpp
    Settings.h
    SlidingWindow.cpp
    SlidingWindow.h
    StreamMetrics.cpp
    StreamMetrics.h
    TerminateLogging.cpp
    TerminateLogging.h
    Text.cpp
    Text.h
    VideoCodec.h
    Windows.cpp
    Windows.h)
add_library(Lynx::core ALIAS lynx_core)

target_include_directories(lynx_core PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(lynx_core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_link_libraries(lynx_core
    PUBLIC spdlog::spdlog
    PRIVATE nlohmann_json::nlohmann_json shell32 ole32 avrt)
lynx_set_warnings(lynx_core)
