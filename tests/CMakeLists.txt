find_package(Catch2 3 CONFIG REQUIRED)

add_executable(lynx_tests
    core/RateCounterTests.cpp
    core/SettingsTests.cpp
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
    render/ColorConversionTests.cpp
    render/FrameMailboxTests.cpp
    render/LayoutTests.cpp
    render/TestPatternTests.cpp)

target_link_libraries(lynx_tests PRIVATE
    Lynx::media
    Lynx::remoteplay
    Lynx::render
    Catch2::Catch2WithMain)
lynx_set_warnings(lynx_tests)

include(Catch)
catch_discover_tests(lynx_tests DISCOVERY_MODE PRE_TEST)
