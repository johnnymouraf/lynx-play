add_library(mfplay_render STATIC
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
    OpticalFlowTimer.cpp
    OpticalFlowTimer.h
    Overlay.cpp
    Overlay.h
    PresentGate.cpp
    PresentGate.h
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
add_library(MFPlay::render ALIAS mfplay_render)

# Shaders compile at build time into headers holding their bytecode:
# render/shaders/<Name>.h defines the byte array k<Name>.
find_program(MFPLAY_FXC fxc REQUIRED HINTS "$ENV{WindowsSdkVerBinPath}x64")
set(mfplay_shader_dir "${CMAKE_CURRENT_BINARY_DIR}/generated/render/shaders")

function(mfplay_compile_shader source entry profile name)
    set(output "${mfplay_shader_dir}/${name}.h")
    add_custom_command(
        OUTPUT "${output}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${mfplay_shader_dir}"
        COMMAND "${MFPLAY_FXC}" /nologo /WX /O3 /T ${profile} /E ${entry} /Vn k${name}
                /Fh "${output}" "${CMAKE_CURRENT_SOURCE_DIR}/${source}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${source}"
        COMMENT "Compiling ${source} (${entry})"
        VERBATIM)
    target_sources(mfplay_render PRIVATE "${output}")
endfunction()

mfplay_compile_shader(shaders/VideoFrame.hlsl vertexMain vs_5_1 VideoFrameVertexShader)
mfplay_compile_shader(shaders/VideoFrame.hlsl pixelMain ps_5_1 VideoFramePixelShader)
mfplay_compile_shader(shaders/Scale.hlsl vertexMain vs_5_1 ScaleVertexShader)
mfplay_compile_shader(shaders/Scale.hlsl pixelMain ps_5_1 ScalePixelShader)
mfplay_compile_shader(shaders/Cadence.hlsl main cs_5_1 CadenceComputeShader)
mfplay_compile_shader(shaders/DlssMotion.hlsl main cs_5_1 DlssMotionComputeShader)
mfplay_compile_shader(shaders/FlowDensify.hlsl main cs_5_1 FlowDensifyComputeShader)

# NVIDIA's NGX SDK: the headers, the loader import library and the two DLSS
# runtimes fetched by scripts/fetch-ngx.ps1. A checkout without the download
# still builds, with both features reporting themselves unavailable.
set(MFPLAY_NGX_ROOT "${PROJECT_SOURCE_DIR}/.ngx" CACHE PATH
    "NVIDIA NGX SDK fetched by scripts/fetch-ngx.ps1")

if(EXISTS "${MFPLAY_NGX_ROOT}/include/nvsdk_ngx.h" AND
   EXISTS "${MFPLAY_NGX_ROOT}/lib/nvsdk_ngx_d.lib")
    add_library(mfplay_ngx INTERFACE)
    target_include_directories(mfplay_ngx SYSTEM INTERFACE "${MFPLAY_NGX_ROOT}/include")
    target_link_libraries(mfplay_ngx INTERFACE "${MFPLAY_NGX_ROOT}/lib/nvsdk_ngx_d.lib")
    target_compile_definitions(mfplay_ngx INTERFACE MFPLAY_HAVE_NGX=1)
    target_link_libraries(mfplay_render PRIVATE mfplay_ngx)
endif()

find_package(imgui CONFIG REQUIRED)

# The overlay compiles against ImGui's headers, which are not ours to fix:
# include them as system headers so the strict warnings ignore them.
get_target_property(mfplay_imgui_includes imgui::imgui INTERFACE_INCLUDE_DIRECTORIES)

if(mfplay_imgui_includes)
    set_property(TARGET imgui::imgui APPEND PROPERTY
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${mfplay_imgui_includes}")
endif()

# The NVAPI headers declare the Reflex entry points. They are portable, and
# the driver's import library is never linked: nvapi64.dll loads at run time.
target_include_directories(mfplay_render SYSTEM PRIVATE
    "${PROJECT_SOURCE_DIR}/third_party/nvapi"
    "${PROJECT_SOURCE_DIR}/third_party/nvof")
target_compile_definitions(mfplay_render PRIVATE MFPLAY_NVAPI=1)

# The NGX SDK (headers, the /MD loader library and the DLSS-G runtime) is
# fetched by CI from NVIDIA's public DLSS release; without it the renderer
# builds with Frame Generation off.
set(MFPLAY_NGX_ROOT "" CACHE PATH "NVIDIA NGX SDK folder fetched from NVIDIA/DLSS")
if(MFPLAY_NGX_ROOT)
    target_include_directories(mfplay_render SYSTEM PRIVATE "${MFPLAY_NGX_ROOT}/include")
    target_compile_definitions(mfplay_render PRIVATE MFPLAY_HAVE_NGX=1)
    target_link_libraries(mfplay_render PRIVATE "${MFPLAY_NGX_ROOT}/lib/nvsdk_ngx_d.lib")
endif()

target_include_directories(mfplay_render PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(mfplay_render
    PUBLIC MFPlay::core d3d12 dxgi
    PRIVATE d3d11 dxguid user32 advapi32 imgui::imgui MFPlay::input)
mfplay_set_warnings(mfplay_render)
