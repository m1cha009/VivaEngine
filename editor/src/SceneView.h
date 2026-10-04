#pragma once

#include "EditorCamera.h"
#include "Picking.h"

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <imgui.h>

class SceneEditor;

namespace Viva {
class GameObject;
class Renderer;
class Scene;
}

// The Scene window: the scene, seen through the editor's camera. The renderer draws the scene into
// an image of the window's size (Renderer::SetSceneTargetSize), and the window shows that image,
// like a RenderTexture on a RawImage in Unity. Over the image it draws the selection's outline,
// and a left click selects the object under the mouse.
class SceneView {
public:
    // Draws the window. `open` is the Window menu's flag (ImGui clears it when the window's tab is
    // closed).
    void Draw(Viva::Renderer& renderer, SceneEditor& editor, bool* open);
    // Flies the camera (while the right mouse button, pressed over the view, is held).
    void Update(float dt);
    // Hands the renderer the editor camera, from Application::OnRender, after the scene set its
    // own: the Scene view shows the scene from here, with the ground grid.
    void Render(Viva::Renderer& renderer, const Viva::Scene& scene) const;

    // Puts the camera where the scene's main camera is, looking the same way, if there is one.
    void StartAtMainCamera(const Viva::Scene& scene);
    // Frames a GameObject (the F key): moves the camera back until it's in full view.
    void Frame(const Viva::GameObject& gameObject);
    // Where new objects go: a little in front of the camera, like the pivot of Unity's Scene view.
    glm::vec3 GetPlacement() const;
    // Whether the window had the keyboard focus, or the mouse over its image, this frame: then the
    // Edit keys act on the selection (see HierarchyWindow::IsFocused).
    bool IsFocused() const { return m_Focused; }

private:
    // The ray from the camera through a point of the image (in ImGui's screen coordinates).
    Ray RayThrough(const ImVec2& point) const;
    // The selection's outline: the bounding boxes of everything it and the objects below it draw.
    void DrawSelectionOutline(const SceneEditor& editor) const;
    // A line between two points in the world, drawn over the image.
    void DrawLine(ImDrawList& drawList, const glm::vec3& from, const glm::vec3& to, ImU32 color) const;

    EditorCamera m_Camera;
    bool m_Looking = false; // the right mouse button is held, flying the camera
    bool m_Focused = false;
    // Where the image was drawn this frame, in ImGui's screen coordinates (points), and the
    // camera's view and projection for it: what the outline and clicks need to map between the
    // world and the image.
    glm::vec2 m_ImageMin { 0.0f };
    glm::vec2 m_ImageSize { 0.0f };
    glm::mat4 m_ViewProjection { 1.0f };
};
