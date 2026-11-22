add_library(lynx_render STATIC
    AdapterSelection.cpp
    AdapterSelection.h)
add_library(Lynx::render ALIAS lynx_render)

target_link_libraries(lynx_render PUBLIC Lynx::core)
lynx_set_warnings(lynx_render)
