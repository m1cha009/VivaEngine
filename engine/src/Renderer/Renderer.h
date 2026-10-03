#pragma once

#include "Platform/Window.h"

#include <cstdint>
#include <memory>

namespace Viva {

class FrameResources;
class Swapchain;
class VulkanContext;

// The engine's renderer, and the only renderer class the rest of the engine talks to. Its header
// doesn't include vulkan.h (the Vulkan classes are only declared), so Core code like Application
// can own a Renderer without ever seeing Vulkan.
class Renderer {
public:
    // Returns nullptr (after logging why) if Vulkan can't be set up on this machine.
    // vsync: wait for the display's refresh between frames, rather than drawing as fast as possible.
    static std::unique_ptr<Renderer> Create(const Window& window, bool vsync);

    Renderer(const Window& window, bool vsync); // creates nothing: use Create()
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Draws one frame and shows it in the window. Does nothing while the window has no area.
    void DrawFrame();

private:
    bool RecreateSwapchain();

    // A reference member: another name for the Window that Application owns, which outlives us.
    // Unlike a pointer it can never be null or point somewhere else later.
    const Window& m_Window;
    bool m_VSync = true;

    // Declared in creation order, so they're destroyed in reverse: swapchain and frames first,
    // the context (with the device they were made from) last.
    std::unique_ptr<VulkanContext> m_Context;
    std::unique_ptr<FrameResources> m_Frames;
    std::unique_ptr<Swapchain> m_Swapchain;

    Extent m_SwapchainWindowSize;    // the window's pixel size when the swapchain was built
    bool m_SwapchainOutdated = false; // set when Vulkan reports the swapchain no longer fits
    uint32_t m_FrameIndex = 0;        // which frame in flight is being recorded
};

} // namespace Viva
