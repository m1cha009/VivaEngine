#pragma once

#include "Viva/Component.h"

namespace Viva {
class Transform;
}

// Keeps a long object, like the road or the grass, under a moving target, so it never ends. The
// object jumps along Z in whole steps of Period, the length of one repeat of its texture: after
// each jump the texture lies exactly where it was, so the jump can't be seen. It's a treadmill under
// the truck. (Endless runners either do this, or keep adding tiles ahead and removing them behind.)
// Like a camera, it follows in OnLateUpdate, once the target has moved.
class Treadmill : public Viva::Component {
public:
    Treadmill(const Viva::Transform& target, float period);

    float Period;

protected:
    void OnStart() override;
    void OnLateUpdate(float dt) override;

private:
    const Viva::Transform* m_Target; // not owned
    float m_StartZ = 0.0f;           // where the object was placed, kept as its offset
};
