# viva_set_warnings(<target>)
#
# Turns on a high warning level for one of *our* targets and treats every warning as an error,
# so "zero warnings" is enforced by the build itself. It's applied per target rather than
# globally, so third-party code like SDL keeps its own settings, and it doesn't leak into
# targets that link against <target> either.
#
# To try something quick without fixing its warnings first, configure with
# "cmake --preset windows-debug --compile-no-warning-as-error".
function(viva_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
    set_target_properties(${target} PROPERTIES COMPILE_WARNING_AS_ERROR ON)
endfunction()
