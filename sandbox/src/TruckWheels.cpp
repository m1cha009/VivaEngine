#include "TruckWheels.h"

#include "Spinner.h"

#include "Viva/GameObject.h"
#include "Viva/Log.h"
#include "Viva/Transform.h"

#include <glm/trigonometric.hpp>

using namespace Viva;

namespace {

// The wheels are 0.86 m across (see the wheel mesh's size in the file).
constexpr float kWheelRadius = 0.43f;

// The two axles, by their path in the model's hierarchy (the Scene window shows it).
constexpr const char* kAxles[] = { "Yup2Zup/Cesium_Milk_Truck/Node/Wheels",
                                   "Yup2Zup/Cesium_Milk_Truck/Node.001/Wheels.001" };

} // namespace

void TruckWheels::OnStart()
{
    // A Spinner turns each axle, found by name (Transform::Find). They're added here rather than in
    // the constructor, which runs before this component is attached to the truck. An axle is Y in
    // the file's own axes (the file's "Yup2Zup" node turns those so that its Z points up), and
    // rolling forward turns the wheels the negative way around it.
    for (const char* path : kAxles) {
        if (Transform* axle = GetTransform().Find(path))
            m_Spinners.push_back(&axle->GetGameObject().AddComponent<Spinner>(glm::vec3(0.0f, -1.0f, 0.0f), 0.0f));
        else
            Log::Warn("TruckWheels: {} has no {}", GetGameObject().GetName(), path);
    }
}

void TruckWheels::OnUpdate(float /*dt*/)
{
    // To roll rather than slide, a wheel must cover its circumference, 2π × radius, in each turn:
    // Speed / radius radians per second.
    for (Spinner* spinner : m_Spinners)
        spinner->DegreesPerSecond = glm::degrees(Speed / kWheelRadius);
}
