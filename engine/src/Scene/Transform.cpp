#include "Viva/Transform.h"

#include "Viva/Assert.h"
#include "Viva/GameObject.h"

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Viva {

Transform::Transform(GameObject& gameObject)
    : m_GameObject(&gameObject)
{
}

Transform::~Transform()
{
    if (m_Parent)
        std::erase(m_Parent->m_Children, this);
    for (Transform* child : m_Children)
        child->m_Parent = nullptr;
}

glm::mat4 Transform::LocalMatrix() const
{
    // Read right to left: scale the object, then turn it (mat4_cast turns the quaternion into a
    // rotation matrix), then move it into place.
    return glm::translate(glm::mat4(1.0f), LocalPosition) * glm::mat4_cast(LocalRotation) *
           glm::scale(glm::mat4(1.0f), LocalScale);
}

glm::mat4 Transform::WorldMatrix() const
{
    // The local matrix takes a point from this object's space into its parent's, and the
    // parent's world matrix takes it on from there into the world. Each ancestor adds its own
    // step, which is why a child follows everything above it.
    return m_Parent ? m_Parent->WorldMatrix() * LocalMatrix() : LocalMatrix();
}

// A world matrix's columns are the object's own axes as the world sees them: column 0 its +X, 1
// its +Y, 2 its +Z, and column 3 where its origin is. Normalizing removes the scale.
glm::vec3 Transform::GetPosition() const
{
    return glm::vec3(WorldMatrix()[3]);
}

glm::vec3 Transform::Forward() const
{
    return glm::normalize(glm::vec3(WorldMatrix()[2]));
}

glm::vec3 Transform::Right() const
{
    return -glm::normalize(glm::vec3(WorldMatrix()[0]));
}

glm::vec3 Transform::Up() const
{
    return glm::normalize(glm::vec3(WorldMatrix()[1]));
}

void Transform::SetParent(Transform* parent)
{
    if (parent == m_Parent)
        return;

    // A transform can't go below itself or below one of its own children: the hierarchy would
    // become a loop, and WorldMatrix would never finish. Release builds skip such a call.
    bool wouldLoop = false;
    for (const Transform* ancestor = parent; ancestor && !wouldLoop; ancestor = ancestor->m_Parent)
        wouldLoop = ancestor == this;
    VIVA_ASSERT(!wouldLoop, "SetParent would put {} below itself", m_GameObject->GetName());
    if (wouldLoop)
        return;

    // std::erase (C++20) removes every element equal to the value, like C#'s List.Remove.
    if (m_Parent)
        std::erase(m_Parent->m_Children, this);
    m_Parent = parent;
    if (m_Parent)
        m_Parent->m_Children.push_back(this);
}

} // namespace Viva
