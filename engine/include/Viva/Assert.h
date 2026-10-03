#pragma once

#include <format>
#include <string_view>

namespace Viva::Detail {

// Logs a failed VIVA_ASSERT and ends the program. Use the macro instead of calling this.
// [[noreturn]] tells the compiler this function never returns, so it doesn't warn about
// code paths that "fall off" after a failed assert.
[[noreturn]] void AssertFailed(const char* expression, const char* file, int line, std::string_view message = {});

} // namespace Viva::Detail

// VIVA_ASSERT(condition)
// VIVA_ASSERT(condition, "format {}", args...)
//
// Checks something that must always be true. If it isn't, logs the condition, the optional
// message, and the file and line, then stops the program. Like Unity's Debug.Assert, asserts
// exist only in Debug builds. In Release the check disappears entirely, so it costs nothing,
// and the condition must not do work the program relies on.
//
// It's a macro, not a function, because only the preprocessor can turn the condition into
// text (#condition), capture the caller's file and line (__FILE__, __LINE__), and remove the
// whole check from Release builds. __VA_OPT__(...) emits its contents only if a message was
// passed. Note: a condition containing a top-level comma, like std::is_same_v<A, B>, needs an
// extra pair of parentheses, because the preprocessor would split it into two arguments.
//
// The do { ... } while (false) wrapper turns the macro into a single statement, so it behaves
// correctly after an "if" without braces.
#if defined(VIVA_DEBUG)
    #define VIVA_ASSERT(condition, ...)                                                          \
        do {                                                                                     \
            if (!(condition))                                                                    \
                ::Viva::Detail::AssertFailed(#condition, __FILE__, __LINE__                      \
                                             __VA_OPT__(, std::format(__VA_ARGS__)));            \
        } while (false)
#else
    // sizeof() refers to the condition without evaluating it. That avoids "unused variable"
    // warnings for variables that only appear inside asserts.
    #define VIVA_ASSERT(condition, ...) \
        do {                            \
            (void)sizeof(!(condition)); \
        } while (false)
#endif
