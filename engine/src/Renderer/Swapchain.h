#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace Viva {

class VulkanContext;

// The swapchain: a small set of images the window shows in turn. The renderer draws into one
// image while the screen displays another; "presenting" hands a finished image to the display,
// and "acquiring" asks for the next free one. A Unity developer never sees this, because Unity
// manages the back buffers itself.
//
// A swapchain has a fixed size and format, so it's thrown away and rebuilt when the window is
// resized (Create's oldSwapchain parameter lets the driver reuse what it can).
class Swapchain {
public:
    // Builds a swapchain for the context's surface. size is the window's size in pixels; vsync
    // picks the present mode (see ChoosePresentMode in the .cpp). Returns nullptr if the surface
    // has no area right now (a minimized window), or on failure (logged).
    static std::unique_ptr<Swapchain> Create(const VulkanContext& context, VkExtent2D size, bool vsync,
                                             VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE);

    explicit Swapchain(VkDevice device);
    ~Swapchain();

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    VkSwapchainKHR GetHandle() const { return m_Swapchain; }
    VkFormat GetFormat() const { return m_Format; }
    VkExtent2D GetExtent() const { return m_Extent; }
    uint32_t GetImageCount() const { return static_cast<uint32_t>(m_Images.size()); }
    VkImage GetImage(uint32_t index) const { return m_Images[index]; }
    VkImageView GetImageView(uint32_t index) const { return m_ImageViews[index]; }

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
    VkFormat m_Format = VK_FORMAT_UNDEFINED;
    VkExtent2D m_Extent {};
    // The images belong to the swapchain (destroying it frees them). The image views are ours:
    // a view says how to look at an image (its format and which part), and rendering needs one.
    std::vector<VkImage> m_Images;
    std::vector<VkImageView> m_ImageViews;
};

} // namespace Viva
