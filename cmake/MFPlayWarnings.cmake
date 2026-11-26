include_guard(GLOBAL)

# Strict warnings for MFPlay's own targets; third-party code keeps its own.
# On Windows (clang-cl) the MSVC-style flags apply. Two are off because Windows
# code needs them: IID_PPV_ARGS expands to __uuidof, and Win32 structs are
# zero-filled through partial designated initializers.
function(mfplay_set_warnings target)
    if(WIN32)
        target_compile_options(${target} PRIVATE
            /W4
            /WX
            /utf-8
            -Wconversion
            -Wshadow
            -Wpedantic
            -Wno-language-extension-token
            -Wno-missing-designated-field-initializers)
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wshadow
            -Werror
            -Wno-language-extension-token
            -Wno-missing-designated-field-initializers)
    endif()
endfunction()
