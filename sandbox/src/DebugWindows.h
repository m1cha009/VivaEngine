#pragma once

#include "Viva/Camera.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace Viva {
class Renderer;
}
class FlyCamera;

// The sandbox's debug UI, made with Dear ImGui: a Stats window and a Camera window. F1 shows and
// hides them.
//
// ImGui is an "immediate mode" UI: nothing is kept between frames. Every frame, Draw describes the
// windows from scratch, reading values straight from the game, and a widget the user changes
// writes straight back into the game's variable. Unity's OnGUI and EditorGUILayout work the same
// way.
class DebugWindows {
public:
    // Call from OnFixedUpdate: the Stats window shows how many fixed updates run per second.
    void CountFixedUpdate() { ++m_FixedUpdatesThisSecond; }

    // Builds this frame's windows. Call it every frame from OnUpdate.
    void Draw(float dt, Viva::Renderer& renderer, Viva::Camera& camera, FlyCamera& flyCamera);

private:
    void RecordFrameTime(float dt);
    void DrawStatsWindow(Viva::Renderer& renderer);
    void DrawCameraWindow(Viva::Camera& camera, FlyCamera& flyCamera);

    // The latest frame times in milliseconds, for the graph: a ring buffer, in which
    // m_NextFrameTime is the oldest entry and the next one to be overwritten.
    std::array<float, 120> m_FrameTimes {};
    std::size_t m_NextFrameTime = 0;

    // Counted for a second, then shown during the next one.
    float m_SecondTimer = 0.0f;
    uint32_t m_FramesThisSecond = 0;
    uint32_t m_FixedUpdatesThisSecond = 0;
    float m_FramesPerSecond = 0.0f;
    uint32_t m_FixedUpdatesPerSecond = 0;

    // The camera as it was on the first frame, for the Reset button.
    std::optional<Viva::Camera> m_StartCamera;

    bool m_Visible = true;
    bool m_ShowDemoWindow = false;
};
