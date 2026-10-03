#pragma once

#include "Viva/Component.h"

#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>

namespace Viva {

// A camera, as a component: Unity's Camera. The scene is drawn from the first active GameObject
// that has one (Scene::GetMainCamera), looking along its Transform's Forward (+Z), with its Up at
// the top of the picture, just like Unity.
//
// The world's axes are right-handed with Y up, as in glTF and most 3D tools. Unity's are
// left-handed, which is why an object's Right is -X here but +X there (see Transform.h).
class Camera : public Component {
public:
    // Vertical field of view, in radians (Unity's default is 60 degrees).
    float FieldOfView = glm::radians(60.0f);
    // Only what lies between these distances is drawn (Unity's clipping planes).
    float NearPlane = 0.1f;
    float FarPlane = 500.0f;

    // The view matrix moves and turns the world so the camera sits at the origin, looking down -Z,
    // the direction OpenGL-style projections expect (Unity's worldToCameraMatrix).
    glm::mat4 ViewMatrix() const;
    // The projection matrix adds perspective and fits the view into clip space. Like Unity's
    // Camera.projectionMatrix it follows OpenGL's convention, with y pointing up; the renderer
    // converts it for Vulkan. aspect: the image's width divided by its height.
    glm::mat4 ProjectionMatrix(float aspect) const;
};

} // namespace Viva
