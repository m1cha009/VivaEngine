#include "Viva/Log.h"

#include <chrono>
#include <cstdio>
#include <iterator>
#include <mutex>
#include <string>

namespace Viva {

// An anonymous namespace makes everything inside it visible only in this .cpp file, a bit like
// C# 11's "file" access modifier. Other files can't call LevelName, and its name can't clash.
namespace {

std::string_view LevelName(Log::Level level)
{
    switch (level) {
    case Log::Level::Trace: return "Trace";
    case Log::Level::Info:  return "Info";
    case Log::Level::Warn:  return "Warn";
    case Log::Level::Error: return "Error";
    }
    return "?";
}

} // namespace

void Log::Write(Level level, std::string_view format, std::format_args args)
{
    // A static local variable is initialized once, the first time execution reaches it (and
    // C++ guarantees that's thread-safe). So this is the time of the first log line, a rough
    // equivalent of Unity's Time.realtimeSinceStartup.
    static const auto startTime = std::chrono::steady_clock::now();
    const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - startTime;

    // Build the whole line first: the prefix, then the message formatted straight onto its end.
    std::string line = std::format("[{:8.3f}] [{:<5}] ", elapsed.count(), LevelName(level));
    std::vformat_to(std::back_inserter(line), format, args);
    line += '\n';

    // Warnings and errors go to stderr, which CLion's console shows in red.
    std::FILE* stream = level >= Level::Warn ? stderr : stdout;

    // Several threads may log at once (from M2 on, Vulkan's validation callback can run on any
    // thread). The lock makes each line come out whole instead of interleaved with others.
    // std::lock_guard is RAII: it locks in its constructor and unlocks in its destructor, at the
    // closing brace, like a C# "lock" block that can't be forgotten.
    static std::mutex mutex;
    const std::lock_guard lock(mutex);
    std::fputs(line.c_str(), stream);
    // Flush right away, so the last lines before a crash aren't stuck in a buffer.
    std::fflush(stream);
}

} // namespace Viva
