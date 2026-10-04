#include "Viva/Spinner.h"

#include "Viva/FieldVisitor.h"
#include "Viva/Transform.h"

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>

namespace Viva {

Spinner::Spinner(const glm::vec3& axis, float degreesPerSecond)
    : Axis(axis)
    , DegreesPerSecond(degreesPerSecond)
{
}

void Spinner::VisitFields(FieldVisitor& fields)
{
    fields.Field("Axis", Axis);
    fields.Field("DegreesPerSecond", DegreesPerSecond);
}

void Spinner::OnUpdate(float dt)
{
    // This frame's small turn, put in front of the current rotation: applied after it, around the
    // parent's axis. (Multiplied the other way round, it would turn around the object's own,
    // already turned, axis.) Normalizing stops the rounding errors of thousands of
    // multiplications from adding up.
    Transform& transform = GetTransform();
    const glm::quat step = glm::angleAxis(glm::radians(DegreesPerSecond) * dt, glm::normalize(Axis));
    transform.LocalRotation = glm::normalize(step * transform.LocalRotation);
}

} // namespace Viva
