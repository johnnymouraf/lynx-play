find_package(FFMPEG REQUIRED)

# FFmpeg's find module only sets variables. As an imported target its headers
# count as system headers, out of our strict warnings.
add_library(mfplay_ffmpeg INTERFACE IMPORTED GLOBAL)
target_include_directories(mfplay_ffmpeg INTERFACE ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(mfplay_ffmpeg INTERFACE ${FFMPEG_LIBRARIES})

add_library(mfplay_media STATIC
    AnnexB.cpp
    AnnexB.h
    FilePlayer.cpp
    FilePlayer.h
    VideoDecoder.cpp
    VideoDecoder.h)
add_library(MFPlay::media ALIAS mfplay_media)

target_link_libraries(mfplay_media
    PUBLIC MFPlay::core MFPlay::render
    PRIVATE mfplay_ffmpeg)
mfplay_set_warnings(mfplay_media)
