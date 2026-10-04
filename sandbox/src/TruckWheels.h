#pragma once

#include "Viva/Component.h"

#include <vector>

namespace Viva {
class Transform;
}

// Turns the milk truck model's wheels (M11) so that they roll at Speed, in meters per second, the
// speed the truck moves at. Put it on the truck's top GameObject, the one Model::Instantiate
// returns. It's the one place that knows the model's wheels: where the axles are in its hierarchy,
// which way they turn, and how big the wheels are.
class TruckWheels : public Viva::Component {
public:
    float Speed = 0.0f;

    void VisitFields(Viva::FieldVisitor& fields) override;

protected:
    void OnStart() override;
    void OnUpdate(float dt) override;

private:
    std::vector<Viva::Transform*> m_Axles; // found by name in OnStart (the scene owns them)
};
