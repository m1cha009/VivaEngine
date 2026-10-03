#pragma once

#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>

namespace Viva {

// Where the scene is seen from, and how it's projected onto the screen. Like Unity's Camera,
// without the GameObject (that comes in M10). The game moves it; the renderer draws from it.
//
// The world's axes are right-handed with Y up, as in glTF and most 3D tools: +X right, +Y up,
// and a camera with Yaw and Pitch at 0 looks down -Z. (Unity is left-handed and looks down +Z.)
struct Camera {
    glm::vec3 Position { 0.0f, 0.0f, 0.0f };
    // Rotation in radians (GLM's unit). Yaw turns around the world's up axis, positive to the
    // left. Pitch tilts the view, positive looking up.
    float Yaw = 0.0f;
    float Pitch = 0.0f;
    // Vertical field of view, in radians (Unity's default is 60 degrees).
    float FieldOfView = glm::radians(60.0f);
    // Only what lies between these distances is drawn (Unity's clipping planes).
    float NearPlane = 0.1f;
    float FarPlane = 500.0f;

    // The camera's own transform, from Position, Yaw and Pitch: like a Unity Transform's
    // localToWorldMatrix.
    glm::mat4 WorldMatrix() const;
    // Unit vectors: where the camera looks, and its right-hand side (always level).
    glm::vec3 Forward() const;
    glm::vec3 Right() const;

    // The view matrix moves the world so the camera sits at the origin looking down -Z.
    glm::mat4 ViewMatrix() const;
    // The projection matrix adds perspective and fits the view into clip space. Like
    // Unity's Camera.projectionMatrix it follows OpenGL's convention, with y pointing up; the
    // renderer converts it for Vulkan. aspect: the image's width divided by its height.
    glm::mat4 ProjectionMatrix(float aspect) const;
};

} // namespace Viva
