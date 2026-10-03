#include "TruckController.h"

#include "Road.h"
#include "Smoothing.h"
#include "TruckWheels.h"

#include "Viva/GameObject.h"
#include "Viva/Input.h"
#include "Viva/Transform.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>

using namespace Viva;

namespace {

// How fast the truck slides sideways into the next lane: 3.5 m in about 0.3 seconds. Real trucks
// can't, but a game that reacts at once feels better.
constexpr float kSideSpeed = 12.0f;
// How far the nose turns into a lane change, at most, and how quickly it gets there and back.
constexpr float kMaxSteer = glm::radians(8.0f);
constexpr float kSteerSharpness = 15.0f;

} // namespace

void TruckController::Drive()
{
    m_Driving = true;
    m_Speed = StartSpeed;
}

void TruckController::Stop()
{
    m_Driving = false;
    m_Speed = 0.0f;
}

void TruckController::Reset()
{
    Stop();
    m_Lane = Road::kStartLane;
    m_Steer = 0.0f;
    Transform& transform = GetTransform();
    transform.LocalPosition = glm::vec3(Road::LaneX(m_Lane), 0.0f, 0.0f);
    transform.LocalRotation = glm::identity<glm::quat>();
}

void TruckController::OnStart()
{
    // A component may add others to its GameObject (here, not in the constructor, which runs
    // before this component is attached). TruckWheels turns the wheels at the speed set below.
    m_Wheels = &GetGameObject().AddComponent<TruckWheels>();
    Reset();
}

void TruckController::OnUpdate(float dt)
{
    if (m_Driving) {
        if (Input::GetKeyDown(Key::A) || Input::GetKeyDown(Key::LeftArrow))
            m_Lane = std::max(m_Lane - 1, 0);
        if (Input::GetKeyDown(Key::D) || Input::GetKeyDown(Key::RightArrow))
            m_Lane = std::min(m_Lane + 1, Road::kLaneCount - 1);
        m_Speed = std::min(m_Speed + Acceleration * dt, MaxSpeed);
    }

    // Forward at the current speed, and sideways towards the lane at a steady speed. Clamping the
    // step to the distance left makes it stop exactly on the lane, like Unity's Mathf.MoveTowards.
    Transform& transform = GetTransform();
    const float maxSideStep = kSideSpeed * dt;
    const float sideStep = std::clamp(Road::LaneX(m_Lane) - transform.LocalPosition.x, -maxSideStep, maxSideStep);
    transform.LocalPosition.x += sideStep;
    transform.LocalPosition.z += m_Speed * dt;

    // The nose turns into the slide, in proportion to how fast the truck slides, easing there and
    // back. A positive turn around Y swings Forward (+Z) towards +X, the left, which is where a
    // positive step goes.
    const float targetSteer = maxSideStep > 0.0f ? kMaxSteer * sideStep / maxSideStep : 0.0f;
    m_Steer = SmoothTowards(m_Steer, targetSteer, kSteerSharpness, dt);
    transform.LocalRotation = glm::angleAxis(m_Steer, glm::vec3(0.0f, 1.0f, 0.0f));

    m_Wheels->Speed = m_Speed;
}
