#pragma once

// The road's layout, shared by the truck (where each lane is), the game's rules (where the crates
// go) and the road's texture (where the lines are). Change kLaneCount, and all three follow.
namespace Road {

constexpr int kLaneCount = 3;
constexpr float kLaneWidth = 3.5f;     // meters
constexpr float kShoulderWidth = 1.0f; // beyond each edge line
constexpr float kWidth = static_cast<float>(kLaneCount) * kLaneWidth + 2.0f * kShoulderWidth;

// The lane the truck starts in: the middle one (with an even count, the one just right of it).
constexpr int kStartLane = kLaneCount / 2;

// Where a lane's center is, across the road. The lanes are centered on the road, and lane 0 is on
// the driver's left, which is +X: facing +Z, Right() is -X (see Transform.h).
constexpr float LaneX(int lane)
{
    return (static_cast<float>(kLaneCount - 1) / 2.0f - static_cast<float>(lane)) * kLaneWidth;
}

} // namespace Road
