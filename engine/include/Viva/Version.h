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

// Versions of the engine and of the libraries it was built with.
//
// They're declared together in this one header, but each is defined in the engine folder that's
// allowed to use that library. SDL code stays in src/Platform/, Vulkan code in src/Renderer/,
// and the game only ever sees these declarations, never an SDL or Vulkan header. The linker
// connects each declaration to its definition when it builds the executable.
Version GetEngineVersion();       // src/Core/Version.cpp
Version GetSDLVersion();          // src/Platform/SDLVersion.cpp
Version GetVulkanHeaderVersion(); // src/Renderer/VulkanVersion.cpp: the vulkan.h we compiled against
Version GetVulkanLoaderVersion(); // src/Renderer/VulkanVersion.cpp: the Vulkan runtime on this machine

} // namespace Viva
