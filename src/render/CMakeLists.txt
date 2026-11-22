add_library(lynx_render STATIC
    AdapterSelection.cpp
    AdapterSelection.h
    CadenceDetector.cpp
    CadenceDetector.h
    CadenceMetric.cpp
    CadenceMetric.h
    ColorConversion.cpp
    ColorConversion.h
    DlssSuperResolution.cpp
    DlssSuperResolution.h
    FrameGeneration.cpp
    FrameGeneration.h
    FrameMailbox.cpp
    FrameMailbox.h
    FramePresenter.cpp
    FramePresenter.h
    FrameRenderer.cpp
    FrameRenderer.h
    FrameSchedule.cpp
    FrameSchedule.h
    GpuDevice.cpp
    GpuDevice.h
    GpuTimer.cpp
    GpuTimer.h
    Hdr.cpp
    Hdr.h
    Layout.cpp
    Layout.h
    MotionVectors.cpp
    MotionVectors.h
    NgxHost.cpp
    NgxHost.h
    OpticalFlow.cpp
    OpticalFlow.h
    Overlay.cpp
    Overlay.h
    ReflexSession.cpp
    ReflexSession.h
    StreamWindow.cpp
    StreamWindow.h
    SwapChain.cpp
    SwapChain.h
    TestPattern.cpp
    TestPattern.h
    TestPatternSource.cpp
    TestPatternSource.h
    VideoFrame.h
    VideoSuperResolution.cpp
    VideoSuperResolution.h)
add_library(Lynx::render ALIAS lynx_render)

# Shaders compile at build time into headers holding their bytecode:
# render/shaders/<Name>.h defines the byte array k<Name>.
find_program(LYNX_FXC fxc REQUIRED HINTS "$ENV{WindowsSdkVerBinPath}x64")
set(lynx_shader_dir "${CMAKE_CURRENT_BINARY_DIR}/generated/render/shaders")

function(lynx_compile_shader source entry profile name)
    set(output "${lynx_shader_dir}/${name}.h")
    add_custom_command(
        OUTPUT "${output}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${lynx_shader_dir}"
        COMMAND "${LYNX_FXC}" /nologo /WX /O3 /T ${profile} /E ${entry} /Vn k${name}
                /Fh "${output}" "${CMAKE_CURRENT_SOURCE_DIR}/${source}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${source}"
        COMMENT "Compiling ${source} (${entry})"
        VERBATIM)
    target_sources(lynx_render PRIVATE "${output}")
endfunction()

lynx_compile_shader(shaders/VideoFrame.hlsl vertexMain vs_5_1 VideoFrameVertexShader)
lynx_compile_shader(shaders/VideoFrame.hlsl pixelMain ps_5_1 VideoFramePixelShader)
lynx_compile_shader(shaders/Scale.hlsl vertexMain vs_5_1 ScaleVertexShader)
lynx_compile_shader(shaders/Scale.hlsl pixelMain ps_5_1 ScalePixelShader)
lynx_compile_shader(shaders/Cadence.hlsl main cs_5_1 CadenceComputeShader)
lynx_compile_shader(shaders/DlssMotion.hlsl main cs_5_1 DlssMotionComputeShader)
lynx_compile_shader(shaders/FlowDensify.hlsl main cs_5_1 FlowDensifyComputeShader)

# NVIDIA's NGX SDK: the headers, the loader import library and the two DLSS
# runtimes fetched by scripts/fetch-ngx.ps1. A checkout without the download
# still builds, with both features reporting themselves unavailable.
set(LYNX_NGX_ROOT "${PROJECT_SOURCE_DIR}/.ngx" CACHE PATH
    "NVIDIA NGX SDK fetched by scripts/fetch-ngx.ps1")

if(EXISTS "${LYNX_NGX_ROOT}/include/nvsdk_ngx.h" AND
   EXISTS "${LYNX_NGX_ROOT}/lib/nvsdk_ngx_d.lib")
    add_library(lynx_ngx INTERFACE)
    target_include_directories(lynx_ngx SYSTEM INTERFACE "${LYNX_NGX_ROOT}/include")
    target_link_libraries(lynx_ngx INTERFACE "${LYNX_NGX_ROOT}/lib/nvsdk_ngx_d.lib")
    target_compile_definitions(lynx_ngx INTERFACE LYNX_HAVE_NGX=1)
    target_link_libraries(lynx_render PRIVATE lynx_ngx)
endif()

find_package(imgui CONFIG REQUIRED)

# The overlay compiles against ImGui's headers, which are not ours to fix:
# include them as system headers so the strict warnings ignore them.
get_target_property(lynx_imgui_includes imgui::imgui INTERFACE_INCLUDE_DIRECTORIES)

if(lynx_imgui_includes)
    set_property(TARGET imgui::imgui APPEND PROPERTY
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${lynx_imgui_includes}")
endif()

# The NVAPI headers declare the Reflex entry points. They are portable, and
# the driver's import library is never linked: nvapi64.dll loads at run time.
target_include_directories(lynx_render SYSTEM PRIVATE
    "${PROJECT_SOURCE_DIR}/third_party/nvapi"
    "${PROJECT_SOURCE_DIR}/third_party/nvof")
target_compile_definitions(lynx_render PRIVATE LYNX_NVAPI=1)

# The NGX SDK (headers, the /MD loader library and the DLSS-G runtime) is
# fetched by CI from NVIDIA's public DLSS release; without it the renderer
# builds with Frame Generation off.
set(LYNX_NGX_ROOT "" CACHE PATH "NVIDIA NGX SDK folder fetched from NVIDIA/DLSS")
if(LYNX_NGX_ROOT)
    target_include_directories(lynx_render SYSTEM PRIVATE "${LYNX_NGX_ROOT}/include")
    target_compile_definitions(lynx_render PRIVATE LYNX_HAVE_NGX=1)
    target_link_libraries(lynx_render PRIVATE "${LYNX_NGX_ROOT}/lib/nvsdk_ngx_d.lib")
endif()

target_include_directories(lynx_render PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(lynx_render
    PUBLIC Lynx::core d3d12 dxgi
    PRIVATE d3d11 dxguid user32 advapi32 imgui::imgui Lynx::input)
lynx_set_warnings(lynx_render)
