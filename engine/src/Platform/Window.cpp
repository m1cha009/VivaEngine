#include "Platform/Window.h"

#include "Platform/InputEvents.h"
#include "Viva/Log.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>

namespace Viva {

std::unique_ptr<Window> Window::Create(const std::string& title, uint32_t width, uint32_t height)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        Log::Error("SDL_Init failed: {}", SDL_GetError());
        return nullptr;
    }

    // VULKAN: the window will be rendered with Vulkan (from M2 on).
    // RESIZABLE: the user can drag its borders.
    // HIGH_PIXEL_DENSITY: on Retina and other high-DPI screens, get the full pixel resolution.
    // Without it, macOS gives us a quarter of the pixels and scales them up, which looks blurry.
    const SDL_WindowFlags flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    SDL_Window* sdlWindow = SDL_CreateWindow(title.c_str(), static_cast<int>(width), static_cast<int>(height), flags);
    if (!sdlWindow) {
        Log::Error("SDL_CreateWindow failed: {}", SDL_GetError());
        SDL_Quit();
        return nullptr;
    }

    auto window = std::make_unique<Window>(sdlWindow);
    const Extent pixels = window->GetPixelSize();
    Log::Info("Window: {}x{} points, {}x{} pixels (display scale {:.2f})", width, height, pixels.Width,
              pixels.Height, SDL_GetWindowDisplayScale(sdlWindow));
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
