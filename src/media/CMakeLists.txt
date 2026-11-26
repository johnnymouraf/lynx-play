add_library(mfplay_media STATIC
    AnnexB.cpp
    AnnexB.h
    VideoDecoding.h)
add_library(MFPlay::media ALIAS mfplay_media)

target_link_libraries(mfplay_media PUBLIC MFPlay::core)
mfplay_set_warnings(mfplay_media)

# The D3D12 decoder and the file player need the renderer and FFmpeg's
# D3D12 video API, so they exist only on Windows. Other platforms get the
# decoder interface and AnnexB only.
if(WIN32)
    find_package(FFMPEG REQUIRED)

    # FFmpeg's find module only sets variables. As an imported target its headers
    # count as system headers, out of our strict warnings.
    add_library(mfplay_ffmpeg INTERFACE IMPORTED GLOBAL)
    target_include_directories(mfplay_ffmpeg INTERFACE ${FFMPEG_INCLUDE_DIRS})
    target_link_libraries(mfplay_ffmpeg INTERFACE ${FFMPEG_LIBRARIES})

    target_sources(mfplay_media PRIVATE
        FilePlayer.cpp
        FilePlayer.h
        VideoDecoder.cpp
        VideoDecoder.h)
    target_link_libraries(mfplay_media PUBLIC MFPlay::render PRIVATE mfplay_ffmpeg)
endif()
