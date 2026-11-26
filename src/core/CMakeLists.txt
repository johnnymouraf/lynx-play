find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

add_library(mfplay_core STATIC
    Files.cpp
    Files.h
    KeyboardBindings.cpp
    KeyboardBindings.h
    Log.cpp
    Log.h
    Paths.h
    RateCounter.cpp
    RateCounter.h
    ReflexMode.h
    RtxCapabilities.h
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
    VideoCodec.h)
add_library(MFPlay::core ALIAS mfplay_core)

target_include_directories(mfplay_core PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(mfplay_core
    PUBLIC spdlog::spdlog
    PRIVATE nlohmann_json::nlohmann_json)

if(WIN32)
    target_sources(mfplay_core PRIVATE
        FramePacer.cpp
        FramePacer.h
        GuardedCall.cpp
        GuardedCall.h
        Latency.cpp
        Latency.h
        Paths.cpp
        Windows.cpp
        Windows.h)
    target_compile_definitions(mfplay_core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    target_link_libraries(mfplay_core PRIVATE shell32 ole32 avrt)
else()
    target_sources(mfplay_core PRIVATE PathsAndroid.cpp PathsAndroid.h)

    # spdlog's Android sink writes to logcat through liblog.
    if(ANDROID)
        target_link_libraries(mfplay_core PRIVATE log)
    endif()
endif()

mfplay_set_warnings(mfplay_core)
