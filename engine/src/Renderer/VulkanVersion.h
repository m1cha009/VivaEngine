#pragma once

#include "Viva/Version.h"

namespace Viva {

// The vulkan.h version the engine was compiled against (from the installed Vulkan SDK).
Version GetVulkanHeaderVersion();

// The newest Vulkan version the Vulkan loader on this machine understands.
Version GetVulkanLoaderVersion();

} // namespace Viva
