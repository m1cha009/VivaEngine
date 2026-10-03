#include "Viva/Version.h"

#include <vulkan/vulkan.h>

namespace Viva {

namespace {

// Vulkan packs a version into one uint32_t (variant, major, minor, patch bit fields).
Version FromVulkanVersion(uint32_t version)
{
    return { VK_API_VERSION_MAJOR(version), VK_API_VERSION_MINOR(version), VK_API_VERSION_PATCH(version) };
}

} // namespace

Version GetVulkanHeaderVersion()
{
    // A compile-time constant from vulkan.h: the version of the API headers this engine was
    // built with, which comes from the installed Vulkan SDK.
    return FromVulkanVersion(VK_HEADER_VERSION_COMPLETE);
}

Version GetVulkanLoaderVersion()
{
    // A runtime question answered by the Vulkan loader. On Windows that's vulkan-1.dll, which
    // normally comes with the GPU driver. On macOS it's libvulkan.1.dylib from the SDK. The
    // result is the newest Vulkan version the loader understands. What the GPU itself supports
    // is a per-device question we'll ask in M2.
    uint32_t version = 0;
    if (vkEnumerateInstanceVersion(&version) != VK_SUCCESS)
        return { 1, 0, 0 }; // Only fails when out of memory. 1.0 is the safe assumption.
    return FromVulkanVersion(version);
}

} // namespace Viva
