#pragma once

#include "Viva/Component.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

namespace Viva {

// How flying responds to the keyboard and mouse (see FlyCamera and Fly).
struct FlySettings {
    float MoveSpeed = 5.0f;         // world units per second
    float LookSensitivity = 0.003f; // radians per point of mouse movement
    // Move only while looking (the right mouse button is held), as in Unity's Scene view. Off,
    // W/A/S/D always move.
    bool MoveOnlyWhileLooking = false;
};

// Flies a position and a rotation in world space like Unity's Scene view camera in flythrough
// mode:
//   right mouse button + mouse   look around (the cursor hides while the button is held)
//   W A S D                      forward, left, back, right
//   Q E                          down, up
//   Shift                        faster
// For cameras that aren't components, like the editor's Scene view camera; FlyCamera uses it for
// GameObjects. Returns whether it's looking around.
bool Fly(glm::vec3& position, glm::quat& rotation, float dt, const FlySettings& settings);

// A component that flies its GameObject (see Fly). Put it on the GameObject with the Camera, a
// root object (it moves the object in world space). Unlike the Scene view, WASD works without
// holding the right mouse button. It came from the sandbox (M6) into the engine in M14, as a
// built-in component that any scene file (and so any project) can use.
class FlyCamera : public Component {
public:
    FlySettings Settings;

    void VisitFields(FieldVisitor& fields) override;

protected:
    void OnUpdate(float dt) override;
};

} // namespace Viva
