#include "Platform/Window.h"

#include "Platform/InputEvents.h"
#include "Viva/Log.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>
// SDL's Vulkan helpers. Without vulkan.h included, this header defines VkInstance and VkSurfaceKHR
// itself, as pointers to the same structs Window.h declares.
#include <SDL3/SDL_vulkan.h>

namespace Viva {

std::unique_ptr<Window> Window::Create(const std::string& title, uint32_t width, uint32_t height, uint32_t display)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        Log::Error("SDL_Init failed: {}", SDL_GetError());
        return nullptr;
    }

    // VULKAN: the window will be rendered with Vulkan (from M2 on).
    // RESIZABLE: the user can drag its borders.
    // HIGH_PIXEL_DENSITY: on Retina and other high-DPI screens, get the full pixel resolution.
    // Without it, macOS gives us a quarter of the pixels and scales them up, which looks blurry.
    // HIDDEN: created invisible, so it can be moved to its monitor before it first appears.
    const SDL_WindowFlags flags =
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN;
    SDL_Window* sdlWindow = SDL_CreateWindow(title.c_str(), static_cast<int>(width), static_cast<int>(height), flags);
    if (!sdlWindow) {
        Log::Error("SDL_CreateWindow failed: {}", SDL_GetError());
        SDL_Quit();
        return nullptr;
    }

    // Monitors are identified by SDL_DisplayID numbers; SDL_GetDisplays lists them in the OS's
    // order. The list is SDL's memory on loan to us, so SDL_free hands it back.
    int displayCount = 0;
    SDL_DisplayID* displays = SDL_GetDisplays(&displayCount);
    SDL_DisplayID displayId = SDL_GetPrimaryDisplay();
    if (displays && display < static_cast<uint32_t>(displayCount))
        displayId = displays[display];
    else if (display != 0)
        Log::Warn("There's no display {}, using the main one", display);
    SDL_free(displays);

    const int centered = static_cast<int>(SDL_WINDOWPOS_CENTERED_DISPLAY(displayId));
    SDL_SetWindowPosition(sdlWindow, centered, centered);
    SDL_ShowWindow(sdlWindow);

    auto window = std::make_unique<Window>(sdlWindow);
    const Extent pixels = window->GetPixelSize();
    Log::Info("Window: {}x{} points, {}x{} pixels (display scale {:.2f}) on {}", width, height, pixels.Width,
              pixels.Height, SDL_GetWindowDisplayScale(sdlWindow), SDL_GetDisplayName(displayId));
    return window;
}

Window::Window(SDL_Window* window)
    : m_Window(window)
{
}

// The destructor is the C++ counterpart of C#'s Dispose() or Unity's OnDestroy(), except that it
// runs automatically when the object is destroyed (here: when Application resets its unique_ptr).
Window::~Window()
{
    SDL_DestroyWindow(m_Window);
    SDL_Quit();
}

void Window::PollEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
        HandleEvent(event);
}

void Window::WaitForEvent() const
{
    // Given nullptr instead of an event to fill in, SDL blocks until an event arrives but leaves
    // it in the queue, so the next PollEvents() handles it like any other.
    SDL_WaitEvent(nullptr);
}

bool Window::IsMinimized() const
{
    // SDL keeps the window's state as a set of flags; MINIMIZED is one of them.
    return (SDL_GetWindowFlags(m_Window) & SDL_WINDOW_MINIMIZED) != 0;
}

Extent Window::GetPixelSize() const
{
    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(m_Window, &width, &height);
    return { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
}

void Window::SetTitle(const std::string& title)
{
    SDL_SetWindowTitle(m_Window, title.c_str());
}

std::vector<const char*> Window::GetRequiredVulkanExtensions()
{
    Uint32 count = 0;
    const char* const* names = SDL_Vulkan_GetInstanceExtensions(&count);
    if (!names) {
        Log::Error("SDL_Vulkan_GetInstanceExtensions failed: {}", SDL_GetError());
        return {};
    }
    // A vector built from a range: the "begin" pointer and one past the last element.
    return std::vector<const char*>(names, names + count);
}

VkSurfaceKHR_T* Window::CreateVulkanSurface(VkInstance_T* instance) const
{
    VkSurfaceKHR surface = nullptr;
    if (!SDL_Vulkan_CreateSurface(m_Window, instance, nullptr, &surface)) {
        Log::Error("SDL_Vulkan_CreateSurface failed: {}", SDL_GetError());
        return nullptr;
    }
    return surface;
}

void Window::HandleEvent(const SDL_Event& event)
{
    switch (event.type) {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        m_ShouldClose = true;
        break;

    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        Log::Trace("Window resized to {}x{} pixels", event.window.data1, event.window.data2);
        break;

    default:
        // Keyboard and mouse events (Input ignores everything else).
        ProcessInputEvent(event);
        break;
    }
}

} // namespace Viva
