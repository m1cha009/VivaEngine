#pragma once

#include <format>
#include <string_view>
#include <utility>

namespace Viva {

// Engine-wide logging: the engine's version of Unity's Debug.Log, LogWarning and LogError.
//
// Messages use std::format syntax, which plays the role of C# string interpolation:
//     Log::Info("Loaded {} meshes in {:.2f} ms", meshCount, milliseconds);
// The format string is checked at compile time. Too few arguments, or {:.2f} applied to a
// string, is a build error instead of a surprise at runtime.
//
// Output looks like "[   0.012] [Info ] message", where the number is seconds since the first
// log line. Trace messages are skipped in Release builds.
class Log {
public:
    enum class Level { Trace, Info, Warn, Error };

    // "= delete" removes the constructor, so nobody can create a Log object. That's the C++
    // way to get a C# static class.
    Log() = delete;

    // These are templates. "typename... Args" is a pack of any number of types, and the
    // compiler generates a separate version of the function for each combination of argument
    // types it's called with. It's the type-safe, zero-overhead cousin of C#'s
    // "params object[] args": no boxing, and every argument's real type is known when compiling.
    template <typename... Args>
    static void Trace(std::format_string<Args...> format, Args&&... args)
    {
        FormatAndWrite<Args...>(Level::Trace, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void Info(std::format_string<Args...> format, Args&&... args)
    {
        FormatAndWrite<Args...>(Level::Info, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void Warn(std::format_string<Args...> format, Args&&... args)
    {
        FormatAndWrite<Args...>(Level::Warn, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void Error(std::format_string<Args...> format, Args&&... args)
    {
        FormatAndWrite<Args...>(Level::Error, format, std::forward<Args>(args)...);
    }

private:
    // Template bodies must live in the header: the compiler needs the full code at every call
    // site to generate the right version. Non-template functions go in Log.cpp as usual.
    template <typename... Args>
    static void FormatAndWrite(Level level, std::format_string<Args...> format, Args&&... args)
    {
        // Check the level first, so skipped messages don't pay for formatting.
        if (IsEnabled(level))
            Write(level, std::format(format, std::forward<Args>(args)...));
    }

    static bool IsEnabled(Level level);
    static void Write(Level level, std::string_view message);
};

} // namespace Viva
