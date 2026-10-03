#include "Viva/Assert.h"

#include "Platform/Debugger.h"
#include "Viva/Log.h"

namespace Viva::Detail {

void AssertFailed(const char* expression, const char* file, int line, std::string_view message)
{
    if (message.empty())
        Log::Error("Assertion failed: {}", expression);
    else
        Log::Error("Assertion failed: {} ({})", expression, message);
    Log::Error("    at {}:{}", file, line);

    BreakIntoDebuggerOrExit();
}

} // namespace Viva::Detail
