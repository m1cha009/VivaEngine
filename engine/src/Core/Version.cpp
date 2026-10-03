#include "Viva/Version.h"

#include <format>

namespace Viva {

std::string Version::ToString() const
{
    return std::format("{}.{}.{}", Major, Minor, Patch);
}

Version GetEngineVersion()
{
    // The VIVA_VERSION_* macros are passed in by CMake as compiler flags
    // (see engine/CMakeLists.txt).
    return { VIVA_VERSION_MAJOR, VIVA_VERSION_MINOR, VIVA_VERSION_PATCH };
}

} // namespace Viva
