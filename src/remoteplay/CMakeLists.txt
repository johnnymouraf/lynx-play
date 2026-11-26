find_package(nlohmann_json CONFIG REQUIRED)

add_library(mfplay_remoteplay STATIC
    AccountId.cpp
    AccountId.h
    Base64.cpp
    Base64.h
    ChiakiLogSink.cpp
    ChiakiLogSink.h
    ChiakiRuntime.cpp
    ChiakiRuntime.h
    ClientDuid.cpp
    ClientDuid.h
    Console.h
    ConsoleDiscovery.cpp
    ConsoleDiscovery.h
    ConsoleMerge.cpp
    ConsoleMerge.h
    ConsoleRegistration.cpp
    ConsoleRegistration.h
    ConsoleStore.cpp
    ConsoleStore.h
    ConsoleWake.cpp
    ConsoleWake.h
    Dpapi.cpp
    HostAddress.cpp
    HostAddress.h
    Pin.cpp
    Pin.h
    PsnDevices.cpp
    PsnDevices.h
    PsnLogin.cpp
    PsnLogin.h
    PsnTokenStore.cpp
    PsnTokenStore.h
    SecretProtector.h
    StreamSession.cpp
    StreamSession.h)
add_library(MFPlay::remoteplay ALIAS mfplay_remoteplay)

target_link_libraries(mfplay_remoteplay
    PUBLIC MFPlay::core MFPlay::render
    PRIVATE MFPlay::audio MFPlay::input MFPlay::media chiaki-lib nlohmann_json::nlohmann_json
            crypt32 iphlpapi ws2_32)
mfplay_set_warnings(mfplay_remoteplay)
