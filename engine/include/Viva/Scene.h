#pragma once

#include "Viva/GameObject.h"

#include <memory>
#include <string>
#include <vector>

namespace Viva {

class Camera;
class Renderer;

// The game world: every GameObject, which the scene owns (Unity's scene). Application has one;
// get it with GetScene(). Each frame the Application updates it, then has it drawn through its
// main camera.
class Scene {
public:
    Scene() = default;

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    // Creates a GameObject that has only a Transform, at the origin of its parent (or of the
    // world without one): Unity's new GameObject(name). The reference stays valid until the
    // object is destroyed.
    GameObject& CreateGameObject(std::string name, GameObject* parent = nullptr);

    // Destroys a GameObject and everything below it in the hierarchy at the end of this frame's
    // updates, before drawing (Unity's Destroy). Any pointer or reference to them is invalid after
    // that: the game must forget them.
    void Destroy(GameObject& gameObject);

    // Whether a GameObject is still in this scene. A destroyed one leaves at the end of the frame
    // it was destroyed in. Only the pointer is compared, so it's safe to ask about an object that's
    // gone: it's how code that keeps pointers to objects others may destroy (the debug windows'
    // Destroy button) can tell, like Unity's "== null" check on a destroyed object.
    bool Contains(const GameObject* gameObject) const;

    // The camera the frame is drawn from: the Camera of the first active GameObject that has one
    // (Unity's Camera.main). Without a camera, nothing in the scene is drawn.
    Camera* GetMainCamera() const;

    // Every GameObject, in the order they were created, for tools like a hierarchy window.
    const std::vector<std::unique_ptr<GameObject>>& GetGameObjects() const { return m_GameObjects; }

private:
    // The engine side, used by Application's main loop.
    friend class Application;
    void FixedUpdate(float fixedDt);
    void Update(float dt);
    // Removes the objects destroyed this frame, then submits every visible MeshRenderer, seen from
    // the main camera.
    void Render(Renderer& renderer);

    void StartNewComponents();
    void RemoveDestroyed();

    std::vector<std::unique_ptr<GameObject>> m_GameObjects;
};

} // namespace Viva
