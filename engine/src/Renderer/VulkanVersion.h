#pragma once

#include "Viva/Version.h"

#include <cstdint>

namespace Viva {

// The vulkan.h version the engine was compiled against (from the installed Vulkan SDK).
Version GetVulkanHeaderVersion();

// The newest Vulkan version the Vulkan loader on this machine understands.
Version GetVulkanLoaderVersion();

// Unpacks a version number in Vulkan's format (variant, major, minor and patch packed into one
// uint32_t), as found in VkPhysicalDeviceProperties::apiVersion and friends.
Version FromVulkanVersion(uint32_t version);

} // namespace Viva
