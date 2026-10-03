#pragma once

#include <cmath>

// Eases `current` towards `target`, and returns the new value. Each call closes a share of the
// gap, and the share grows with dt, so that a second of 60 frames and a second of 144 frames close
// the same amount ("exponential smoothing"). The common value = Lerp(value, target, 0.1f), without
// dt, eases faster on a faster PC. sharpness: higher eases faster; in 1 / sharpness seconds, about
// two thirds of the gap closes.
inline float SmoothTowards(float current, float target, float sharpness, float dt)
{
    return current + (target - current) * (1.0f - std::exp(-sharpness * dt));
}
