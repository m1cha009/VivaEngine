#include "Viva/Assert.h"

#include "Platform/Debugger.h"
#include "Viva/Log.h"

#include <string>

namespace Viva::Detail {

void AssertFailed(const char* expression, std::source_location location)
{
    // An empty format string and no arguments: there's no message.
    AssertFailedWithMessage(expression, location, {}, std::make_format_args());
}

void AssertFailedWithMessage(const char* expression, std::source_location location,
                             std::string_view format, std::format_args args)
{
    const std::string message = std::vformat(format, args);
    if (message.empty())
        Log::Error("Assertion failed: {}", expression);
    else
        Log::Error("Assertion failed: {} ({})", expression, message);
    Log::Error("    at {}:{}", location.file_name(), location.line());

    BreakIntoDebuggerOrExit();
}

} // namespace Viva::Detail
