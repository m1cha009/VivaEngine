#pragma once

#include <cstdint>
#include <string>

namespace Viva {

// A version number in the usual major.minor.patch form, like C#'s System.Version.
//
// In C++, "struct" and "class" are nearly identical. The only difference is that struct members
// are public by default. By convention, a struct is a plain bundle of data like this one.
struct Version {
    uint32_t Major = 0;
    uint32_t Minor = 0;
    uint32_t Patch = 0;

    // "const" after the parameter list promises this function doesn't modify the Version,
    // like a readonly member of a C# struct.
    std::string ToString() const;
};

// The engine's version, from project(VERSION ...) in the root CMakeLists.txt. The versions of SDL
// and Vulkan are engine internals (src/Platform/SDLVersion.h and src/Renderer/VulkanVersion.h);
// the engine logs them when it starts.
Version GetEngineVersion();

} // namespace Viva
