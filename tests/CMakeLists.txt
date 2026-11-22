find_package(Catch2 3 CONFIG REQUIRED)

add_executable(lynx_tests
    audio/AudioOutputTests.cpp
    audio/HapticsTests.cpp
    core/KeyboardMouseTests.cpp
    core/RateCounterTests.cpp
    core/SettingsTests.cpp
    core/SlidingWindowTests.cpp
    core/StreamMetricsTests.cpp
    input/ControllerMappingTests.cpp
    input/DualSenseEffectsTests.cpp
    input/KeyboardMouseTests.cpp
    media/AnnexBTests.cpp
    media/FilePlayerTests.cpp
    remoteplay/AccountIdTests.cpp
    remoteplay/ChiakiRuntimeTests.cpp
    remoteplay/ConsoleMergeTests.cpp
    remoteplay/ConsoleStoreTests.cpp
    remoteplay/ConsoleWakeTests.cpp
    remoteplay/HostAddressTests.cpp
    remoteplay/PairingFailureTests.cpp
    remoteplay/PinTests.cpp
    remoteplay/StreamSessionTests.cpp
    render/AdapterSelectionTests.cpp
    render/CadenceDetectorTests.cpp
    render/ColorConversionTests.cpp
    render/FrameMailboxTests.cpp
    render/GpuTimerTests.cpp
    render/OpticalFlowTests.cpp
    render/LayoutTests.cpp
    render/ReflexTests.cpp
    render/TestPatternTests.cpp
    render/VideoSuperResolutionTests.cpp)

target_link_libraries(lynx_tests PRIVATE
    Lynx::audio
    Lynx::input
    Lynx::media
    Lynx::remoteplay
    Lynx::render
    Catch2::Catch2WithMain)
lynx_set_warnings(lynx_tests)

include(Catch)
catch_discover_tests(lynx_tests DISCOVERY_MODE PRE_TEST)
