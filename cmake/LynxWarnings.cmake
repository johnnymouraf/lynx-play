include_guard(GLOBAL)

# Strict warnings for Lynx Play's own targets; third-party code keeps its own.
function(lynx_set_warnings target)
    target_compile_options(${target} PRIVATE
        /W4
        /WX
        /utf-8
        -Wconversion
        -Wshadow
        -Wpedantic)
endfunction()
