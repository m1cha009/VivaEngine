#include "SceneView.h"

#include "Picking.h"
#include "SceneEditor.h"

#include "Viva/Camera.h"
#include "Viva/GameObject.h"
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

} // namespace

void SceneView::Draw(Renderer& renderer, SceneEditor& editor, bool* open)
{
    // No padding: the image fills the window to its edges.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const bool visible = ImGui::Begin("Scene", open);
    ImGui::PopStyleVar();
    bool hovered = false; // the mouse is over the scene's image
    if (visible) {
        // The image is as big as the space the window has, in pixels: ImGui works in points, which
        // a Retina screen has two pixels per. The renderer makes an image of that size for the next
        // frame; this frame shows the current one, stretched if the size just changed.
        const ImVec2 size = ImGui::GetContentRegionAvail();
        const ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
        const auto width = static_cast<uint32_t>(std::max(size.x * scale.x, 0.0f));
        const auto height = static_cast<uint32_t>(std::max(size.y * scale.y, 0.0f));
        renderer.SetSceneTargetSize(width, height);
        if (const uint64_t texture = renderer.GetSceneTexture(); texture != 0 && width > 0 && height > 0) {
            const ImVec2 min = ImGui::GetCursorScreenPos();
            ImGui::Image(ImTextureRef(static_cast<ImTextureID>(texture)), size);
            hovered = ImGui::IsItemHovered();
            m_ImageMin = { min.x, min.y };
            m_ImageSize = { size.x, size.y };
            m_ViewProjection = m_Camera.ProjectionMatrix(size.x / size.y) * m_Camera.ViewMatrix();

            DrawSelectionOutline(editor);

            // A left click selects what's under the mouse, or nothing if it's empty space, as in
            // Unity. ImGui sees the click first, so it doesn't reach the scene when it lands on
            // another window in front of the image.
            if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                editor.Select(PickGameObject(editor.GetScene(), RayThrough(ImGui::GetMousePos())));
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

Ray SceneView::RayThrough(const ImVec2& point) const
{
    // The point in "normalized device coordinates": -1 to 1 across the image, and -1 to 1 from its
    // bottom to its top (the projection is OpenGL-style, y up, while screen y runs down).
    const glm::vec2 uv = (glm::vec2(point.x, point.y) - m_ImageMin) / m_ImageSize;
    const glm::vec2 ndc(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    // The projection squeezed everything the camera sees into a box where depth runs from 0 (the
    // near plane) to 1 (the far plane). Its inverse takes that box back into the world: the point
    // at depth 0 lies on the near plane, the one at depth 1 on the far plane, and the ray runs
    // from one to the other. The division by w undoes the perspective divide.
    const glm::mat4 toWorld = glm::inverse(m_ViewProjection);
    glm::vec4 nearPoint = toWorld * glm::vec4(ndc, 0.0f, 1.0f);
    glm::vec4 farPoint = toWorld * glm::vec4(ndc, 1.0f, 1.0f);
    nearPoint /= nearPoint.w;
    farPoint /= farPoint.w;
    return { .Origin = glm::vec3(nearPoint), .Direction = glm::normalize(glm::vec3(farPoint - nearPoint)) };
}

void SceneView::DrawLine(ImDrawList& drawList, const glm::vec3& from, const glm::vec3& to, ImU32 color) const
{
    // Into clip space, where the near plane is z = 0 and everything in front of the camera has
    // z >= 0. A line reaching behind the camera is cut where it crosses the near plane first:
    // projecting a point behind the camera would flip it to the wrong side of the picture.
    glm::vec4 a = m_ViewProjection * glm::vec4(from, 1.0f);
    glm::vec4 b = m_ViewProjection * glm::vec4(to, 1.0f);
    if (a.z < 0.0f && b.z < 0.0f)
        return; // all behind
    if (a.z < 0.0f)
        a = glm::mix(a, b, a.z / (a.z - b.z));
    else if (b.z < 0.0f)
        b = glm::mix(b, a, b.z / (b.z - a.z));

    // Then onto the image: divide by w (the perspective divide), and map -1..1 to its corners.
    const auto toImage = [this](const glm::vec4& clip) {
        const glm::vec2 ndc = glm::vec2(clip) / clip.w;
        return ImVec2(m_ImageMin.x + (ndc.x * 0.5f + 0.5f) * m_ImageSize.x,
                      m_ImageMin.y + (0.5f - ndc.y * 0.5f) * m_ImageSize.y);
    };
    drawList.AddLine(toImage(a), toImage(b), color, 1.5f);
}

void SceneView::DrawSelectionOutline(const SceneEditor& editor) const
{
    const GameObject* selection = editor.GetSelection();
    if (!selection)
        return;

    // Lines drawn with ImGui's draw list, over the image, rather than by the renderer into it: they
    // stay visible behind other objects, and need no 3D line drawing. M17's gizmos work the same
    // way. Clipped to the image, so they don't spill over the window's edges.
    ImDrawList& drawList = *ImGui::GetWindowDrawList();
    drawList.PushClipRect(ImVec2(m_ImageMin.x, m_ImageMin.y),
                          ImVec2(m_ImageMin.x + m_ImageSize.x, m_ImageMin.y + m_ImageSize.y), true);

    // Each box the selection and the objects below it draw (they move with it), as it lies in the
    // world: its 12 edges join the corners whose numbers differ in exactly one bit (see
    // Bounds::Corner).
    ForEachDrawnBox(*selection, true, [&](const Bounds& bounds, const glm::mat4& world) {
        glm::vec3 corners[8];
        for (int i = 0; i < 8; ++i)
            corners[i] = world * glm::vec4(bounds.Corner(i), 1.0f);
        for (int i = 0; i < 8; ++i) {
            for (int bit = 1; bit < 8; bit <<= 1) {
                if (!(i & bit))
                    DrawLine(drawList, corners[i], corners[i | bit], kSelectionColor);
            }
        }
    });
    drawList.PopClipRect();
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
