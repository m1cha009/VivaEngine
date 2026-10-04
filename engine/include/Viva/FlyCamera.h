#pragma once

#include "Viva/Component.h"

namespace Viva {

// A component that flies its GameObject like Unity's Scene view camera in flythrough mode. Put it
// on the GameObject with the Camera, a root object (it moves the object in world space):
//   right mouse button + mouse   look around (the cursor hides while the button is held)
//   W A S D                      forward, left, back, right
//   Q E                          down, up
//   Shift                        faster
// Unlike the Scene view, WASD works without holding the right mouse button. It came from the
// sandbox (M6) into the engine in M14, because the editor uses it too.
class FlyCamera : public Component {
public:
    float MoveSpeed = 5.0f;          // world units per second
    float LookSensitivity = 0.003f;  // radians per point of mouse movement

    void VisitFields(FieldVisitor& fields) override;

protected:
    void OnUpdate(float dt) override;
};

} // namespace Viva
