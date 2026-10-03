#include "DebugWindows.h"

#include "FlyCamera.h"

#include "Viva/Input.h"
#include "Viva/Log.h"
#include "Viva/Renderer.h"

#include <glm/trigonometric.hpp>
#include <imgui.h>

using namespace Viva;

namespace {

float ToMegabytes(uint64_t bytes)
{
    return static_cast<float>(bytes) / (1024.0f * 1024.0f);
}

} // namespace

void DebugWindows::Draw(float dt, Renderer& renderer, Camera& camera, FlyCamera& flyCamera)
{
    RecordFrameTime(dt);
    if (!m_StartCamera)
        m_StartCamera = camera;

    if (Input::GetKeyDown(Key::F1))
        m_Visible = !m_Visible;
    if (!m_Visible)
        return;

    DrawStatsWindow(renderer);
    DrawCameraWindow(camera, flyCamera);
    // ImGui's own demo: every widget it has, each with the code that makes it (imgui_demo.cpp).
    // The pointer to the bool gives the window a close button that sets it to false.
    if (m_ShowDemoWindow)
        ImGui::ShowDemoWindow(&m_ShowDemoWindow);
}

void DebugWindows::RecordFrameTime(float dt)
{
    m_FrameTimes[m_NextFrameTime] = dt * 1000.0f;
    m_NextFrameTime = (m_NextFrameTime + 1) % m_FrameTimes.size();

    m_SecondTimer += dt;
    ++m_FramesThisSecond;
    if (m_SecondTimer >= 1.0f) {
        m_FramesPerSecond = static_cast<float>(m_FramesThisSecond) / m_SecondTimer;
        m_FixedUpdatesPerSecond = m_FixedUpdatesThisSecond;
        Log::Trace("Last second: {} frames, {} fixed updates", m_FramesThisSecond, m_FixedUpdatesThisSecond);
        m_SecondTimer = 0.0f;
        m_FramesThisSecond = 0;
        m_FixedUpdatesThisSecond = 0;
    }
}

void DebugWindows::DrawStatsWindow(Renderer& renderer)
{
    // Where the window first appears, in points from the top-left corner; after that the user can
    // drag it anywhere.
    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);

    // Begin and End bracket a window: every widget in between goes into it. Begin returns false
    // while the window is collapsed (nothing to fill in), but End must be called either way.
    if (ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        // Text takes printf-style formatting: ImGui is a C-style API, unlike Viva::Log.
        const float frameMs = m_FramesPerSecond > 0.0f ? 1000.0f / m_FramesPerSecond : 0.0f;
        ImGui::Text("%.0f FPS (%.2f ms per frame)", m_FramesPerSecond, frameMs);

        // The graph reads the ring buffer from m_NextFrameTime on, so the oldest frame is on the
        // left. "##" hides a label but keeps it as the widget's ID, which must be unique.
        ImGui::PlotLines("##FrameTimes", m_FrameTimes.data(), static_cast<int>(m_FrameTimes.size()),
                         static_cast<int>(m_NextFrameTime), "frame time, 0 to 33 ms", 0.0f, 33.3f,
                         ImVec2(0.0f, 50.0f));
        ImGui::Text("Fixed updates: %u per second", m_FixedUpdatesPerSecond);

        ImGui::SeparatorText("Renderer");
        const RenderStats& stats = renderer.GetStats();
        ImGui::Text("Scene: %u draw calls, %u triangles", stats.DrawCalls, stats.Triangles);
        ImGui::Text("Debug UI: %u draw calls", stats.UiDrawCalls);
        ImGui::Text("GPU memory: %u allocations, %.2f MB", stats.GpuAllocations, ToMegabytes(stats.GpuAllocationBytes));
        ImGui::Text("  in %u memory blocks, %.1f MB", stats.GpuMemoryBlocks, ToMegabytes(stats.GpuMemoryBlockBytes));

        // A widget edits a variable through a pointer and returns true on the frame it changed.
        // Vsync lives in the renderer, so it's copied out, edited, and handed back on a change.
        bool vsync = renderer.IsVSync();
        if (ImGui::Checkbox("VSync", &vsync))
            renderer.SetVSync(vsync);

        ImGui::Separator();
        ImGui::Checkbox("ImGui demo window", &m_ShowDemoWindow);
        ImGui::TextDisabled("F1 hides the debug windows");
    }
    ImGui::End();
}

void DebugWindows::DrawCameraWindow(Camera& camera, FlyCamera& flyCamera)
{
    // In the top-right corner: the pivot (1, 0) makes the position the window's top-right corner
    // rather than its top-left.
    const float right = ImGui::GetIO().DisplaySize.x - 10.0f;
    ImGui::SetNextWindowPos(ImVec2(right, 10.0f), ImGuiCond_FirstUseEver, ImVec2(1.0f, 0.0f));
    if (ImGui::Begin("Camera", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        // These widgets write straight into the camera. DragFloat3 edits three floats in a row:
        // a glm::vec3's x, y and z are laid out one after another, so &x is all it needs.
        ImGui::DragFloat3("Position", &camera.Position.x, 0.05f);

        // The angles are stored in radians, and SliderAngle shows and edits them in degrees.
        ImGui::SliderAngle("Yaw", &camera.Yaw, -180.0f, 180.0f);
        // AlwaysClamp also limits values typed in (Ctrl+click a slider to type).
        const float maxPitch = glm::degrees(FlyCamera::kMaxPitch);
        ImGui::SliderAngle("Pitch", &camera.Pitch, -maxPitch, maxPitch, "%.0f deg", ImGuiSliderFlags_AlwaysClamp);
        ImGui::SliderAngle("Field of view", &camera.FieldOfView, 20.0f, 120.0f, "%.0f deg", ImGuiSliderFlags_AlwaysClamp);

        ImGui::SliderFloat("Fly speed", &flyCamera.MoveSpeed, 0.5f, 50.0f, "%.1f", ImGuiSliderFlags_Logarithmic);

        // A button returns true on the frame it's clicked: the immediate-mode way to handle a click.
        if (ImGui::Button("Reset"))
            camera = *m_StartCamera; // set by Draw on the first frame
    }
    ImGui::End();
}
