add_library(lynx_render STATIC
    AdapterSelection.cpp
    AdapterSelection.h
    ColorConversion.cpp
    ColorConversion.h
    FrameMailbox.cpp
    FrameMailbox.h
    FramePresenter.cpp
    FramePresenter.h
    FrameRenderer.cpp
    FrameRenderer.h
    GpuDevice.cpp
    GpuDevice.h
    Layout.cpp
    Layout.h
    StreamWindow.cpp
    StreamWindow.h
    SwapChain.cpp
    SwapChain.h
    TestPattern.cpp
    TestPattern.h
    TestPatternSource.cpp
    TestPatternSource.h
    VideoFrame.h)
add_library(Lynx::render ALIAS lynx_render)

# Shaders compile at build time into headers holding their bytecode, included
# as render/shaders/<Name>.h.
find_program(LYNX_FXC fxc REQUIRED HINTS "$ENV{WindowsSdkVerBinPath}x64")
set(lynx_shader_dir "${CMAKE_CURRENT_BINARY_DIR}/generated/render/shaders")

function(lynx_compile_shader source entry profile name)
    set(output "${lynx_shader_dir}/${name}.h")
    add_custom_command(
        OUTPUT "${output}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${lynx_shader_dir}"
        COMMAND "${LYNX_FXC}" /nologo /WX /O3 /T ${profile} /E ${entry} /Vn ${name}
                /Fh "${output}" "${CMAKE_CURRENT_SOURCE_DIR}/${source}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${source}"
        COMMENT "Compiling ${source} (${entry})"
        VERBATIM)
    target_sources(lynx_render PRIVATE "${output}")
endfunction()

lynx_compile_shader(shaders/VideoFrame.hlsl vertexMain vs_5_1 kVideoFrameVertexShader)
lynx_compile_shader(shaders/VideoFrame.hlsl pixelMain ps_5_1 kVideoFramePixelShader)

target_include_directories(lynx_render PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(lynx_render
    PUBLIC Lynx::core d3d12 dxgi
    PRIVATE dxguid user32)
lynx_set_warnings(lynx_render)
