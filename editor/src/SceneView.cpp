#include "SceneView.h"

#include "EditorUi.h"
#include "Picking.h"
#include "SceneEditor.h"

#include "Viva/Camera.h"
#include "Viva/GameObject.h"
#include "Viva/Input.h"
#include "Viva/Renderer.h"
#include "Viva/Scene.h"
#include "Viva/Transform.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/matrix.hpp>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

using namespace Viva;

namespace {

// The background when the scene has no camera to take one from: a neutral grey-blue sky.
constexpr glm::vec3 kDefaultBackground(0.15f, 0.18f, 0.24f);

// How far in front of the camera new objects go.
constexpr float kPlacementDistance = 8.0f;

// The selection's outline: Unity's orange.
const ImU32 kSelectionColor = IM_COL32(255, 140, 0, 255);

// The tools, with their toolbar labels and keys: one table, so the key a button shows is the one
// that works.
struct ToolInfo {
    GizmoTool Tool;
    const char* Label;
    Key ShortcutKey;
};
constexpr ToolInfo kTools[] = {
    { GizmoTool::Move, "Move (W)", Key::W },
    { GizmoTool::Rotate, "Rotate (E)", Key::E },
    { GizmoTool::Scale, "Scale (R)", Key::R },
};

} // namespace

void SceneView::Draw(Renderer& renderer, SceneEditor& editor, bool* open)
{
    // No padding: the image fills the window to its edges.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const bool visible = ImGui::Begin("Scene", open);
    ImGui::PopStyleVar();
    bool hovered = false; // the mouse is over the scene's image
    if (visible) {
        DrawToolbar();

        // The image is as big as the space the window has left, in pixels: ImGui works in points,
        // which a Retina screen has two pixels per. The renderer makes an image of that size for
        // the next frame; this frame shows the current one, stretched if the size just changed.
        const ImVec2 size = ImGui::GetContentRegionAvail();
        const ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
        const auto width = static_cast<uint32_t>(std::max(size.x * scale.x, 0.0f));
        const auto height = static_cast<uint32_t>(std::max(size.y * scale.y, 0.0f));
        renderer.SetSceneTargetSize(width, height);
        if (const uint64_t texture = renderer.GetSceneTexture(); texture != 0 && width > 0 && height > 0) {
            // The image area is an invisible button that takes the left mouse button: an ImGui
            // item, so ImGui knows when a press on it starts (IsItemActivated), lasts (IsItemActive)
            // and ends, as for any button. A gizmo drag is exactly that. While it's held, the
            // window doesn't move, and the editor knows not to end the undo step yet. The picture
            // is drawn on it with the draw list.
            const ImVec2 min = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("SceneImage", size, ImGuiButtonFlags_MouseButtonLeft);
            hovered = ImGui::IsItemHovered();
            const bool pressed = ImGui::IsItemActivated();
            const bool held = ImGui::IsItemActive();
            ImDrawList& drawList = *ImGui::GetWindowDrawList();
            drawList.AddImage(ImTextureRef(static_cast<ImTextureID>(texture)), min, ImVec2(min.x + size.x, min.y + size.y));

            m_Viewport = {
                .ImageMin = { min.x, min.y },
                .ImageSize = { size.x, size.y },
                .View = m_Camera.ViewMatrix(),
                .Projection = m_Camera.ProjectionMatrix(size.x / size.y),
                .CameraPosition = m_Camera.Position,
            };

            // The outline and the gizmo are clipped to the image, so they don't spill over the
            // window's edges.
            drawList.PushClipRect(min, ImVec2(min.x + size.x, min.y + size.y), true);
            DrawSelectionOutline(drawList, editor);
            const glm::vec2 mouse(ImGui::GetMousePos().x, ImGui::GetMousePos().y);
            if (GameObject* selection = editor.GetSelection()) {
                const GizmoInput input { .Mouse = mouse, .Pressed = pressed, .Held = held, .Snap = IsCtrlHeld() };
                if (m_Gizmo.Update(m_Viewport, drawList, selection->GetTransform(), input))
                    editor.MarkChanged();
            }
            drawList.PopClipRect();

            // A press that didn't grab a handle selects what's under the mouse, or nothing if it's
            // empty space, as in Unity.
            if (pressed && !m_Gizmo.IsDragging())
                editor.Select(PickGameObject(editor.GetScene(), m_Viewport.RayThrough(mouse)));
        }
    }
    m_Focused = visible && (hovered || ImGui::IsWindowFocused());
    ImGui::End();

    // ImGui wants the mouse and keyboard while the pointer is over one of its windows, and the
    // engine then keeps them from Input (M9). Over the scene's image, or while flying, they belong
    // to the camera instead: these tell ImGui to let them go in the next frame.
    if (hovered || m_Looking) {
        ImGui::SetNextFrameWantCaptureMouse(false);
        ImGui::SetNextFrameWantCaptureKeyboard(false);
    }
}

