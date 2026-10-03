// The sandbox: a small test program that uses the engine the way a game would.
// It includes only engine headers (Viva/...) and GLM, never SDL or Vulkan.

#include "Viva/Assert.h"
#include "Viva/Log.h"
#include "Viva/Version.h"

#include <glm/glm.hpp>

#include <string_view>

int main(int argc, char* argv[])
{
    // Like "using Viva;" in C#: lets us write Log::Info instead of Viva::Log::Info.
    using namespace Viva;

    Log::Info("VivaEngine {}", GetEngineVersion().ToString());
    Log::Info("SDL {}", GetSDLVersion().ToString());
    Log::Info("Vulkan headers {}", GetVulkanHeaderVersion().ToString());
    Log::Info("Vulkan loader {}", GetVulkanLoaderVersion().ToString());
    // GLM is header-only and part of the engine's public API, so the sandbox uses it directly.
    Log::Info("GLM {}", Version{ GLM_VERSION_MAJOR, GLM_VERSION_MINOR, GLM_VERSION_PATCH }.ToString());
    Log::Trace("Trace messages like this one only appear in Debug builds");

    // "Sandbox --test-assert" shows what a failed assertion looks like (Debug builds only:
    // in Release, VIVA_ASSERT compiles to nothing and this flag has no effect).
    const bool failOnPurpose = argc > 1 && std::string_view(argv[1]) == "--test-assert";
    VIVA_ASSERT(!failOnPurpose, "failing on purpose because of {}", argv[1]);

    return 0;
}
