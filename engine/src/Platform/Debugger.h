#pragma once

namespace Viva {

// Called after a fatal error (like a failed VIVA_ASSERT) has been logged.
//
// With a debugger attached (CLion, Visual Studio, Xcode...), execution stops right here, so you
// can inspect the call stack and variables; the frames below lead back to the failure. Without
// a debugger, the program ends immediately with exit code 1.
[[noreturn]] void BreakIntoDebuggerOrExit();

} // namespace Viva
