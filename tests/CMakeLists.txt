find_package(Catch2 3 CONFIG REQUIRED)

add_executable(mfplay_tests
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
    remoteplay/AccountIdTests.cpp
    remoteplay/ChiakiRuntimeTests.cpp
    remoteplay/ClientDuidTests.cpp
    remoteplay/ConsoleMergeTests.cpp
    remoteplay/ConsoleStoreTests.cpp
    remoteplay/ConsoleWakeTests.cpp
    remoteplay/HostAddressTests.cpp
    remoteplay/PairingFailureTests.cpp
    remoteplay/PinTests.cpp
    remoteplay/PsnConnectionTests.cpp
    remoteplay/PsnDevicesTests.cpp
    remoteplay/PsnLoginTests.cpp
    remoteplay/PsnTokenStoreTests.cpp
    remoteplay/StreamSessionTests.cpp)

target_link_libraries(mfplay_tests PRIVATE
    MFPlay::audio
    MFPlay::input
    MFPlay::media
    MFPlay::remoteplay
    Catch2::Catch2WithMain)

# The Direct3D 12 renderer, the file player and the SEH guard exist only on
# Windows, and so do their tests.
if(WIN32)
    target_sources(mfplay_tests PRIVATE
        core/GuardedCallTests.cpp
        media/FilePlayerTests.cpp
        render/AdapterSelectionTests.cpp
        render/CadenceDetectorTests.cpp
        render/ColorConversionTests.cpp
        render/DlssSuperResolutionTests.cpp
        render/FrameMailboxTests.cpp
        render/FrameScheduleTests.cpp
        render/GpuTimerTests.cpp
        render/HdrTests.cpp
        render/LayoutTests.cpp
        render/OpticalFlowTests.cpp
        render/PresentGateTests.cpp
        render/ReflexTests.cpp
        render/StatisticsLayoutTests.cpp
        render/TestPatternTests.cpp
        render/VideoSuperResolutionTests.cpp)

    target_link_libraries(mfplay_tests PRIVATE MFPlay::render)
endif()

mfplay_set_warnings(mfplay_tests)

include(Catch)
catch_discover_tests(mfplay_tests DISCOVERY_MODE PRE_TEST)
