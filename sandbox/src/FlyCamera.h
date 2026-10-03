#pragma once

#include "Viva/Camera.h"

// Moves a camera like Unity's Scene view in flythrough mode:
//   right mouse button + mouse   look around (the cursor hides while the button is held)
//   W A S D                      forward, left, back, right
//   Q E                          down, up
//   Shift                        faster
// Unlike the Scene view, WASD works without holding the right mouse button.
class FlyCamera {
public:
    void Update(Viva::Camera& camera, float dt) const;

    float MoveSpeed = 5.0f;          // world units per second
    float LookSensitivity = 0.003f;  // radians per point of mouse movement
};
