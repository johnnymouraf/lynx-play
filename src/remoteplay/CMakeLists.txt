add_library(lynx_remoteplay STATIC
    ChiakiRuntime.cpp
    ChiakiRuntime.h)
add_library(Lynx::remoteplay ALIAS lynx_remoteplay)

target_link_libraries(lynx_remoteplay
    PUBLIC Lynx::core
    PRIVATE chiaki-lib)
lynx_set_warnings(lynx_remoteplay)
