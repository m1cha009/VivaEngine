#pragma once

#include "Viva/Component.h"

#include <glm/vec3.hpp>

// A component that keeps turning its GameObject around an axis, at a steady speed. The axis is in
// the parent's space (the world's, for an object without a parent).
class Spinner : public Viva::Component {
public:
    Spinner(const glm::vec3& axis, float degreesPerSecond);

    glm::vec3 Axis;
    float DegreesPerSecond;

protected:
    void OnUpdate(float dt) override;
};
