# viva_set_warnings(<target>)
#
# Turns on a high warning level for one of *our* targets. It's applied per target rather than
# globally, so third-party code like SDL keeps its own settings and doesn't flood the build
# output with warnings we can't fix. It's PRIVATE, so it doesn't leak into targets that link
# against <target> either.
function(viva_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
endfunction()
