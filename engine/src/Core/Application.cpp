#include "Viva/Application.h"

#include "Platform/InputEvents.h"
#include "Platform/SDLVersion.h"
#include "Platform/Window.h"
#include "Renderer/VulkanVersion.h"
#include "Viva/Log.h"
#include "Viva/Renderer.h"
#include "Viva/Time.h"
#include "Viva/Version.h"

#include <imgui.h>

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

// Defined here, not in the header: destroying the unique_ptrs needs the full definitions of
// Window and Renderer, which only this file includes.
//
// This is where the renderer and the window are destroyed: members go in reverse order of
// declaration, the renderer (all of Vulkan) first, then the window and SDL. It runs after the
// game's own destructor, so the GPU resources a game keeps in its members (shared_ptrs to meshes,
// materials...) have already been released when the renderer shuts down.
Application::~Application() = default;

int Application::Run()
{
    Log::Info("VivaEngine {} | SDL {} | Vulkan headers {} | Vulkan loader {}", GetEngineVersion().ToString(),
              GetSDLVersion().ToString(), GetVulkanHeaderVersion().ToString(), GetVulkanLoaderVersion().ToString());

    m_Window = Window::Create(m_Settings.Title, m_Settings.Width, m_Settings.Height, m_Settings.Display);
    if (!m_Window)
        return EXIT_FAILURE;

    m_Renderer = Renderer::Create(*m_Window, m_Settings.VSync);
    if (!m_Renderer)
        return EXIT_FAILURE;

    OnStart();

    using Clock = std::chrono::steady_clock;
    Clock::time_point previousTime = Clock::now();
    float fixedTimeAccumulator = 0.0f;

    while (!m_QuitRequested) {
        // Wait for the GPU, and with vsync for the display, first. Input read right after that is
        // as fresh as it can be by the time this frame is drawn. (Waiting at the end instead would
        // let input sit unused for up to a whole refresh.)
        const bool drawing = m_Renderer->BeginFrame();

        // Input that arrived since the game's last update. (It's cleared after each update, not
        // here, so a key press that arrives during a pass that can't draw isn't lost.)
        m_Window->PollEvents();
        if (m_Window->ShouldClose())
            m_QuitRequested = true;

        if (!drawing) {
            // This frame can't be drawn. If nothing is visible (minimized, or dragged down to zero
            // height), don't spin at 100% CPU: sleep until an event arrives, such as the window
            // being restored. Otherwise the swapchain is being rebuilt; just go round again. The
            // time spent like this doesn't count as a frame.
            // (Asked again here, after PollEvents: the events may have just restored the window.)
            if (!m_Window->IsDrawable() && !m_QuitRequested)
                m_Window->WaitForEvent();
            previousTime = Clock::now();
            continue;
        }

        const Clock::time_point now = Clock::now();
        const float dt = std::min(std::chrono::duration<float>(now - previousTime).count(), kMaxDeltaTime);
        previousTime = now;
        Time::BeginFrame(dt);

        // Dear ImGui's frame starts: from here until ImGui::Render(), the game can build debug
        // windows (normally in OnUpdate), the way OnGUI code does in Unity. The renderer's half
        // of ImGui started its frame in BeginFrame.
        m_Window->NewImGuiFrame();
        ImGui::NewFrame();

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
        OnUpdate(dt); // the game updates, submits what to draw and builds its debug windows

        // The UI is complete: ImGui turns this frame's windows into lists of triangles, which
        // EndFrame draws over the scene.
        ImGui::Render();

        // Last, like in Unity: draw the frame the game just updated. Once BeginFrame succeeded,
        // EndFrame must follow, even when quitting: it submits the work that signals the frame's
        // fence.
        m_Renderer->EndFrame(m_Camera);

        // The game has seen this frame's input: clear the "pressed/released this frame" flags and
        // the mouse movement before collecting the next frame's.
        BeginInputFrame();
    }

    OnShutdown();
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
