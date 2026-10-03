#include "Viva/Camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/matrix.hpp>

namespace Viva {

glm::mat4 Camera::WorldMatrix() const
{
    // Read right to left, as always with transforms: tilt by Pitch around the camera's own right
    // axis (X), then turn by Yaw around the world's up axis (Y), then move to Position.
    glm::mat4 world = glm::translate(glm::mat4(1.0f), Position);
    world = glm::rotate(world, Yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    return glm::rotate(world, Pitch, glm::vec3(1.0f, 0.0f, 0.0f));
}

// A transform's columns are the object's own axes in world space: column 0 is its right (+X),
// column 1 its up (+Y), column 2 its back (+Z). A camera looks down its own -Z.
glm::vec3 Camera::Forward() const
{
    return -glm::vec3(WorldMatrix()[2]);
}

glm::vec3 Camera::Right() const
{
    return glm::vec3(WorldMatrix()[0]);
}

glm::mat4 Camera::ViewMatrix() const
{
    // The view matrix undoes the camera's transform: moving the whole world the opposite way puts
    // the camera at the origin, looking down -Z.
    return glm::inverse(WorldMatrix());
}

glm::mat4 Camera::ProjectionMatrix(float aspect) const
{
    // Depth comes out as 0 (near plane) to 1 (far plane), because GLM_FORCE_DEPTH_ZERO_TO_ONE is
    // set (see cmake/Dependencies.cmake).
    return glm::perspective(FieldOfView, aspect, NearPlane, FarPlane);
}

} // namespace Viva
