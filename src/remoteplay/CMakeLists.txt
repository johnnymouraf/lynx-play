find_package(nlohmann_json CONFIG REQUIRED)

add_library(mfplay_remoteplay STATIC
    AccountId.cpp
    AccountId.h
    Base64.cpp
    Base64.h
    ChiakiLogSink.cpp
    ChiakiLogSink.h
    ChiakiRegisteredHost.cpp
    ChiakiRegisteredHost.h
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
    HostAddress.cpp
    HostAddress.h
    Pin.cpp
    Pin.h
    PsnConnection.cpp
    PsnConnection.h
    PsnDevices.cpp
    PsnDevices.h
    PsnLogin.cpp
    PsnLogin.h
    PsnPairing.cpp
    PsnPairing.h
    PsnPolicy.cpp
    PsnPolicy.h
    PsnTokenStore.cpp
    PsnTokenStore.h
    SecretProtector.h
    StreamSession.cpp
    StreamSession.h)
add_library(MFPlay::remoteplay ALIAS mfplay_remoteplay)

if(WIN32)
    target_sources(mfplay_remoteplay PRIVATE Dpapi.cpp)
else()
    target_sources(mfplay_remoteplay PRIVATE SecretProtectorAndroid.cpp)
endif()

target_link_libraries(mfplay_remoteplay
    PUBLIC MFPlay::core MFPlay::media chiaki-lib
    PRIVATE MFPlay::audio MFPlay::input nlohmann_json::nlohmann_json)

if(WIN32)
    target_link_libraries(mfplay_remoteplay PRIVATE crypt32 iphlpapi ws2_32)
endif()

mfplay_set_warnings(mfplay_remoteplay)
