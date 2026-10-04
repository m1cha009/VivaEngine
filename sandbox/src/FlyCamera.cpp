#include "FlyCamera.h"

#include "Viva/FieldVisitor.h"
#include "Viva/Input.h"
#include "Viva/Transform.h"

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>

using namespace Viva;

namespace {

// How far the view tilts up or down: just short of straight up or down, so it can't flip over.
constexpr float kMaxPitch = glm::radians(89.0f);

} // namespace

void FlyCamera::VisitFields(FieldVisitor& fields)
{
    fields.Field("MoveSpeed", MoveSpeed);
    fields.Field("LookSensitivity", LookSensitivity);
}

void FlyCamera::OnUpdate(float dt)
{
    Transform& transform = GetTransform();

    // Looking happens while the right mouse button is held. Meanwhile the cursor is locked: hidden
    // and frozen, so the view can keep turning past the edge of the screen.
    const bool looking = Input::GetMouseButton(MouseButton::Right);
    Input::SetCursorLocked(looking);

    if (looking) {
        // Where the camera looks now, as yaw (turned left from +Z, around the world's up axis) and
        // pitch (tilted up). They're read back from the Transform rather than kept here, so
        // changes made elsewhere, like in the debug UI, are respected.
        const glm::vec3 forward = transform.Forward();
        float yaw = std::atan2(forward.x, forward.z);
        float pitch = std::asin(std::clamp(forward.y, -1.0f, 1.0f));

        // Moving the mouse right turns right (yaw goes down); moving it up looks up (screen y
        // grows downwards, so pitch goes the opposite way).
        const glm::vec2 delta = Input::MouseDelta();
        yaw -= delta.x * LookSensitivity;
        pitch = std::clamp(pitch - delta.y * LookSensitivity, -kMaxPitch, kMaxPitch);

        // Quaternions combine like matrices, applied right to left: tilt around the camera's own
        // X axis, then turn by yaw around the world's up axis (Y). A positive turn around X tips
        // the front (+Z) down, so tilting up is -pitch. The camera never rolls.
        transform.LocalRotation = glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f)) *
                                  glm::angleAxis(-pitch, glm::vec3(1.0f, 0.0f, 0.0f));
    }

    // Moving: add up the directions of the keys held, relative to where the camera looks. Up and
    // down (Q and E) are the world's, like the Scene view.
    glm::vec3 direction(0.0f);
    if (Input::GetKey(Key::W))
        direction += transform.Forward();
    if (Input::GetKey(Key::S))
        direction -= transform.Forward();
    if (Input::GetKey(Key::D))
        direction += transform.Right();
    if (Input::GetKey(Key::A))
        direction -= transform.Right();
    if (Input::GetKey(Key::E))
        direction.y += 1.0f;
    if (Input::GetKey(Key::Q))
        direction.y -= 1.0f;

    // Normalized, so moving diagonally isn't faster. Multiplying by dt makes the speed the same at
    // any frame rate, as with Time.deltaTime in Unity.
    if (glm::length(direction) > 0.0f) {
        const bool fast = Input::GetKey(Key::LeftShift) || Input::GetKey(Key::RightShift);
        const float speed = MoveSpeed * (fast ? 3.0f : 1.0f);
        transform.LocalPosition += glm::normalize(direction) * speed * dt;
    }
}
