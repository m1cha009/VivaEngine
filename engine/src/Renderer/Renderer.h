#pragma once

#include "Platform/Window.h"

#include <cstdint>
#include <memory>

// Vulkan's command buffer handle type, declared without vulkan.h (Window.h does the same for the
// instance and surface).
struct VkCommandBuffer_T;

namespace Viva {

struct Camera;
struct DemoScene;
class FrameResources;
class FrameUniforms;
class Image;
class Pipeline;
class Swapchain;
class TextureDescriptors;
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

    // Draws one frame, seen from `camera`, and shows it in the window. Does nothing while the
    // window has no area.
    void DrawFrame(const Camera& camera);

private:
    bool RecreateSwapchain();
    // Records this frame's draw calls, between vkCmdBeginRendering and vkCmdEndRendering. The
    // parameter is a VkCommandBuffer. Until M7 it draws the demo scene; M8 replaces it with the
    // game's own list of draws.
    void RecordDraws(VkCommandBuffer_T* cmd);

    // A reference member: another name for the Window that Application owns, which outlives us.
    // Unlike a pointer it can never be null or point somewhere else later.
    const Window& m_Window;
    bool m_VSync = true;

    // Declared in creation order, so they're destroyed in reverse: meshes, pipeline, swapchain and
    // so on first, the context (with the device they were made from) last.
    std::unique_ptr<VulkanContext> m_Context;
    std::unique_ptr<FrameResources> m_Frames;
    std::unique_ptr<FrameUniforms> m_FrameUniforms;
    std::unique_ptr<TextureDescriptors> m_TextureDescriptors;
    std::unique_ptr<Swapchain> m_Swapchain;
    std::unique_ptr<Image> m_DepthImage; // the swapchain images' size, so it's rebuilt with them
    std::unique_ptr<Pipeline> m_UnlitPipeline;
    // The meshes and textures being drawn, defined in Renderer.cpp. (M8 moves "what to draw" into
    // the game.)
    std::unique_ptr<DemoScene> m_Scene;

    Extent m_SwapchainWindowSize;    // the window's pixel size when the swapchain was built
    bool m_SwapchainOutdated = false; // set when Vulkan reports the swapchain no longer fits
    uint32_t m_FrameIndex = 0;        // which frame in flight is being recorded
};

} // namespace Viva
