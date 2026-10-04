#include "TruckWheels.h"

#include "Viva/FieldVisitor.h"
#include "Viva/GameObject.h"
#include "Viva/Log.h"
#include "Viva/Transform.h"

#include <glm/gtc/quaternion.hpp>

using namespace Viva;

namespace {

// The wheels are 0.86 m across (see the wheel mesh's size in the file).
constexpr float kWheelRadius = 0.43f;

// The two axles, by their path in the model's hierarchy (the Scene window shows it).
constexpr const char* kAxles[] = { "Yup2Zup/Cesium_Milk_Truck/Node/Wheels",
                                   "Yup2Zup/Cesium_Milk_Truck/Node.001/Wheels.001" };

} // namespace

void TruckWheels::VisitFields(FieldVisitor& fields)
{
    fields.Field("Speed", Speed);
}

void TruckWheels::OnStart()
{
    // The axles are found by name (Transform::Find). That happens here rather than in the
    // constructor, which runs before this component is attached to the truck.
    for (const char* path : kAxles) {
        if (Transform* axle = GetTransform().Find(path))
            m_Axles.push_back(axle);
        else
            Log::Warn("TruckWheels: {} has no {}", GetGameObject().GetName(), path);
    }
}

void TruckWheels::OnUpdate(float dt)
{
    // To roll rather than slide, a wheel must cover its circumference, 2π × radius, in each turn:
    // Speed / radius radians per second. An axle is Y in the file's own axes (the file's "Yup2Zup"
    // node turns those so that its Z points up), and rolling forward turns the wheels the negative
    // way around it. Each axle turns the way a Spinner turns its object (see Spinner.cpp). Turning
    // them here, rather than adding a Spinner to each axle, keeps the scene as it was built: a
    // component that adds components while the game runs would find them already there in a scene
    // saved afterwards.
    const glm::quat step = glm::angleAxis(Speed / kWheelRadius * dt, glm::vec3(0.0f, -1.0f, 0.0f));
    for (Transform* axle : m_Axles)
        axle->LocalRotation = glm::normalize(step * axle->LocalRotation);
}
