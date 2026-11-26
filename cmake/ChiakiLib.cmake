include_guard(GLOBAL)

# Builds only chiaki-ng's protocol library (target `chiaki-lib`) from the pinned
# submodule. chiaki-ng's CMake predates CMP0077, so its options must be forced
# into the cache before add_subdirectory.

set(CHIAKI_SOURCE_DIR "${PROJECT_SOURCE_DIR}/third_party/chiaki-ng")

if(NOT EXISTS "${CHIAKI_SOURCE_DIR}/lib/include/chiaki/session.h")
    message(FATAL_ERROR
        "chiaki-ng is missing. Run scripts/init-chiaki-submodules.ps1 first.")
endif()

foreach(disabled_option IN ITEMS
        CHIAKI_ENABLE_GUI
        CHIAKI_ENABLE_CLI
        CHIAKI_ENABLE_TESTS
        CHIAKI_ENABLE_ANDROID
        CHIAKI_ENABLE_BOREALIS
        CHIAKI_ENABLE_STEAM_SHORTCUT
        CHIAKI_LIB_ENABLE_MBEDTLS
        CHIAKI_LIB_JSONC_EXTERNAL_PROJECT
        CHIAKI_LIB_MINIUPNPC_EXTERNAL_PROJECT)
    set(${disabled_option} OFF CACHE BOOL "" FORCE)
endforeach()

foreach(disabled_tri_option IN ITEMS
        CHIAKI_ENABLE_SETSU
        CHIAKI_ENABLE_STEAMDECK_NATIVE
        CHIAKI_ENABLE_SPEEX
        CHIAKI_ENABLE_FFMPEG_DECODER
        CHIAKI_ENABLE_PI_DECODER
        CHIAKI_USE_SYSTEM_JERASURE
        CHIAKI_USE_SYSTEM_NANOPB
        CHIAKI_USE_SYSTEM_CURL)
    set(${disabled_tri_option} OFF CACHE STRING "" FORCE)
endforeach()

set(CHIAKI_LIB_ENABLE_OPUS ON CACHE BOOL "" FORCE)

# A cross build (Android) searches programs inside the target sysroot, where
# vcpkg's host protoc isn't; chiaki-ng's find_program keeps a preset PROTOC.
file(GLOB host_protoc_dirs LIST_DIRECTORIES true
    "${VCPKG_INSTALLED_DIR}/*/tools/protobuf"
    "${CMAKE_BINARY_DIR}/vcpkg_installed/*/tools/protobuf")
find_program(PROTOC protoc
    HINTS ${host_protoc_dirs}
    NO_CMAKE_FIND_ROOT_PATH)
message(STATUS "protoc: ${PROTOC}")

add_subdirectory("${CHIAKI_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/chiaki-ng" EXCLUDE_FROM_ALL)

# gf-complete calls __cpuidex without including <intrin.h> on MSVC-compatible
# compilers; chiaki-ng patches the source in its own CI, we force the include.
if(WIN32)
    target_compile_options(gf_complete PRIVATE /FIintrin.h)
endif()

# vcpkg's static miniupnpc headers default to dllimport unless told otherwise.
target_compile_definitions(chiaki-lib PRIVATE MINIUPNP_STATICLIB)

# Static libevent and OpenSSL need these Windows libraries, which their
# pkg-config files only list as private. chiaki-lib uses the plain
# target_link_libraries signature, so this call must use it too.
if(WIN32)
    target_link_libraries(chiaki-lib shell32 advapi32 user32 crypt32 bcrypt iphlpapi ws2_32)
endif()

# Keep chiaki's headers out of our strict warnings.
set_target_properties(chiaki-lib PROPERTIES SYSTEM TRUE)
