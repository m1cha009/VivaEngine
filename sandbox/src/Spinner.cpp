#include "Spinner.h"

#include "Viva/Transform.h"

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>

Spinner::Spinner(const glm::vec3& axis, float degreesPerSecond)
    : Axis(axis)
    , DegreesPerSecond(degreesPerSecond)
{
}

void Spinner::OnUpdate(float dt)
{
    // This frame's small turn, put in front of the current rotation: applied after it, around the
    // parent's axis. (Multiplied the other way round, it would turn around the object's own,
    // already turned, axis.) Normalizing stops the rounding errors of thousands of
    // multiplications from adding up.
    Viva::Transform& transform = GetTransform();
    const glm::quat step = glm::angleAxis(glm::radians(DegreesPerSecond) * dt, glm::normalize(Axis));
    transform.LocalRotation = glm::normalize(step * transform.LocalRotation);
}
