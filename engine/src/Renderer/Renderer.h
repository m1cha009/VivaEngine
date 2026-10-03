#pragma once

#include <memory>

namespace Viva {

class VulkanContext;
class Window;

// The engine's renderer, and the only renderer class the rest of the engine talks to. Its header
// doesn't include vulkan.h (VulkanContext is only declared), so Core code like Application can
// own a Renderer without ever seeing Vulkan.
//
// M2: it sets Vulkan up (VulkanContext). M3 adds the swapchain and draws frames.
class Renderer {
public:
    // Returns nullptr (after logging why) if Vulkan can't be set up on this machine.
    static std::unique_ptr<Renderer> Create(Window& window);

    explicit Renderer(std::unique_ptr<VulkanContext> context);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

private:
    std::unique_ptr<VulkanContext> m_Context;
};

} // namespace Viva
