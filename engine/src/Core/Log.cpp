#include "Viva/Log.h"

#include <chrono>
#include <cstdio>
#include <iterator>
#include <mutex>
#include <string>
#include <utility>

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

// Several threads may log at once (from M2 on, Vulkan's validation callback can run on any
// thread). This lock makes each line come out whole instead of interleaved with others, and keeps
// the listener from being replaced while it's called. std::lock_guard is RAII: it locks in its
// constructor and unlocks in its destructor, at the closing brace, like a C# "lock" block that
// can't be forgotten.
std::mutex s_Mutex;
Log::Listener s_Listener;

} // namespace

void Log::SetListener(Listener listener)
{
    const std::lock_guard lock(s_Mutex);
    s_Listener = std::move(listener);
}

void Log::Write(Level level, std::string_view format, std::format_args args)
{
    // A static local variable is initialized once, the first time execution reaches it (and
    // C++ guarantees that's thread-safe). So this is the time of the first log line, a rough
    // equivalent of Unity's Time.realtimeSinceStartup.
    static const auto startTime = std::chrono::steady_clock::now();
    const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - startTime;

    // Build the whole line: the prefix, then the message formatted straight onto its end (the
    // listener gets that part on its own), then a line break.
    std::string line = std::format("[{:8.3f}] [{:<5}] ", elapsed.count(), LevelName(level));
    const size_t prefixSize = line.size();
    std::vformat_to(std::back_inserter(line), format, args);
    line += '\n';

    // Warnings and errors go to stderr, which CLion's console shows in red.
    std::FILE* stream = level >= Level::Warn ? stderr : stdout;

    const std::lock_guard lock(s_Mutex);
    std::fputs(line.c_str(), stream);
    // Flush right away, so the last lines before a crash aren't stuck in a buffer.
    std::fflush(stream);
    if (s_Listener)
        s_Listener(level, elapsed.count(), std::string_view(line).substr(prefixSize, line.size() - prefixSize - 1));
}

} // namespace Viva
