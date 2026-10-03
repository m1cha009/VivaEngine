#pragma once

#include "Viva/Component.h"

#include <vector>

class Spinner;

// Turns the milk truck model's wheels (M11) so that they roll at Speed, in meters per second, the
// speed the truck moves at. Put it on the truck's top GameObject, the one Model::Instantiate
// returns. It's the one place that knows the model's wheels: where the axles are in its hierarchy,
// which way they turn, and how big the wheels are.
class TruckWheels : public Viva::Component {
public:
    float Speed = 0.0f;

protected:
    void OnStart() override;
    void OnUpdate(float dt) override;

private:
    std::vector<Spinner*> m_Spinners; // one per axle (the axles' GameObjects own them)
};
