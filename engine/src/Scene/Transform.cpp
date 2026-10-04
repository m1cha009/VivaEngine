#include "Viva/Transform.h"

#include "Viva/Assert.h"
#include "Viva/FieldVisitor.h"
#include "Viva/GameObject.h"

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat3x3.hpp>

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

glm::quat Transform::GetRotation() const
{
    // Like WorldMatrix: the parent's rotation, then this one's on top (applied right to left).
    return m_Parent ? m_Parent->GetRotation() * LocalRotation : LocalRotation;
}

void Transform::LookAt(const glm::vec3& worldPoint, const glm::vec3& worldUp)
{
    // The wanted rotation, built from its three axes in world space (the columns of its matrix):
    // +Z towards the point, +X at right angles to both up and +Z (that's what a cross product
    // gives), and +Y at right angles to the other two. A zero cross product means the point is
    // straight along up, or at the object itself. (GLM has this as glm::quatLookAtLH, where "LH"
    // means +Z towards the point: this engine's Forward.)
    const glm::vec3 toPoint = worldPoint - GetPosition();
    const glm::vec3 xAxis = glm::cross(worldUp, toPoint);
    if (glm::length(xAxis) < 1e-6f)
        return;
    const glm::vec3 z = glm::normalize(toPoint);
    const glm::vec3 x = glm::normalize(xAxis);
    const glm::vec3 y = glm::cross(z, x);
    const glm::quat worldRotation = glm::quat_cast(glm::mat3(x, y, z));

    // LocalRotation is relative to the parent, so the parent's rotation is taken back out: the
    // inverse of a rotation undoes it.
    LocalRotation = m_Parent ? glm::inverse(m_Parent->GetRotation()) * worldRotation : worldRotation;
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

Transform* Transform::Find(std::string_view path) const
{
    // Split the path at its first "/": the name to look for here, and the rest for that child to
    // find below itself. A std::string_view is a view of characters stored elsewhere, so cutting
    // it into pieces copies nothing.
    const size_t slash = path.find('/');
    const std::string_view name = path.substr(0, slash);
    for (Transform* child : m_Children) {
        if (child->m_GameObject->GetName() != name)
            continue;
        if (slash == std::string_view::npos)
            return child;
        // If the rest of the path isn't below this child, another one with the same name may have it.
        if (Transform* found = child->Find(path.substr(slash + 1)))
            return found;
    }
    return nullptr;
}

void Transform::VisitFields(FieldVisitor& fields)
{
    fields.Field("Position", LocalPosition);
    fields.Field("Rotation", LocalRotation);
    fields.Field("Scale", LocalScale);
}

} // namespace Viva
