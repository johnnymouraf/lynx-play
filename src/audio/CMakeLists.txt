find_package(SDL3 CONFIG REQUIRED)

add_library(lynx_audio STATIC
    AudioOutput.cpp
    AudioOutput.h
    Devices.cpp
    Devices.h
    Haptics.cpp
    Haptics.h
    Microphone.cpp
    Microphone.h)
add_library(Lynx::audio ALIAS lynx_audio)

target_link_libraries(lynx_audio
    PUBLIC Lynx::core SDL3::SDL3)
lynx_set_warnings(lynx_audio)
