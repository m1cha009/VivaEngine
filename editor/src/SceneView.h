#pragma once

#include "EditorCamera.h"
#include "Gizmo.h"
#include "SceneViewport.h"

#include <glm/vec3.hpp>

#include <memory>

class SceneEditor;

namespace Viva {
class GameObject;
class Renderer;
class RenderTarget;
class Scene;
}

// The Scene window: the scene, seen through the editor's camera. The renderer draws the scene into
// an image of the window's size (a render target, Renderer::DrawScene), and the window shows that
// image, like a RenderTexture on a RawImage in Unity. Over the image it draws the selection's
// outline and the gizmo (M17); a left click selects the object under the mouse, or grabs a gizmo
// handle. A row of tool buttons sits above the image.
class SceneView {
public:
    // Draws the window. `open` is the Window menu's flag (ImGui clears it when the window's tab is
    // closed).
    void Draw(Viva::Renderer& renderer, SceneEditor& editor, bool* open);
    // Flies the camera (while the right mouse button, pressed over the view, is held).
    void Update(float dt);

    // The tool keys, as in Unity: W, E and R pick Move, Rotate and Scale, and X switches between
    // the object's axes and the world's. Not while flying, where W moves the camera.
    void ReadToolKeys();

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
    void DrawToolbar();
    // The selection's outline: the bounding boxes of everything it and the objects below it draw.
    void DrawSelectionOutline(ImDrawList& drawList, const SceneEditor& editor) const;

    EditorCamera m_Camera;
    std::shared_ptr<Viva::RenderTarget> m_Target; // made on first use, when the renderer exists
    Gizmo m_Gizmo;
    SceneViewport m_Viewport; // this frame's image and camera
    bool m_Looking = false;   // the right mouse button is held, flying the camera
    bool m_Focused = false;
};
