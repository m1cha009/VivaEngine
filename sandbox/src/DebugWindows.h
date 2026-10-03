#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace Viva {
class GameObject;
class Renderer;
class Scene;
}

// The sandbox's debug UI, made with Dear ImGui: a Stats window, and a Scene window with the
// hierarchy and an inspector, like Unity's. F1 shows and hides them.
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
    void Draw(float dt, Viva::Renderer& renderer, Viva::Scene& scene);

    // Shown or hidden, as F1 toggles them. They start shown.
    void SetVisible(bool visible) { m_Visible = visible; }

private:
    void RecordFrameTime(float dt);
    void DrawStatsWindow(Viva::Renderer& renderer);
    void DrawSceneWindow(Viva::Scene& scene);
    void DrawHierarchyNode(Viva::GameObject& gameObject);
    void DrawInspector(Viva::GameObject& gameObject);

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

    // The GameObject picked in the hierarchy, or nullptr. Only a pointer: the scene owns the
    // object, so the Scene window checks it's still there before using it.
    Viva::GameObject* m_Selected = nullptr;

    bool m_Visible = true;
    bool m_ShowDemoWindow = false;
};
