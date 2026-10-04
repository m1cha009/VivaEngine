#pragma once

#include "EditorCamera.h"

namespace Viva {
class GameObject;
class Renderer;
class Scene;
}

// The Scene window: the scene, seen through the editor's camera. The renderer draws the scene into
// an image of the window's size (Renderer::SetSceneTargetSize), and the window shows that image,
// like a RenderTexture on a RawImage in Unity.
class SceneView {
public:
    // Draws the window. `open` is the Window menu's flag (ImGui clears it when the window's tab is
    // closed).
    void Draw(Viva::Renderer& renderer, bool* open);
    // Flies the camera (while the right mouse button, pressed over the view, is held).
    void Update(float dt);
    // Hands the renderer the editor camera, from Application::OnRender, after the scene set its
    // own: the Scene view shows the scene from here.
    void Render(Viva::Renderer& renderer, const Viva::Scene& scene) const;

    // Puts the camera where the scene's main camera is, looking the same way, if there is one.
    void StartAtMainCamera(const Viva::Scene& scene);
    // Frames a GameObject (the F key).
    void Frame(const Viva::GameObject& gameObject);

private:
    EditorCamera m_Camera;
    bool m_Looking = false; // the right mouse button is held, flying the camera
};
