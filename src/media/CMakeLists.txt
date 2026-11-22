find_package(FFMPEG REQUIRED)

# FFmpeg's find module only sets variables. As an imported target its headers
# count as system headers, out of our strict warnings.
add_library(lynx_ffmpeg INTERFACE IMPORTED GLOBAL)
target_include_directories(lynx_ffmpeg INTERFACE ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(lynx_ffmpeg INTERFACE ${FFMPEG_LIBRARIES})

add_library(lynx_media STATIC
    AnnexB.cpp
    AnnexB.h
    FilePlayer.cpp
    FilePlayer.h
    VideoDecoder.cpp
    VideoDecoder.h)
add_library(Lynx::media ALIAS lynx_media)

target_link_libraries(lynx_media
    PUBLIC Lynx::core Lynx::render
    PRIVATE lynx_ffmpeg)
lynx_set_warnings(lynx_media)