void SceneView::DrawToolbar()
{
    // A row of buttons above the image, inset by the usual spacing (the window has no padding).
    // The current tool's button is drawn as pressed.
    // (SetCursorPos counts from the window's corner, title bar included, so this starts from where
    // the content begins.)
    const ImVec2 spacing = ImGui::GetStyle().ItemSpacing;
    const ImVec2 start = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(start.x + spacing.x, start.y + spacing.y));
    for (const ToolInfo& tool : kTools) {
        const bool current = m_Gizmo.Tool == tool.Tool;
        if (current)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button(tool.Label))
            m_Gizmo.Tool = tool.Tool;
        if (current)
            ImGui::PopStyleColor();
        ImGui::SameLine();
    }
    if (ImGui::Button(m_Gizmo.Local ? "Local (X)" : "Global (X)"))
        m_Gizmo.Local = !m_Gizmo.Local;
    ImGui::SameLine();
    ImGui::TextDisabled("Hold Ctrl to snap");
    // The next line starts at the window's left edge again (it has no padding), so the image
    // below fills the width.
}

void SceneView::ReadToolKeys()
{
    if (m_Looking)
        return;
    for (const ToolInfo& tool : kTools) {
        if (Input::GetKeyDown(tool.ShortcutKey))
            m_Gizmo.Tool = tool.Tool;
    }
    if (Input::GetKeyDown(Key::X))
        m_Gizmo.Local = !m_Gizmo.Local;
}

void SceneView::DrawSelectionOutline(ImDrawList& drawList, const SceneEditor& editor) const
{
    const GameObject* selection = editor.GetSelection();
    if (!selection)
        return;
    // Each box the selection and the objects below it draw (they move with it), as it lies in the
    // world: its 12 edges join the corners whose numbers differ in exactly one bit (see
    // Bounds::Corner). Drawn with ImGui's draw list over the image, rather than by the renderer
    // into it, they stay visible behind other objects and need no 3D line drawing.
    ForEachDrawnBox(*selection, true, [&](const Bounds& bounds, const glm::mat4& world) {
        glm::vec3 corners[8];
        for (int i = 0; i < 8; ++i)
            corners[i] = world * glm::vec4(bounds.Corner(i), 1.0f);
        for (int i = 0; i < 8; ++i) {
            for (int bit = 1; bit < 8; bit <<= 1) {
                if (!(i & bit))
                    m_Viewport.DrawLine(drawList, corners[i], corners[i | bit], kSelectionColor);
            }
        }
    });
}

void SceneView::Update(float dt)
{
    m_Looking = m_Camera.Update(dt);
}

void SceneView::Render(Renderer& renderer, const Scene& scene) const
{
    renderer.SetCamera(m_Camera.ViewMatrix(), m_Camera.ProjectionMatrix(renderer.GetAspectRatio()));
    const Camera* camera = scene.GetMainCamera();
    renderer.SetClearColor(camera ? camera->BackgroundColor : kDefaultBackground);
    renderer.DrawGrid();
}

void SceneView::StartAtMainCamera(const Scene& scene)
{
    if (const Camera* camera = scene.GetMainCamera()) {
        const Transform& transform = camera->GetTransform();
        m_Camera.Position = transform.GetPosition();
        m_Camera.Rotation = transform.GetRotation();
    }
}

void SceneView::Frame(const GameObject& gameObject)
{
    // The object's bounds, with everything below it, if it draws anything. A sphere around that
    // box fits in the picture when the camera is radius / sin(half the field of view) away from
    // its center; a little more leaves a margin. An object that draws nothing (an empty or a
    // camera) is framed as if it were about a unit across.
    glm::vec3 center = gameObject.GetTransform().GetPosition();
    float radius = 0.5f;
    if (const std::optional<Bounds> bounds = GetWorldBounds(gameObject)) {
        center = bounds->Center();
        radius = std::max(glm::length(bounds->Size()) * 0.5f, 0.1f);
    }
    m_Camera.Frame(center, radius / std::sin(m_Camera.FieldOfView * 0.5f) * 1.2f);
}

glm::vec3 SceneView::GetPlacement() const
{
    return m_Camera.Position + m_Camera.Rotation * Transform::kForwardAxis * kPlacementDistance;
}
