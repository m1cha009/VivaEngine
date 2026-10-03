#pragma once

#include <format>
#include <source_location>
#include <string_view>

namespace Viva::Detail {

// Called by VIVA_ASSERT when its condition is false: they log the condition, the optional
// message and where the assert is, then end the program. Use the macro instead of calling these.
// [[noreturn]] tells the compiler a function never returns, so it doesn't warn about code paths
// that "fall off" after a failed assert.
[[noreturn]] void AssertFailed(const char* expression, std::source_location location);
[[noreturn]] void AssertFailedWithMessage(const char* expression, std::source_location location,
                                          std::string_view format, std::format_args args);

// The message version checks the format string at compile time, like Log does, and leaves the
// formatting itself to Assert.cpp.
template <typename... Args>
[[noreturn]] void AssertFailed(const char* expression, std::source_location location,
                               std::format_string<Args...> format, Args&&... args)
{
    AssertFailedWithMessage(expression, location, format.get(), std::make_format_args(args...));
}

} // namespace Viva::Detail

// VIVA_ASSERT(condition)
// VIVA_ASSERT(condition, "format {}", args...)
//
// Checks something that must always be true. If it isn't, logs the condition, the optional
// message, and the file and line, then stops the program. Like Unity's Debug.Assert, asserts
// exist only in Debug builds. In Release the check disappears entirely, so it costs nothing,
// and the condition must not do work the program relies on.
//
// It's a macro, not a function, because only the preprocessor can turn the condition into text
// (#condition) and remove the whole check from Release builds. The file and line come from
// std::source_location::current(), C++20's version of C#'s [CallerFilePath] and
// [CallerLineNumber]: written inside the macro, it reports the line where VIVA_ASSERT is used.
//
// __VA_OPT__(...) emits its contents only if a message was passed. Note: a condition containing
// a top-level comma, like std::is_same_v<A, B>, needs an extra pair of parentheses, because the
// preprocessor would split it into two arguments. The do { ... } while (false) wrapper turns the
// macro into a single statement, so it behaves correctly after an "if" without braces.
#if defined(VIVA_DEBUG)
    #define VIVA_ASSERT(condition, ...)                                                          \
        do {                                                                                     \
            if (!(condition))                                                                    \
                ::Viva::Detail::AssertFailed(#condition, std::source_location::current()        \
                                             __VA_OPT__(, __VA_ARGS__));                         \
        } while (false)
#else
    // sizeof() refers to the condition without evaluating it. That avoids "unused variable"
    // warnings for variables that only appear inside asserts.
    #define VIVA_ASSERT(condition, ...) \
        do {                            \
            (void)sizeof(!(condition)); \
        } while (false)
#endif
