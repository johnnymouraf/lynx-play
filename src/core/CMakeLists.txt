find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

add_library(mfplay_core STATIC
    Files.cpp
    Files.h
    FramePacer.cpp
    FramePacer.h
    GuardedCall.cpp
    GuardedCall.h
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
add_library(MFPlay::core ALIAS mfplay_core)

target_include_directories(mfplay_core PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(mfplay_core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_link_libraries(mfplay_core
    PUBLIC spdlog::spdlog
    PRIVATE nlohmann_json::nlohmann_json shell32 ole32 avrt)
mfplay_set_warnings(mfplay_core)
