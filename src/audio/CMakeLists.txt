find_package(SDL3 CONFIG REQUIRED)

add_library(mfplay_audio STATIC
    AudioOutput.cpp
    AudioOutput.h
    Devices.cpp
    Devices.h
    Haptics.cpp
    Haptics.h
    Microphone.cpp
    Microphone.h)
add_library(MFPlay::audio ALIAS mfplay_audio)

target_link_libraries(mfplay_audio
    PUBLIC MFPlay::core SDL3::SDL3)
mfplay_set_warnings(mfplay_audio)
