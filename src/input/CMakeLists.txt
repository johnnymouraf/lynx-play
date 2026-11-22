find_package(SDL3 CONFIG REQUIRED)

add_library(lynx_input STATIC
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
add_library(Lynx::input ALIAS lynx_input)

target_link_libraries(lynx_input
    PUBLIC Lynx::core chiaki-lib SDL3::SDL3)
lynx_set_warnings(lynx_input)
