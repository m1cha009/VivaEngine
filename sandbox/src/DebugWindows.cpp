// GLM's "gtx" extensions are marked experimental (their API may still change), so GLM asks for
// this define before one is included. Here it's for gtx/euler_angles.hpp, below.
#define GLM_ENABLE_EXPERIMENTAL

#include "DebugWindows.h"

#include "FlyCamera.h"
#include "Spinner.h"

#include "Viva/Camera.h"
#include "Viva/Input.h"
#include "Viva/Log.h"
#include "Viva/MeshRenderer.h"
#include "Viva/Renderer.h"
#include "Viva/Scene.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/trigonometric.hpp>
#include <imgui.h>

#include <algorithm>
#include <memory>
#include <vector>

using namespace Viva;

namespace {

float ToMegabytes(uint64_t bytes)
{
    return static_cast<float>(bytes) / (1024.0f * 1024.0f);
}

} // namespace

void DebugWindows::Draw(float dt, Renderer& renderer, Scene& scene)
{
    RecordFrameTime(dt);

    if (Input::GetKeyDown(Key::F1))
        m_Visible = !m_Visible;
    if (!m_Visible)
        return;

    DrawStatsWindow(renderer);
    DrawSceneWindow(scene);
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

void DebugWindows::DrawSceneWindow(Scene& scene)
{
    // In the top-right corner: the pivot (1, 0) makes the position the window's top-right corner
    // rather than its top-left. Its size is fixed, so opening the hierarchy can't grow it off the
    // screen (it scrolls instead), and given in multiples of the font size, so it scales with it.
    const float right = ImGui::GetIO().DisplaySize.x - 10.0f;
    const float fontSize = ImGui::GetFontSize();
    ImGui::SetNextWindowPos(ImVec2(right, 10.0f), ImGuiCond_FirstUseEver, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(26.0f * fontSize, 34.0f * fontSize), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Scene")) {
        const std::vector<std::unique_ptr<GameObject>>& gameObjects = scene.GetGameObjects();
        ImGui::Text("%zu game objects", gameObjects.size());

        // A selected object that's no longer in the scene was destroyed: forget it, before the
        // pointer is used.
        const auto isSelected = [this](const std::unique_ptr<GameObject>& gameObject) { return gameObject.get() == m_Selected; };
        if (m_Selected && std::ranges::none_of(gameObjects, isSelected))
            m_Selected = nullptr;

        // The tree starts at the root objects, those without a parent. Each node draws its own
        // children below it.
        ImGui::SeparatorText("Hierarchy");
        for (const std::unique_ptr<GameObject>& gameObject : gameObjects) {
            if (!gameObject->GetTransform().GetParent())
                DrawHierarchyNode(*gameObject);
        }

        ImGui::SeparatorText("Inspector");
        if (m_Selected)
            DrawInspector(*m_Selected);
        else
            ImGui::TextDisabled("Click an object in the hierarchy");
    }
    ImGui::End();
}

void DebugWindows::DrawHierarchyNode(GameObject& gameObject)
{
    const std::vector<Transform*>& children = gameObject.GetTransform().GetChildren();
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                               ImGuiTreeNodeFlags_SpanAvailWidth;
    if (children.empty())
        flags |= ImGuiTreeNodeFlags_Leaf; // no arrow
    if (&gameObject == m_Selected)
        flags |= ImGuiTreeNodeFlags_Selected;

    // Inactive objects are grayed out, as in Unity. Push changes a style setting until the
    // matching Pop.
    const bool active = gameObject.IsActiveInHierarchy();
    if (!active)
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    // The object's address is its ID in the tree: names needn't be unique, addresses are.
    const bool open = ImGui::TreeNodeEx(&gameObject, flags, "%s", gameObject.GetName().c_str());
    if (!active)
        ImGui::PopStyleColor();

    // A click on the name selects the object; a click on the arrow only opens or closes it.
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        m_Selected = &gameObject;

    if (open) {
        for (Transform* child : children)
            DrawHierarchyNode(child->GetGameObject());
        ImGui::TreePop();
    }
}

void DebugWindows::DrawInspector(GameObject& gameObject)
{
    ImGui::Text("%s", gameObject.GetName().c_str());

    // The object's own switch: below an inactive parent it stays hidden either way.
    bool active = gameObject.IsActiveSelf();
    if (ImGui::Checkbox("Active", &active))
        gameObject.SetActive(active);
    // Destroying is deferred: the object and its children disappear at the end of this frame's
    // updates, and the selection is forgotten on the next frame, once it's gone from the scene.
    ImGui::SameLine();
    if (ImGui::Button("Destroy"))
        gameObject.GetScene().Destroy(gameObject);

    // The Transform's local values, relative to the parent, as in Unity's inspector. DragFloat3
    // edits three floats in a row: a glm::vec3's x, y and z lie one after another in memory, so &x
    // is all it needs.
    Transform& transform = gameObject.GetTransform();
    ImGui::DragFloat3("Position", &transform.LocalPosition.x, 0.05f);
    // The rotation is a quaternion. It's shown as three angles in degrees around X, Y and Z
    // ("Euler angles"), converted both ways every frame. They're taken apart in Unity's order: a
    // turn around Z, then X, then Y. That way the usual turn, around Y, covers the full -180..180,
    // and only the tilt around X is limited to -90..90 (past that, the three angles jump to a
    // different set that means the same rotation; Unity's inspector avoids even that by
    // remembering the angles typed in).
    glm::vec3 angles; // x: around X, y: around Y, z: around Z
    glm::extractEulerAngleYXZ(glm::mat4_cast(transform.LocalRotation), angles.y, angles.x, angles.z);
    angles = glm::degrees(angles);
    if (ImGui::DragFloat3("Rotation", &angles.x, 0.5f)) {
        const glm::vec3 radians = glm::radians(angles);
        transform.LocalRotation = glm::quat_cast(glm::eulerAngleYXZ(radians.y, radians.x, radians.z));
    }
    ImGui::DragFloat3("Scale", &transform.LocalScale.x, 0.01f);

    // The components this window knows how to show. GetComponent returns nullptr when the object
    // has none of that type.
    if (Camera* camera = gameObject.GetComponent<Camera>()) {
        ImGui::SeparatorText("Camera");
        // The angle is stored in radians, and SliderAngle shows and edits it in degrees.
        // AlwaysClamp also limits values typed in (Ctrl+click a slider to type).
        ImGui::SliderAngle("Field of view", &camera->FieldOfView, 20.0f, 120.0f, "%.0f deg", ImGuiSliderFlags_AlwaysClamp);
    }
    if (FlyCamera* flyCamera = gameObject.GetComponent<FlyCamera>()) {
        ImGui::SeparatorText("Fly Camera");
        ImGui::SliderFloat("Move speed", &flyCamera->MoveSpeed, 0.5f, 50.0f, "%.1f", ImGuiSliderFlags_Logarithmic);
    }
    if (Spinner* spinner = gameObject.GetComponent<Spinner>()) {
        ImGui::SeparatorText("Spinner");
        ImGui::DragFloat("Speed", &spinner->DegreesPerSecond, 1.0f, 0.0f, 0.0f, "%.0f deg/s");
    }
    if (gameObject.GetComponent<MeshRenderer>()) {
        ImGui::SeparatorText("Mesh Renderer");
        ImGui::TextDisabled("Draws a mesh with a material");
    }
}
