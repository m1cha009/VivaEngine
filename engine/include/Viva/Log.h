#pragma once

#include <format>
#include <functional>
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
// log line. Trace messages are dropped in Release builds, but their arguments are still
// evaluated, so keep those cheap.
class Log {
public:
    enum class Level { Trace, Info, Warn, Error };

    // Messages below this level are skipped.
#if defined(VIVA_DEBUG)
    static constexpr Level kMinLevel = Level::Trace;
#else
    static constexpr Level kMinLevel = Level::Info;
#endif

    // "= delete" removes the constructor, so nobody can create a Log object. That's the C++
    // way to get a C# static class.
    Log() = delete;

    // Logs at a level that's only known at runtime, such as a Vulkan message severity.
    //
    // This is a template. "typename... Args" is a pack of any number of types, and the compiler
    // generates a separate version of the function for each combination of argument types it's
    // called with. It's the type-safe, zero-overhead cousin of C#'s "params object[] args": no
    // boxing, and every argument's real type is known when compiling. Template bodies must
    // live in the header, because the compiler needs the full code at every call site.
    template <typename... Args>
    static void Message(Level level, std::format_string<Args...> format, Args&&... args)
    {
        // The template part stays tiny: it checks the level and packs references to the
        // arguments into std::format_args. The formatting code itself is compiled once, in
        // Log.cpp, instead of again in every file that logs.
        if (level >= kMinLevel)
            Write(level, format.get(), std::make_format_args(args...));
    }

    template <typename... Args>
    static void Trace(std::format_string<Args...> format, Args&&... args)
    {
        Message<Args...>(Level::Trace, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void Info(std::format_string<Args...> format, Args&&... args)
    {
        Message<Args...>(Level::Info, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void Warn(std::format_string<Args...> format, Args&&... args)
    {
        Message<Args...>(Level::Warn, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void Error(std::format_string<Args...> format, Args&&... args)
    {
        Message<Args...>(Level::Error, format, std::forward<Args>(args)...);
    }

    // Something that wants every message too, like the editor's Console window. It gets the
    // level, the seconds since the first message (as in the printed line), and the message without
    // that prefix. There's one listener at a time; setting another replaces it, and nullptr removes
    // it. It's called on the thread that logs (nearly always the main one) while other threads'
    // messages wait, so it must be quick, and mustn't log itself.
    using Listener = std::function<void(Level level, double seconds, std::string_view message)>;
    static void SetListener(Listener listener);

private:
    static void Write(Level level, std::string_view format, std::format_args args);
};

} // namespace Viva
