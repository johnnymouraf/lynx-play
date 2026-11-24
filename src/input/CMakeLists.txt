find_package(SDL3 CONFIG REQUIRED)

add_library(mfplay_input STATIC
    ControllerMapping.cpp
    ControllerMapping.h
    DualSenseEffects.cpp
    DualSenseEffects.h
    GamepadManager.cpp
    GamepadManager.h
    KeyboardMouse.cpp
    KeyboardMouse.h
    Sdl.cpp
    Sdl.h)
add_library(MFPlay::input ALIAS mfplay_input)

target_link_libraries(mfplay_input
    PUBLIC MFPlay::core chiaki-lib SDL3::SDL3)
mfplay_set_warnings(mfplay_input)
