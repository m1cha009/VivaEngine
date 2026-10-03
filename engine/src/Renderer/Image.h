#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <memory>

namespace Viva {

class VulkanContext;

// What to create an image from. Fill it with designated initializers, like PipelineSettings.
struct ImageSettings {
    VkExtent2D Extent {};
    VkFormat Format = VK_FORMAT_UNDEFINED;
    // What the image will be used for: drawn into as a depth buffer, sampled as a texture (M7)...
    VkImageUsageFlags Usage = 0;
    // Which part of the image its view shows: color, or depth for a depth buffer.
    VkImageAspectFlags Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
};

// A 2D image in GPU memory, plus the view that rendering and shaders use to access it. The depth
// buffer is one (M6), textures are others (M7). In Unity terms it's a RenderTexture or Texture2D.
//
//   VkImage      The pixels, in a GPU-specific memory layout ("optimal tiling").
//   VkImageView  How to look at them: which aspect, which mip levels. Rendering and shaders always
//                go through a view, never the image itself.
class Image {
public:
    static std::unique_ptr<Image> Create(const VulkanContext& context, const ImageSettings& settings);

    Image(VkDevice device, VmaAllocator allocator); // creates nothing: use Create()
    ~Image();

    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    VkImage GetHandle() const { return m_Image; }
    VkImageView GetView() const { return m_View; }

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    VmaAllocator m_Allocator = VK_NULL_HANDLE;
    VkImage m_Image = VK_NULL_HANDLE;
    VmaAllocation m_Allocation = VK_NULL_HANDLE;
    VkImageView m_View = VK_NULL_HANDLE;
};

} // namespace Viva
