#pragma once

#include "Viva/Component.h"

#include <glm/vec3.hpp>

namespace Viva {
class Transform;
}

// A chase camera: it stays above and behind a target and looks a little ahead of it, like a
// Cinemachine follow camera in Unity. Sideways it lags behind a little, so a lane change swings
// the view smoothly instead of jerking it. It follows in OnLateUpdate, once every OnUpdate has
// moved things, the way Unity cameras do.
class FollowCamera : public Viva::Component {
public:
    explicit FollowCamera(const Viva::Transform& target);

    // Where to stay, from the target, in world space: above and behind it, high enough to see the
    // road ahead over a truck.
    glm::vec3 Offset { 0.0f, 6.0f, -10.0f };
    // The point to look at, from the target: ahead of it.
    glm::vec3 LookAhead { 0.0f, 0.5f, 14.0f };
    // How quickly the camera catches up sideways. Higher is stiffer.
    float SideSharpness = 6.0f;

protected:
    void OnStart() override;
    void OnLateUpdate(float dt) override;

private:
    // Not owned, like every pointer to a scene object (M10): the target must outlive the camera.
    const Viva::Transform* m_Target;
};
