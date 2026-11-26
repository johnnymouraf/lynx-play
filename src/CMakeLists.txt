add_subdirectory(core)
add_subdirectory(input)

# Direct3D 12, NVIDIA's SDKs and the Win32 stream window exist only on Windows.
if(WIN32)
    add_subdirectory(render)
endif()

add_subdirectory(media)
add_subdirectory(audio)
add_subdirectory(remoteplay)
add_subdirectory(app)
