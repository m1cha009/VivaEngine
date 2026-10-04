#pragma once

#include "Viva/FlyCamera.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>

// The camera the Scene view looks through, like Unity's Scene view camera. It isn't part of the
// scene: it isn't a GameObject, isn't saved, and doesn't change what the game's own cameras see.
// It's a position and a rotation that Fly (Viva/FlyCamera.h) moves, plus the lens.
class EditorCamera {
public:
    glm::vec3 Position { 0.0f, 3.5f, -8.0f };
    glm::quat Rotation = glm::identity<glm::quat>(); // facing +Z
    float FieldOfView = glm::radians(60.0f);
    float NearPlane = 0.1f;
    float FarPlane = 1000.0f;
    // As in Unity's Scene view: look and move only while the right mouse button is held, so the
    // keys are free for shortcuts the rest of the time.
    Viva::FlySettings Fly { .MoveSpeed = 8.0f, .MoveOnlyWhileLooking = true };

    // Flies with the mouse and keyboard. Returns whether it's looking around (right button held).
    bool Update(float dt);

    // Moves back from `target`, keeping the direction it looks in, until the target is `distance`
    // away straight ahead: Unity's Frame Selected (F).
    void Frame(const glm::vec3& target, float distance);

    // Like Camera's: the view matrix, and an OpenGL-style projection (see Viva/Camera.h).
    glm::mat4 ViewMatrix() const;
    glm::mat4 ProjectionMatrix(float aspect) const;
};
