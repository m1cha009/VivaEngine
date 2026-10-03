#include "Renderer/FrameResources.h"

#include "Renderer/VulkanCheck.h"

namespace Viva {

namespace {

VkSemaphore CreateBinarySemaphore(VkDevice device)
{
    const VkSemaphoreCreateInfo info { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkSemaphore semaphore = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSemaphore(device, &info, nullptr, &semaphore));
    return semaphore;
}

} // namespace

FrameResources::FrameResources(VkDevice device, uint32_t graphicsQueueFamily)
    : m_Device(device)
{
    for (FrameData& frame : m_Frames) {
        const VkCommandPoolCreateInfo poolInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, // its buffers are re-recorded every frame
            .queueFamilyIndex = graphicsQueueFamily,       // they'll be submitted to the graphics queue
        };
        VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &frame.CommandPool));

        const VkCommandBufferAllocateInfo bufferInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = frame.CommandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        VK_CHECK(vkAllocateCommandBuffers(device, &bufferInfo, &frame.CommandBuffer));

        frame.ImageAcquired = CreateBinarySemaphore(device);

        // Created already signaled, so the very first wait on it returns immediately.
        const VkFenceCreateInfo fenceInfo { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT };
        VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &frame.InFlight));
    }
}

FrameResources::~FrameResources()
{
    for (VkSemaphore semaphore : m_RenderFinished)
        vkDestroySemaphore(m_Device, semaphore, nullptr);
    for (FrameData& frame : m_Frames) {
        vkDestroyFence(m_Device, frame.InFlight, nullptr);
        vkDestroySemaphore(m_Device, frame.ImageAcquired, nullptr);
        vkDestroyCommandPool(m_Device, frame.CommandPool, nullptr); // also frees its command buffer
    }
}

void FrameResources::EnsureRenderFinishedSemaphores(uint32_t imageCount)
{
    while (m_RenderFinished.size() < imageCount)
        m_RenderFinished.push_back(CreateBinarySemaphore(m_Device));
}

} // namespace Viva
