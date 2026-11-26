include_guard(GLOBAL)

include(CheckCXXCompilerFlag)

# Clang split designated initializers out of -Wmissing-field-initializers in
# version 19; the Android NDK's older clang only has the broader switch.
check_cxx_compiler_flag(-Wmissing-designated-field-initializers
    MFPLAY_HAS_DESIGNATED_FIELD_WARNING)

if(MFPLAY_HAS_DESIGNATED_FIELD_WARNING)
    set(MFPLAY_NO_PARTIAL_INIT_WARNING -Wno-missing-designated-field-initializers)
else()
    set(MFPLAY_NO_PARTIAL_INIT_WARNING -Wno-missing-field-initializers)
endif()

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
            ${MFPLAY_NO_PARTIAL_INIT_WARNING})
    endif()
endfunction()
