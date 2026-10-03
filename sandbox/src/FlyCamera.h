#pragma once

#include "Viva/Component.h"

// A component that flies its GameObject like Unity's Scene view camera in flythrough mode. Put it
// on the GameObject with the Camera, a root object (it moves the object in world space):
//   right mouse button + mouse   look around (the cursor hides while the button is held)
//   W A S D                      forward, left, back, right
//   Q E                          down, up
//   Shift                        faster
// Unlike the Scene view, WASD works without holding the right mouse button.
class FlyCamera : public Viva::Component {
public:
    float MoveSpeed = 5.0f;          // world units per second
    float LookSensitivity = 0.003f;  // radians per point of mouse movement

protected:
    void OnUpdate(float dt) override;
};
