#pragma once

#include <vulkan/vulkan.h>

#include <source_location>
#include <string>

namespace Viva {

// Names of Vulkan enum values, such as "VK_ERROR_OUT_OF_DEVICE_MEMORY", for log messages. (They
// wrap the SDK's string_Vk... functions, so only VulkanCheck.cpp includes that very large header.)
const char* VkResultName(VkResult result);
const char* VkFormatName(VkFormat format);
const char* VkPresentModeName(VkPresentModeKHR mode);
// Flags give every bit that's set, joined by "|".
std::string VkMemoryPropertyNames(VkMemoryPropertyFlags flags);

namespace Detail {

// Called by VK_CHECK when a Vulkan call fails: logs the call, the name of its VkResult and where
// it happened, then stops the program (see BreakIntoDebuggerOrExit).
[[noreturn]] void VulkanCallFailed(VkResult result, const char* call, std::source_location location);

} // namespace Detail

} // namespace Viva

// VK_CHECK(vkSomething(...));
//
// Most Vulkan functions return a VkResult. VK_CHECK runs the call and treats any result other
// than VK_SUCCESS as fatal, in every build: it reports the call's text, file and line and stops.
// Use it for calls that only fail when something is badly wrong (out of memory, a lost device),
// where carrying on would just crash somewhere else later. Failures that can happen on a healthy
// machine, like a GPU without a feature we need, are checked and handled explicitly instead.
#define VK_CHECK(call)                                                                              \
    do {                                                                                            \
        const VkResult vkCheckResult = (call);                                                      \
        if (vkCheckResult != VK_SUCCESS)                                                            \
            ::Viva::Detail::VulkanCallFailed(vkCheckResult, #call, std::source_location::current()); \
    } while (false)
