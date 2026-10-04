#include "Viva/Camera.h"

#include "Viva/FieldVisitor.h"
#include "Viva/Transform.h"

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Viva {

glm::mat4 Camera::ViewMatrix() const
{
    // lookAt builds the view from where the camera is, a point it looks at and which way is up.
    // They're read from the camera's world matrix (column 3 is its position, columns 1 and 2 its
    // Up and Forward axes; see Transform.cpp), normalized so that a scaled parent doesn't stretch
    // the view. The camera's own Up is always at right angles to Forward, so this works even when
    // looking straight up or down.
    const glm::mat4 world = GetTransform().WorldMatrix();
    const glm::vec3 position(world[3]);
    const glm::vec3 forward = glm::normalize(glm::vec3(world[2]));
    const glm::vec3 up = glm::normalize(glm::vec3(world[1]));
    return glm::lookAt(position, position + forward, up);
}

glm::mat4 Camera::ProjectionMatrix(float aspect) const
{
    // Depth comes out as 0 (near plane) to 1 (far plane), because GLM_FORCE_DEPTH_ZERO_TO_ONE is
    // set (see cmake/Dependencies.cmake).
    return glm::perspective(FieldOfView, aspect, NearPlane, FarPlane);
}

void Camera::VisitFields(FieldVisitor& fields)
{
    // In radians, like the field itself.
    fields.Field("FieldOfView", FieldOfView);
    fields.Field("NearPlane", NearPlane);
    fields.Field("FarPlane", FarPlane);
    fields.Field("BackgroundColor", BackgroundColor);
}

} // namespace Viva
