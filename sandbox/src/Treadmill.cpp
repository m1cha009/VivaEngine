#include "Treadmill.h"

#include "Viva/Transform.h"

#include <cmath>

Treadmill::Treadmill(const Viva::Transform& target, float period)
    : Period(period)
    , m_Target(&target)
{
}

void Treadmill::OnStart()
{
    m_StartZ = GetTransform().LocalPosition.z;
}

void Treadmill::OnLateUpdate(float /*dt*/)
{
    // std::floor rounds down to whole steps: the target's distance, in steps of Period.
    const float steps = std::floor(m_Target->GetPosition().z / Period);
    GetTransform().LocalPosition.z = m_StartZ + steps * Period;
}
