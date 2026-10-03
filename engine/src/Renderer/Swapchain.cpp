#include "Renderer/Swapchain.h"

#include "Renderer/VulkanCheck.h"
#include "Renderer/VulkanContext.h"
#include "Renderer/VulkanHelpers.h"
#include "Viva/Log.h"


#include <algorithm>

namespace Viva {

namespace {

// The pixel format and color space of the swapchain images. We prefer 8-bit color in sRGB: the
// GPU then converts the linear colors we write into sRGB, which is what monitors expect. (That's
// what Unity's "Linear" color space setting does behind the scenes.)
VkSurfaceFormatKHR ChooseSurfaceFormat(VkPhysicalDevice gpu, VkSurfaceKHR surface)
{
    uint32_t count = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &count, nullptr));
    std::vector<VkSurfaceFormatKHR> formats(count);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &count, formats.data()));

    for (VkFormat preferred : { VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB }) {
        for (const VkSurfaceFormatKHR& format : formats) {
            if (format.format == preferred && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                return format;
        }
    }

    // None of our favorites: take what the surface lists first. Everything we draw assumes an sRGB
    // swapchain (shaders write linear colors, ImGui's shader decodes its colors), so say so.
    Log::Warn("The surface offers no 8-bit sRGB format, using {}: colors may look wrong",
              VkFormatName(formats[0].format));
    return formats[0];
}

// How finished images reach the screen:
//   FIFO       Images wait in a queue and the display takes one per refresh: vsync. Always
//              available. The frame rate matches the monitor, and the CPU waits instead of
//              spinning.
//   MAILBOX    Like FIFO, but a newer image replaces one that's still waiting. No tearing and low
//              latency, but the GPU renders as fast as it can and throws most frames away.
//   IMMEDIATE  Shown right away, even halfway through a refresh: visible tearing.
VkPresentModeKHR ChoosePresentMode(VkPhysicalDevice gpu, VkSurfaceKHR surface, bool vsync)
{
    if (vsync)
        return VK_PRESENT_MODE_FIFO_KHR;

    uint32_t count = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &count, nullptr));
    std::vector<VkPresentModeKHR> modes(count);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &count, modes.data()));

    for (VkPresentModeKHR preferred : { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR }) {
        if (std::ranges::find(modes, preferred) != modes.end())
            return preferred;
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

// How the window's pixels combine with what's behind it. We want an opaque window; a few
// platforms only offer one of the other modes, so take the first one supported.
VkCompositeAlphaFlagBitsKHR ChooseCompositeAlpha(VkCompositeAlphaFlagsKHR supported)
{
    for (VkCompositeAlphaFlagBitsKHR mode : { VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
                                              VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
                                              VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR }) {
        if (supported & mode)
            return mode;
    }
    return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
}

} // namespace

std::unique_ptr<Swapchain> Swapchain::Create(const VulkanContext& context, VkExtent2D size, bool vsync,
                                             VkSwapchainKHR oldSwapchain)
{
    VkPhysicalDevice gpu = context.GetPhysicalDevice();
    VkSurfaceKHR surface = context.GetSurface();
    VkDevice device = context.GetDevice();

    VkSurfaceCapabilitiesKHR capabilities {};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &capabilities));

    // The surface usually dictates the size (currentExtent). The special value 0xFFFFFFFF means
    // "your choice", and then we use the window's pixel size, kept within the allowed range.
    VkExtent2D extent = capabilities.currentExtent;
    if (extent.width == UINT32_MAX) {
        extent.width = std::clamp(size.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = std::clamp(size.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }

    // A swapchain can't be 0x0. The surface reports that while the window is being minimized,
    // which can happen a moment before the window itself says so. The caller tries again later.
    if (extent.width == 0 || extent.height == 0)
        return nullptr;

    // One image more than the minimum, so the driver never has to wait for us to hand one back.
    // A maxImageCount of 0 means "no limit".
    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0)
        imageCount = std::min(imageCount, capabilities.maxImageCount);

    const VkSurfaceFormatKHR format = ChooseSurfaceFormat(gpu, surface);
    const VkPresentModeKHR presentMode = ChoosePresentMode(gpu, surface, vsync);

    // If drawing and presenting happen in different queue families, both must be allowed to use
    // the images (CONCURRENT). Usually it's a single family, and EXCLUSIVE is the faster mode.
    const uint32_t families[] = { context.GetGraphicsQueueFamily(), context.GetPresentQueueFamily() };
    const bool oneFamily = families[0] == families[1];

    const VkSwapchainCreateInfoKHR createInfo {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = imageCount,
        .imageFormat = format.format,
        .imageColorSpace = format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, // we render into these images directly
        .imageSharingMode = oneFamily ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT,
        .queueFamilyIndexCount = oneFamily ? 0u : 2u,
        .pQueueFamilyIndices = oneFamily ? nullptr : families,
        .preTransform = capabilities.currentTransform, // no rotation (matters on phones)
        .compositeAlpha = ChooseCompositeAlpha(capabilities.supportedCompositeAlpha),
        .presentMode = presentMode,
        .clipped = VK_TRUE, // pixels hidden behind other windows needn't be correct
        .oldSwapchain = oldSwapchain,
    };

    auto swapchain = std::make_unique<Swapchain>(device);
    const VkResult result = vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain->m_Swapchain);
    if (result != VK_SUCCESS) {
        Log::Error("Couldn't create the swapchain: {}", VkResultName(result));
        return nullptr;
    }
    swapchain->m_Format = format.format;
    swapchain->m_Extent = extent;

    // The driver may create more images than we asked for, so ask how many there are.
    uint32_t count = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain->m_Swapchain, &count, nullptr));
    swapchain->m_Images.resize(count);
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain->m_Swapchain, &count, swapchain->m_Images.data()));

    // Rendering draws into an image through a view: here, each image's color data, whole.
    for (VkImage image : swapchain->m_Images)
        swapchain->m_ImageViews.push_back(CreateImageView(device, image, format.format, VK_IMAGE_ASPECT_COLOR_BIT));

    Log::Info("Swapchain: {}x{} pixels, {} images, {}, {}", extent.width, extent.height, count,
              VkFormatName(format.format), VkPresentModeName(presentMode));
    return swapchain;
}

Swapchain::Swapchain(VkDevice device)
    : m_Device(device)
{
}

Swapchain::~Swapchain()
{
    for (VkImageView view : m_ImageViews)
        vkDestroyImageView(m_Device, view, nullptr);
    if (m_Swapchain)
        vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
}

} // namespace Viva
