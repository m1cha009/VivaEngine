// This file is the first place that needs code for each operating system, which is why it lives
// in Platform/: the rest of the engine calls BreakIntoDebuggerOrExit() and never sees an #ifdef.

#include "Platform/Debugger.h"

#include <SDL3/SDL_assert.h>

#include <cstdlib>

#if defined(_WIN32)
    // WIN32_LEAN_AND_MEAN skips rarely used parts of the (huge) windows.h. NOMINMAX stops it from
    // defining min and max as macros, which would break std::min and std::max.
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <windows.h>
#elif defined(__APPLE__)
    #include <sys/sysctl.h>
    #include <unistd.h>
#endif

namespace Viva {

namespace {

bool IsDebuggerAttached()
{
#if defined(_WIN32)
    return IsDebuggerPresent() != FALSE;
#elif defined(__APPLE__)
    // Apple's documented approach (Technical Q&A QA1361): ask the kernel about this process and
    // check whether it's being traced, which is what a debugger does to it.
    int query[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid() };
    kinfo_proc info {};
    size_t size = sizeof(info);
    if (sysctl(query, 4, &info, &size, nullptr, 0) != 0)
        return false;
    return (info.kp_proc.p_flag & P_TRACED) != 0;
#else
    return false;
#endif
}

} // namespace

void BreakIntoDebuggerOrExit()
{
    // Without the check, a breakpoint instruction with no debugger attached counts as a crash.
    // Windows would then offer to launch a just-in-time debugger in a dialog.
    if (IsDebuggerAttached())
        SDL_TriggerBreakpoint();

    // std::_Exit ends the process at once, like std::abort(), but without the
    // "abort() has been called" dialog that the MSVC Debug runtime shows. Destructors and other
    // cleanup don't run, which is what we want: after a fatal error, the state can't be trusted.
    std::_Exit(EXIT_FAILURE);
}

} // namespace Viva
