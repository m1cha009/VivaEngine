#include "SceneView.h"

#include "Viva/Camera.h"
#include "Viva/GameObject.h"
#include "Viva/Renderer.h"
#include "Viva/Scene.h"
#include "Viva/Transform.h"

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <imgui.h>

#include <algorithm>
#include <cstdint>

using namespace Viva;

namespace {

// The background when the scene has no camera to take one from: a neutral grey-blue sky.
constexpr glm::vec3 kDefaultBackground(0.15f, 0.18f, 0.24f);

} // namespace

void SceneView::Draw(Renderer& renderer, bool* open)
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
            ImGui::Image(ImTextureRef(static_cast<ImTextureID>(texture)), size);
            hovered = ImGui::IsItemHovered();
        }
    }
    ImGui::End();

    // ImGui wants the mouse and keyboard while the pointer is over one of its windows, and the
    // engine then keeps them from Input (M9). Over the scene's image, or while flying, they belong
    // to the camera instead: these tell ImGui to let them go in the next frame.
    if (hovered || m_Looking) {
        ImGui::SetNextFrameWantCaptureMouse(false);
        ImGui::SetNextFrameWantCaptureKeyboard(false);
    }
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
    // Without the object's size (M16 adds bounds), its scale stands in for it: the built-in
    // primitives are about 1 unit across at scale 1.
    // The world matrix's first three columns are the object's axes, each as long as its scale
    // along it, parents' scales included.
    const Transform& transform = gameObject.GetTransform();
    const glm::mat4 world = transform.WorldMatrix();
    const float size = std::max({ glm::length(glm::vec3(world[0])), glm::length(glm::vec3(world[1])),
                                  glm::length(glm::vec3(world[2])), 0.5f });
    m_Camera.Frame(transform.GetPosition(), 2.0f + size * 2.0f);
}
