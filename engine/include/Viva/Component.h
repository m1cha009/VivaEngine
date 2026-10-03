#pragma once

namespace Viva {

class GameObject;
class Transform;

// The base class of everything that can be attached to a GameObject: Unity's Component and
// MonoBehaviour in one. A game derives from it and overrides the On... functions it needs:
//
//     class Spinner : public Viva::Component {
//     protected:
//         void OnUpdate(float dt) override { /* turn GetTransform() a little */ }
//     };
//
//     crate.AddComponent<Spinner>();
//
// GameObject::AddComponent creates components, and the GameObject owns them. The constructor runs
// before the component is attached, so (like in Unity) set-up that needs the GameObject belongs
// in OnStart, not in the constructor.
class Component {
public:
    // A class with virtual functions needs a virtual destructor: the GameObject destroys its
    // components through Component pointers, and this makes that run the derived class's
    // destructor too. A component's destructor is the place for clean-up (Unity's OnDestroy). It
    // runs when its GameObject is removed after Scene::Destroy, or at the end of
    // Application::Run, when the scene goes.
    virtual ~Component() = default;

    Component(const Component&) = delete;
    Component& operator=(const Component&) = delete;

    // The GameObject this component is attached to, and that object's Transform: Unity's
    // gameObject and transform.
    GameObject& GetGameObject() const { return *m_GameObject; }
    Transform& GetTransform() const;

protected:
    // Protected: only derived classes construct a Component, never code that wants a plain one.
    Component() = default;

    // Called by the scene for components whose GameObject is active, like their Unity namesakes:
    // OnStart once, before the component's first update (Start); OnUpdate every frame (Update);
    // OnFixedUpdate at the fixed rate, 50 times a second by default (FixedUpdate).
    virtual void OnStart() {}
    virtual void OnUpdate(float /*dt*/) {}
    virtual void OnFixedUpdate(float /*fixedDt*/) {}

private:
    // GameObject attaches the component and calls its On... functions.
    friend class GameObject;
    GameObject* m_GameObject = nullptr;
    bool m_Started = false; // OnStart has been called
};

} // namespace Viva
