add_library(lynx_render STATIC
    AdapterSelection.cpp
    AdapterSelection.h
    FrameRenderer.cpp
    FrameRenderer.h
    GpuDevice.cpp
    GpuDevice.h
    StreamWindow.cpp
    StreamWindow.h
    SwapChain.cpp
    SwapChain.h
    TestPattern.cpp
    TestPattern.h)
add_library(Lynx::render ALIAS lynx_render)

target_link_libraries(lynx_render
    PUBLIC Lynx::core d3d12 dxgi
    PRIVATE dxguid user32)
lynx_set_warnings(lynx_render)
