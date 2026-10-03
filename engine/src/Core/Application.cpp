#include "Viva/Application.h"

#include "Platform/InputEvents.h"
#include "Platform/SDLVersion.h"
#include "Platform/Window.h"
#include "Renderer/VulkanVersion.h"
#include "Viva/Log.h"
#include "Viva/Time.h"
#include "Viva/Version.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <utility>

namespace Viva {

namespace {

// A frame that took longer than this (a breakpoint, dragging the window) counts as this long.
// Otherwise the game would jump ahead, and the fixed-update loop would have to run hundreds of
// steps to catch up, making the next frame slow too (the "spiral of death").
constexpr float kMaxDeltaTime = 0.25f;

} // namespace

Application::Application(ApplicationSettings settings)
    : m_Settings(std::move(settings))
{
}

// Defined here, not in the header: destroying the unique_ptr<Window> needs Window's full
// definition, and only this file includes Platform/Window.h.
Application::~Application() = default;

int Application::Run()
{
    Log::Info("VivaEngine {} | SDL {} | Vulkan headers {} | Vulkan loader {}", GetEngineVersion().ToString(),
              GetSDLVersion().ToString(), GetVulkanHeaderVersion().ToString(), GetVulkanLoaderVersion().ToString());

    m_Window = Window::Create(m_Settings.Title, m_Settings.Width, m_Settings.Height);
    if (!m_Window)
        return EXIT_FAILURE;

    OnStart();

    using Clock = std::chrono::steady_clock;
    Clock::time_point previousTime = Clock::now();
    float fixedTimeAccumulator = 0.0f;

    while (!m_QuitRequested) {
        BeginInputFrame();
        m_Window->PollEvents();
        if (m_Window->ShouldClose())
            break;

        if (m_Window->IsMinimized()) {
            // Nothing is visible, so don't spin at 100% CPU: sleep until an event arrives (such as
            // the window being restored). The time spent minimized doesn't count as a frame.
            m_Window->WaitForEvent();
            previousTime = Clock::now();
            continue;
        }

        const Clock::time_point now = Clock::now();
        const float dt = std::min(std::chrono::duration<float>(now - previousTime).count(), kMaxDeltaTime);
        previousTime = now;
        Time::BeginFrame(dt);

        // Fixed timestep: real time goes into an accumulator, and every whole fixed step it holds
        // runs one OnFixedUpdate. At 144 FPS that's sometimes zero calls per frame; at 30 FPS it's
        // one or two. Either way it averages 50 per second, which keeps gameplay that runs in
        // fixed steps (physics, later) behaving the same at any frame rate.
        fixedTimeAccumulator += dt;
        const float fixedDt = Time::FixedDeltaTime();
        while (fixedTimeAccumulator >= fixedDt) {
            Time::SetDeltaTime(fixedDt); // as in Unity, DeltaTime() inside OnFixedUpdate is the fixed step
            OnFixedUpdate(fixedDt);
            fixedTimeAccumulator -= fixedDt;
        }

        Time::SetDeltaTime(dt);
        OnUpdate(dt);
    }

    OnShutdown();
    m_Window.reset(); // closes the window and shuts SDL down (Window's destructor)
    return EXIT_SUCCESS;
}

void Application::Quit()
{
    m_QuitRequested = true;
}

void Application::SetWindowTitle(const std::string& title)
{
    if (m_Window)
        m_Window->SetTitle(title);
}

} // namespace Viva
