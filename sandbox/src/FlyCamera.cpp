#include "FlyCamera.h"

#include "Viva/Input.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace Viva;

void FlyCamera::Update(Camera& camera, float dt) const
{
    // Looking happens while the right mouse button is held. Meanwhile the cursor is locked: hidden
    // and frozen, so the view can keep turning past the edge of the screen.
    const bool looking = Input::GetMouseButton(MouseButton::Right);
    Input::SetCursorLocked(looking);

    // Moving the mouse right turns right (Yaw goes down); moving it up looks up (screen y grows
    // downwards, so Pitch goes the opposite way). Pitch stops at kMaxPitch.
    if (looking) {
        const glm::vec2 delta = Input::MouseDelta();
        camera.Yaw -= delta.x * LookSensitivity;
        camera.Pitch = std::clamp(camera.Pitch - delta.y * LookSensitivity, -kMaxPitch, kMaxPitch);
    }
    // Turning round and round would make Yaw grow without end. Keeping it within -180..180 degrees
    // (the remainder of a division by a full turn) points the camera the same way.
    camera.Yaw = std::remainder(camera.Yaw, 2.0f * std::numbers::pi_v<float>);

    // Moving: add up the directions of the keys held, relative to where the camera looks. Up and
    // down (Q and E) are the world's, like the Scene view.
    glm::vec3 direction(0.0f);
    if (Input::GetKey(Key::W))
        direction += camera.Forward();
    if (Input::GetKey(Key::S))
        direction -= camera.Forward();
    if (Input::GetKey(Key::D))
        direction += camera.Right();
    if (Input::GetKey(Key::A))
        direction -= camera.Right();
    if (Input::GetKey(Key::E))
        direction.y += 1.0f;
    if (Input::GetKey(Key::Q))
        direction.y -= 1.0f;

    // Normalized, so moving diagonally isn't faster. Multiplying by dt makes the speed the same at
    // any frame rate, as with Time.deltaTime in Unity.
    if (glm::length(direction) > 0.0f) {
        const bool fast = Input::GetKey(Key::LeftShift) || Input::GetKey(Key::RightShift);
        const float speed = MoveSpeed * (fast ? 3.0f : 1.0f);
        camera.Position += glm::normalize(direction) * speed * dt;
    }
}
