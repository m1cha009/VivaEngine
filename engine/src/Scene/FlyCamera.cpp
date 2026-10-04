#include "Viva/FlyCamera.h"

#include "Viva/FieldVisitor.h"
#include "Viva/Input.h"
#include "Viva/Transform.h"

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>

namespace Viva {

namespace {

// How far the view tilts up or down: just short of straight up or down, so it can't flip over.
constexpr float kMaxPitch = glm::radians(89.0f);

} // namespace

void FlyCamera::VisitFields(FieldVisitor& fields)
{
    fields.Field("MoveSpeed", Settings.MoveSpeed);
    fields.Field("LookSensitivity", Settings.LookSensitivity);
}

bool Fly(glm::vec3& position, glm::quat& rotation, float dt, const FlySettings& settings)
{
    // Looking happens while the right mouse button is held. Meanwhile the cursor is locked: hidden
    // and frozen, so the view can keep turning past the edge of the screen.
    const bool looking = Input::GetMouseButton(MouseButton::Right);
    Input::SetCursorLocked(looking);

    if (looking) {
        // Where the camera looks now, as yaw (turned left from +Z, around the world's up axis) and
        // pitch (tilted up). They're read back from the rotation rather than kept here, so
        // changes made elsewhere, like in an inspector, are respected.
        const glm::vec3 forward = rotation * Transform::kForwardAxis;
        float yaw = std::atan2(forward.x, forward.z);
        float pitch = std::asin(std::clamp(forward.y, -1.0f, 1.0f));

        // Moving the mouse right turns right (yaw goes down); moving it up looks up (screen y
        // grows downwards, so pitch goes the opposite way).
        const glm::vec2 delta = Input::MouseDelta();
        yaw -= delta.x * settings.LookSensitivity;
        pitch = std::clamp(pitch - delta.y * settings.LookSensitivity, -kMaxPitch, kMaxPitch);

        // Quaternions combine like matrices, applied right to left: tilt around the camera's own
        // X axis, then turn by yaw around the world's up axis (Y). A positive turn around X tips
        // the front (+Z) down, so tilting up is -pitch. The camera never rolls.
        rotation = glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::angleAxis(-pitch, glm::vec3(1.0f, 0.0f, 0.0f));
    }
    if (settings.MoveOnlyWhileLooking && !looking)
        return false;

    // Moving: add up the directions of the keys held, relative to where the camera looks. Up and
    // down (Q and E) are the world's, like the Scene view.
    const glm::vec3 forward = rotation * Transform::kForwardAxis;
    const glm::vec3 right = rotation * Transform::kRightAxis;
    glm::vec3 direction(0.0f);
    if (Input::GetKey(Key::W))
        direction += forward;
    if (Input::GetKey(Key::S))
        direction -= forward;
    if (Input::GetKey(Key::D))
        direction += right;
    if (Input::GetKey(Key::A))
        direction -= right;
    if (Input::GetKey(Key::E))
        direction.y += 1.0f;
    if (Input::GetKey(Key::Q))
        direction.y -= 1.0f;

    // Normalized, so moving diagonally isn't faster. Multiplying by dt makes the speed the same at
    // any frame rate, as with Time.deltaTime in Unity.
    if (glm::length(direction) > 0.0f) {
        const bool fast = Input::GetKey(Key::LeftShift) || Input::GetKey(Key::RightShift);
        const float speed = settings.MoveSpeed * (fast ? 3.0f : 1.0f);
        position += glm::normalize(direction) * speed * dt;
    }
    return looking;
}

void FlyCamera::OnUpdate(float dt)
{
    // A root object's local position and rotation are its world ones.
    Transform& transform = GetTransform();
    Fly(transform.LocalPosition, transform.LocalRotation, dt, Settings);
}

} // namespace Viva
