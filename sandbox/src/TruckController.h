#pragma once

#include "Road.h"

#include "Viva/Component.h"

class TruckWheels;

// Drives the milk truck down the road: forward along +Z, faster and faster, and sideways between
// the lanes (Road.h) with A/D or the arrow keys. Put it on the truck's top GameObject, the one
// Model::Instantiate returns. It turns the wheels through a TruckWheels component it adds.
class TruckController : public Viva::Component {
public:
    float StartSpeed = 14.0f;  // meters per second when a run starts (50 km/h)
    float MaxSpeed = 40.0f;    // 144 km/h
    float Acceleration = 0.4f; // meters per second gained every second

    float GetSpeed() const { return m_Speed; }

    // Sets off at StartSpeed. From then on the speed rises and the lane keys work.
    void Drive();
    // Stops dead, after a crash.
    void Stop();
    // Back to the start: standing still in the middle lane.
    void Reset();

protected:
    void OnStart() override;
    void OnUpdate(float dt) override;

private:
    TruckWheels* m_Wheels = nullptr; // on the same GameObject, which owns it
    int m_Lane = Road::kStartLane;
    float m_Speed = 0.0f;
    float m_Steer = 0.0f; // radians: how far the nose is turned into a lane change
    bool m_Driving = false;
};
