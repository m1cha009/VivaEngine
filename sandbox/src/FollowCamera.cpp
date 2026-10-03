#include "FollowCamera.h"

#include "Smoothing.h"

#include "Viva/Transform.h"

FollowCamera::FollowCamera(const Viva::Transform& target)
    : m_Target(&target)
{
}

void FollowCamera::OnStart()
{
    // Start in place, rather than swinging in sideways from wherever the camera was put.
    GetTransform().LocalPosition = m_Target->GetPosition() + Offset;
}

void FollowCamera::OnLateUpdate(float dt)
{
    const glm::vec3 target = m_Target->GetPosition();
    const glm::vec3 wanted = target + Offset;
    glm::vec3& position = GetTransform().LocalPosition;

    // Height and distance are kept exactly: a camera lagging behind a truck at 40 m/s would fall
    // far back. Sideways it eases after the target.
    position.x = SmoothTowards(position.x, wanted.x, SideSharpness, dt);
    position.y = wanted.y;
    position.z = wanted.z;
    GetTransform().LookAt(target + LookAhead);
}
