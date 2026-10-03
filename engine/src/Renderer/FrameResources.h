#pragma once

#include "Renderer/GpuResource.h"

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace Viva {

// What one frame in flight needs. The CPU records frame N+1 while the GPU is still working on
// frame N, so each of those frames has its own copy of these objects.
struct FrameData {
    // Command buffers hold recorded GPU commands; the pool provides their memory. One pool per
    // frame lets us reset the whole frame's commands in one call.
    VkCommandPool CommandPool = VK_NULL_HANDLE;
    VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
    // Signaled (GPU to GPU) when the acquired swapchain image is ready to be drawn into.
    VkSemaphore ImageAcquired = VK_NULL_HANDLE;
    // Signaled (GPU to CPU) when the GPU has finished this frame's commands, so the CPU knows it
    // can reuse this frame's command buffer and semaphores.
    VkFence InFlight = VK_NULL_HANDLE;
    // Resources the game let go of while this frame was being built. This frame (or an earlier
    // one still in flight) may draw with them, so they're destroyed only after waiting for this
    // frame's fence, the next time this frame slot comes around. (The deferred-deletion queue.)
    std::vector<std::unique_ptr<GpuResource>> ReleasedResources;
};

// The frames in flight, plus the "render finished" semaphores that tell presentation when an
// image is fully drawn. Those can't be per frame: presentation may still be waiting on an image's
// semaphore when the same frame slot comes around again, so there's one per swapchain image.
class FrameResources {
public:
    // Two frames in flight: the CPU can be at most one frame ahead of the GPU. More adds latency
    // (input shows up later on screen) for little gain.
    static constexpr uint32_t kFramesInFlight = 2;

    FrameResources(VkDevice device, uint32_t graphicsQueueFamily);
    ~FrameResources();

    FrameResources(const FrameResources&) = delete;
    FrameResources& operator=(const FrameResources&) = delete;

    FrameData& GetFrame(uint32_t frameIndex) { return m_Frames[frameIndex]; }
    VkSemaphore GetRenderFinished(uint32_t imageIndex) const { return m_RenderFinished[imageIndex]; }

    // Makes sure there's a "render finished" semaphore for every swapchain image. Called whenever
    // the swapchain is created; it only ever adds semaphores, because one that presentation may
    // still be waiting on mustn't be destroyed early.
    void EnsureRenderFinishedSemaphores(uint32_t imageCount);

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    std::array<FrameData, kFramesInFlight> m_Frames {};
    std::vector<VkSemaphore> m_RenderFinished;
};

} // namespace Viva
