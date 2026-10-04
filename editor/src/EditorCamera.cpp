#include "EditorCamera.h"

#include "Viva/Transform.h"

#include <glm/gtc/matrix_transform.hpp>

using Viva::Transform;

bool EditorCamera::Update(float dt)
{
    return Viva::Fly(Position, Rotation, dt, Fly);
}

void EditorCamera::Frame(const glm::vec3& target, float distance)
{
    Position = target - Rotation * Transform::kForwardAxis * distance;
}

glm::mat4 EditorCamera::ViewMatrix() const
{
    // The same as Camera::ViewMatrix, from the position and rotation instead of a Transform.
    return glm::lookAt(Position, Position + Rotation * Transform::kForwardAxis, Rotation * Transform::kUpAxis);
}

glm::mat4 EditorCamera::ProjectionMatrix(float aspect) const
{
    return glm::perspective(FieldOfView, aspect, NearPlane, FarPlane);
}
