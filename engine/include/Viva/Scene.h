#pragma once

#include "Viva/GameObject.h"

#include <memory>
#include <string>
#include <vector>

namespace Viva {

class Assets;
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
    // Removes a component from its GameObject, the same way: at the end of this frame's updates
    // (Unity's Destroy(component)). The Transform isn't a component here, so it can't be removed.
    void Destroy(Component& component);
    // Destroys every GameObject, the same way: they're gone at the end of this frame's updates.
    // Objects created after this call stay. Loading another scene file after it replaces the
    // scene (Unity's LoadScene in its default, single mode).
    void Clear();

    // Whether a GameObject is still in this scene. A destroyed one leaves at the end of the frame
    // it was destroyed in. Only the pointer is compared, so it's safe to ask about an object that's
    // gone: it's how code that keeps pointers to objects others may destroy (the debug windows'
    // Destroy button) can tell, like Unity's "== null" check on a destroyed object.
    bool Contains(const GameObject* gameObject) const;

    // Creates a copy of `original` and of everything below it, below `parent` (or as a root
    // object): the same name, active flag, Transform values and components, with the same field
    // values. Unity's Instantiate(original, parent), and what its Duplicate command does.
    //
    // Like Unity's, it works through serialization: the original is written as it would be into
    // a scene file, and the copy is read back from that, its meshes, textures and materials found
    // through `assets`. So it copies exactly what a scene file would hold: components that aren't
    // registered, and meshes and textures made in code, are left out (with a warning each).
    GameObject& Instantiate(const GameObject& original, Assets& assets, GameObject* parent = nullptr);

    // The camera the frame is drawn from: the Camera of the first active GameObject that has one
    // (Unity's Camera.main). Without a camera, nothing in the scene is drawn.
    Camera* GetMainCamera() const;

    // Every GameObject, in the order they were created, for tools like a hierarchy window.
    const std::vector<std::unique_ptr<GameObject>>& GetGameObjects() const { return m_GameObjects; }

    // Scene files (M13): every GameObject, with its name, active flag, Transform, components and
    // children, as JSON text (see docs/milestones/M13.md for the format). Unity saves .unity files
    // the same way, as text. Paths are UTF-8; both return false (after logging why) if the file
    // can't be written or read.
    //
    // Save writes what each component's VisitFields hands over. Components whose type isn't
    // registered (see Viva/ComponentRegistry.h), and meshes and textures made in code, can't be
    // written: they're left out, with a warning for each.
    bool Save(const std::string& path) const;
    // Scene files end in .scene (this is without the dot, as file dialogs want it).
    static constexpr const char* kFileExtension = "scene";
    // Adds the file's GameObjects to the scene, next to any already in it (Unity's LoadScene with
    // LoadSceneMode.Additive). Their meshes, textures and materials come from `assets`. A component
    // type that isn't registered is left out, with a warning. Their OnStart runs before their first
    // update, as for objects created in code.
    bool Load(const std::string& path, Assets& assets);

private:
    // The engine side, used by Application's main loop.
    friend class Application;
    void FixedUpdate(float fixedDt);
    void Update(float dt);
    // Removes the objects destroyed this frame, hands the renderer the main camera (if any), then
    // submits every visible MeshRenderer.
    void Render(Renderer& renderer);

    void StartNewComponents();
    void RemoveDestroyed();

    std::vector<std::unique_ptr<GameObject>> m_GameObjects;
};

} // namespace Viva
