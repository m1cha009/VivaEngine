#include "Renderer/Image.h"

#include "Renderer/VulkanCheck.h"
#include "Renderer/VulkanContext.h"
#include "Renderer/VulkanHelpers.h"

namespace Viva {

std::unique_ptr<Image> Image::Create(const VulkanContext& context, const ImageSettings& settings)
{
    auto image = std::make_unique<Image>(context.GetDevice(), context.GetAllocator());

    const VkImageCreateInfo imageInfo {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = settings.Format,
        .extent = { settings.Extent.width, settings.Extent.height, 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL, // the GPU's own layout: fastest, but not readable as rows
        .usage = settings.Usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED, // contents start undefined; a barrier sets a layout
    };
    // Like a Gpu buffer: only the GPU touches it. (For big render targets, VMA follows the driver's
    // advice and may give the image a memory block of its own.)
    const VmaAllocationCreateInfo allocationInfo { .usage = VMA_MEMORY_USAGE_AUTO };
    VK_CHECK(vmaCreateImage(image->m_Allocator, &imageInfo, &allocationInfo, &image->m_Image, &image->m_Allocation,
                            nullptr));

    image->m_View = CreateImageView(image->m_Device, image->m_Image, settings.Format, settings.Aspect);
    return image;
}

Image::Image(VkDevice device, VmaAllocator allocator)
    : m_Device(device)
    , m_Allocator(allocator)
{
}

Image::~Image()
{
    // The view first: it refers to the image.
    vkDestroyImageView(m_Device, m_View, nullptr);
    vmaDestroyImage(m_Allocator, m_Image, m_Allocation);
}

} // namespace Viva
