#pragma once

#include "Viva/Component.h"
#include "Viva/Transform.h"

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace Viva {

class Scene;

// An object in the scene, like Unity's GameObject: a name, a Transform, and components that give
// it looks and behavior (a MeshRenderer to be seen, a Camera to see from, the game's own logic).
// Create them with Scene::CreateGameObject: the scene owns them.
class GameObject {
public:
    // Used by Scene::CreateGameObject. A GameObject made any other way isn't part of a scene.
    GameObject(Scene& scene, std::string name);

    GameObject(const GameObject&) = delete;
    GameObject& operator=(const GameObject&) = delete;

    const std::string& GetName() const { return m_Name; }
    void SetName(std::string name) { m_Name = std::move(name); }

    // Two versions: a const GameObject hands out a const Transform, which can be read but not changed.
    Transform& GetTransform() { return m_Transform; }
    const Transform& GetTransform() const { return m_Transform; }
    Scene& GetScene() const { return *m_Scene; }

    // An inactive object, and everything below it in the hierarchy, neither updates nor is drawn:
    // Unity's SetActive, activeSelf and activeInHierarchy.
    void SetActive(bool active) { m_Active = active; }
    bool IsActiveSelf() const { return m_Active; }
    bool IsActiveInHierarchy() const;

    // Creates a component of type T, attaches it and returns it: Unity's AddComponent<T>(), except
    // that any arguments are passed on to T's constructor:
    //     GameObject& crate = scene.CreateGameObject("Crate");
    //     crate.AddComponent<MeshRenderer>(cubeMesh, crateMaterial);
    template <typename T, typename... Args>
    T& AddComponent(Args&&... args);

    // The first component of type T (or of a type derived from T), or nullptr if there's none:
    // Unity's GetComponent<T>().
    template <typename T>
    T* GetComponent() const;

private:
    // The scene drives the components through these, and handles destruction.
    friend class Scene;
    void StartComponents();
    void UpdateComponents(float dt);
    void FixedUpdateComponents(float fixedDt);
    template <typename Callback>
    void ForEachStartedComponent(Callback callback);
    bool CanUpdate() const { return !m_Destroyed && IsActiveInHierarchy(); }
    void MarkDestroyed(); // this object and everything below it

    // Declared in this order because they're initialized in this order: m_Transform takes the
    // GameObject it belongs to, and the others are ready by then.
    Scene* m_Scene;
    std::string m_Name;
    Transform m_Transform;
    std::vector<std::unique_ptr<Component>> m_Components;
    bool m_Active = true;
    bool m_Destroyed = false; // Scene::Destroy was called: it goes at the end of the frame's updates
};

// Templates are defined in the header: the compiler generates AddComponent<MeshRenderer>,
// AddComponent<Camera> and so on in each file that uses them, so it must see the code there. (C#
// generics are compiled once; C++ templates are stamped out per type.)

template <typename T, typename... Args>
T& GameObject::AddComponent(Args&&... args)
{
    // static_assert checks at compile time: AddComponent<int>() stops the build with this message.
    static_assert(std::is_base_of_v<Component, T>, "AddComponent<T>: T must derive from Viva::Component");

    // "typename... Args" is a parameter pack: any number of arguments of any types.
    // std::forward<Args>(args)... hands them to T's constructor exactly as they were passed.
    auto component = std::make_unique<T>(std::forward<Args>(args)...);
    T& added = *component;
    static_cast<Component&>(added).m_GameObject = this;
    // A unique_ptr<T> converts to a unique_ptr<Component>: ownership moves into the list.
    m_Components.push_back(std::move(component));
    return added;
}

template <typename T>
T* GameObject::GetComponent() const
{
    // dynamic_cast asks at runtime whether a Component is really a T, like C#'s "as": it gives a T*,
    // or nullptr if the component is something else.
    for (const std::unique_ptr<Component>& component : m_Components) {
        if (T* match = dynamic_cast<T*>(component.get()))
            return match;
    }
    return nullptr;
}

} // namespace Viva
