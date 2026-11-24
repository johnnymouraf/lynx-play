include_guard(GLOBAL)

# Strict warnings for MFPlay's own targets; third-party code keeps its own.
# Two are off because Windows code needs them: IID_PPV_ARGS expands to
# __uuidof, and Win32 structs are zero-filled through partial designated
# initializers.
function(mfplay_set_warnings target)
    target_compile_options(${target} PRIVATE
        /W4
        /WX
        /utf-8
        -Wconversion
        -Wshadow
        -Wpedantic
        -Wno-language-extension-token
        -Wno-missing-designated-field-initializers)
endfunction()
