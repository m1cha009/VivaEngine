#include "Viva/GameObject.h"

namespace Viva {

GameObject::GameObject(Scene& scene, std::string name)
    : m_Scene(&scene)
    , m_Name(std::move(name))
    , m_Transform(*this)
{
}

bool GameObject::IsActiveInHierarchy() const
{
    // Active only if this object and every object above it are.
    for (const Transform* transform = &m_Transform; transform; transform = transform->GetParent()) {
        if (!transform->GetGameObject().m_Active)
            return false;
    }
    return true;
}

// The loops below count with an index rather than a range-based for: a component's OnStart or
// OnUpdate may add components to this very object. Adding to a std::vector can move its elements
// to a bigger block of memory, which would leave a range-based for loop reading the old one. (The
// components themselves stay where they are: the vector only holds unique_ptrs to them.)

void GameObject::StartComponents()
{
    for (size_t i = 0; i < m_Components.size() && CanUpdate(); ++i) {
        Component& component = *m_Components[i];
        if (!component.m_Started) {
            // Marked first, so a component started in this pass isn't started twice.
            component.m_Started = true;
            component.OnStart();
        }
    }
}

// A template used only in this file can be defined here rather than in the header: every
// version the compiler needs (one per lambda below) is made in this file.
template <typename Callback>
void GameObject::ForEachStartedComponent(Callback callback)
{
    // CanUpdate is asked again before each component: one may destroy or deactivate its own object.
    for (size_t i = 0; i < m_Components.size() && CanUpdate(); ++i) {
        Component& component = *m_Components[i];
        if (component.m_Started)
            callback(component);
    }
}

void GameObject::UpdateComponents(float dt)
{
    // [dt] captures dt by value: the lambda keeps its own copy, like a C# closure would.
    ForEachStartedComponent([dt](Component& component) { component.OnUpdate(dt); });
}

void GameObject::LateUpdateComponents(float dt)
{
    ForEachStartedComponent([dt](Component& component) { component.OnLateUpdate(dt); });
}

void GameObject::FixedUpdateComponents(float fixedDt)
{
    ForEachStartedComponent([fixedDt](Component& component) { component.OnFixedUpdate(fixedDt); });
}

void GameObject::MarkDestroyed()
{
    m_Destroyed = true;
    for (Transform* child : m_Transform.GetChildren())
        child->GetGameObject().MarkDestroyed();
}

} // namespace Viva
