find_package(nlohmann_json CONFIG REQUIRED)

add_library(lynx_remoteplay STATIC
    AccountId.cpp
    AccountId.h
    Base64.cpp
    Base64.h
    ChiakiLogSink.cpp
    ChiakiLogSink.h
    ChiakiRuntime.cpp
    ChiakiRuntime.h
    Console.h
    ConsoleDiscovery.cpp
    ConsoleDiscovery.h
    ConsoleMerge.cpp
    ConsoleMerge.h
    ConsoleStore.cpp
    ConsoleStore.h
    Dpapi.cpp
    Pin.cpp
    Pin.h
    SecretProtector.h)
add_library(Lynx::remoteplay ALIAS lynx_remoteplay)

target_link_libraries(lynx_remoteplay
    PUBLIC Lynx::core
    PRIVATE chiaki-lib nlohmann_json::nlohmann_json crypt32 iphlpapi ws2_32)
lynx_set_warnings(lynx_remoteplay)
