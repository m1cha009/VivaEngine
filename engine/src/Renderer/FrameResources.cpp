#include "Renderer/FrameResources.h"

#include "Renderer/VulkanHelpers.h"

namespace Viva {

FrameResources::FrameResources(VkDevice device, uint32_t graphicsQueueFamily)
    : m_Device(device)
{
    for (FrameData& frame : m_Frames) {
        // The frame's command buffer is re-recorded every frame and submitted to the graphics queue.
        frame.CommandPool = CreateCommandPool(device, graphicsQueueFamily);
        frame.CommandBuffer = AllocateCommandBuffer(device, frame.CommandPool);
        frame.ImageAcquired = CreateBinarySemaphore(device);
        // Created already signaled, so the very first wait on it returns immediately.
        frame.InFlight = CreateFence(device, true);
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
