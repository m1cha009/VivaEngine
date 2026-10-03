#pragma once

// Dear ImGui's compile-time settings, its "imconfig". The IMGUI_USER_CONFIG define (see
// cmake/Dependencies.cmake) makes imgui.h include this file first, in ImGui's own source files and
// in ours alike, so everything is compiled with the same settings. Games don't include it
// themselves; they include <imgui.h>.

#include <source_location>

namespace Viva::Detail {

// The function VIVA_ASSERT calls when a check fails (Viva/Assert.h, defined in Core/Assert.cpp):
// it logs the check and where it is, then stops the program. Declared again here rather than
// including Assert.h, which would pull <format> into every file that includes imgui.h.
[[noreturn]] void AssertFailed(const char* expression, std::source_location location);

} // namespace Viva::Detail

// ImGui checks the rules of its API with IM_ASSERT: every Begin needs its End, IDs must be unique,
// and so on. By default that's the C library's assert(), whose failure dialog in Debug builds
// would freeze the window. Here a broken rule goes through the engine's assert handling instead,
// with ImGui's file and line. As with assert(), the checks exist only when NDEBUG isn't defined,
// which is in Debug builds.
#ifdef NDEBUG
    #define IM_ASSERT(expression) ((void)0)
#else
    #define IM_ASSERT(expression) \
        ((expression) ? (void)0 : ::Viva::Detail::AssertFailed(#expression, std::source_location::current()))
#endif
