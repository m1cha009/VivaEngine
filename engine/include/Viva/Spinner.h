#pragma once

#include "Viva/Component.h"

#include <glm/vec3.hpp>

namespace Viva {

// A component that keeps turning its GameObject around an axis, at a steady speed. The axis is in
// the parent's space (the world's, for an object without a parent). It came from the sandbox
// (M10) into the engine in M13, so scenes made without code, such as the editor's, can use it.
class Spinner : public Component {
public:
    // A quarter turn per second around Y: what a Spinner does when a scene file or the editor
    // adds one without saying more.
    Spinner() = default;
    Spinner(const glm::vec3& axis, float degreesPerSecond);

    glm::vec3 Axis { 0.0f, 1.0f, 0.0f };
    float DegreesPerSecond = 90.0f;

    void VisitFields(FieldVisitor& fields) override;

protected:
    void OnUpdate(float dt) override;
};

} // namespace Viva
