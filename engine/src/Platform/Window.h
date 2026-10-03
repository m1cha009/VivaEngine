#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <vector>

// SDL's types, declared here without including SDL's headers. This header is included by Core
// and Renderer code, which must not see SDL, and declarations are all it needs.
struct SDL_Window;
union SDL_Event;

// Vulkan's handle types, declared the same way so Platform code never needs vulkan.h. On 64-bit
// systems vulkan.h defines VkInstance as "VkInstance_T*" and VkSurfaceKHR as "VkSurfaceKHR_T*",
// so these are exactly the types the renderer passes in.
struct VkInstance_T;
struct VkSurfaceKHR_T;

namespace Viva {

// The size of something in pixels.
struct Extent {
    uint32_t Width = 0;
    uint32_t Height = 0;
};

// The game window: a thin wrapper around an SDL window. It also owns SDL itself, which is
// initialized with the window and shut down with it (the engine has exactly one window).
class Window {
public:
    // Opens the window. Returns nullptr if that fails. Without exceptions a constructor has no
    // way to report failure, so creation goes through this "factory" function instead.
    static std::unique_ptr<Window> Create(const std::string& title, uint32_t width, uint32_t height);

    // Takes ownership of an SDL window. Use Create() rather than calling this directly.
    explicit Window(SDL_Window* window);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // Handles every pending OS event: quit and resize here, keyboard and mouse in Input.
    void PollEvents();
    // Sleeps until an event arrives, leaving it queued for the next PollEvents().
    void WaitForEvent() const;

    bool ShouldClose() const { return m_ShouldClose; }
    bool IsMinimized() const;

    // The drawable size in pixels. On a high-DPI (Retina) screen this is larger than the size in
    // points the window was created with, and it's the size the renderer must use.
    Extent GetPixelSize() const;

    void SetTitle(const std::string& title);

    // The Vulkan instance extensions this OS needs for window surfaces, as SDL reports them
    // (VK_KHR_surface plus VK_KHR_win32_surface on Windows, VK_EXT_metal_surface on macOS).
    static std::vector<const char*> GetRequiredVulkanExtensions();

    // Creates the Vulkan surface for this window, or returns null after logging why. The caller
    // owns it and destroys it with vkDestroySurfaceKHR.
    VkSurfaceKHR_T* CreateVulkanSurface(VkInstance_T* instance) const;

private:
    void HandleEvent(const SDL_Event& event);

    SDL_Window* m_Window = nullptr;
    bool m_ShouldClose = false;
};

} // namespace Viva
