add_library(lynx_remoteplay STATIC
    ChiakiLogSink.cpp
    ChiakiLogSink.h
    ChiakiRuntime.cpp
    ChiakiRuntime.h
    Console.h
    ConsoleDiscovery.cpp
    ConsoleDiscovery.h)
add_library(Lynx::remoteplay ALIAS lynx_remoteplay)

target_link_libraries(lynx_remoteplay
    PUBLIC Lynx::core
    PRIVATE chiaki-lib iphlpapi ws2_32)
lynx_set_warnings(lynx_remoteplay)
