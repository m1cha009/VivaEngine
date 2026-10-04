#include "Picking.h"

#include "Viva/GameObject.h"
#include "Viva/MeshRenderer.h"
#include "Viva/Renderer.h"
#include "Viva/Scene.h"
#include "Viva/Transform.h"

#include <glm/common.hpp>
#include <glm/matrix.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <utility>

using namespace Viva;

void ForEachDrawnBox(const GameObject& gameObject, bool withChildren,
                     const std::function<void(const Bounds& bounds, const glm::mat4& world)>& visit)
{
    if (gameObject.IsDestroyed() || !gameObject.IsActiveInHierarchy())
        return; // neither it nor anything below it is drawn
    if (const MeshRenderer* meshRenderer = gameObject.GetComponent<MeshRenderer>()) {
        // Every part sits where the object is, so they share one world matrix.
        const glm::mat4 world = gameObject.GetTransform().WorldMatrix();
        for (const MeshPart& part : meshRenderer->Parts) {
            if (part.Mesh)
                visit(Renderer::GetBounds(*part.Mesh), world);
        }
    }
    if (withChildren) {
        for (const Transform* child : gameObject.GetTransform().GetChildren())
            ForEachDrawnBox(child->GetGameObject(), true, visit);
    }
}

std::optional<float> IntersectBounds(const Ray& ray, const Bounds& bounds, const glm::mat4& world)
{
    // The box is lined up with the axes of the object's own space, but may be turned, scaled and
    // moved in the world. So the ray goes into the object's space instead: the inverse of the
    // world matrix takes world points back there. A point is moved (w = 1), a direction only
    // turned and scaled (w = 0). The direction isn't made length 1 again, so "distance t along
    // the ray" means the same point in both spaces, and t stays a world distance.
    if (glm::determinant(world) == 0.0f)
        return std::nullopt; // scaled to nothing in some direction: there's nothing to hit
    const glm::mat4 toLocal = glm::inverse(world);
    const glm::vec3 origin = toLocal * glm::vec4(ray.Origin, 1.0f);
    const glm::vec3 direction = toLocal * glm::vec4(ray.Direction, 0.0f);

    // The "slab" test: the box is where three slabs overlap, one per axis (min.x <= x <= max.x,
    // and so on). The ray is inside each slab between two distances; it's inside the box where
    // all three of those stretches overlap, from the latest entry to the earliest exit.
    float enter = 0.0f; // the ray starts here, so nothing behind it counts
    float exit = std::numeric_limits<float>::max();
    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(direction[axis]) < 1e-8f) {
            // Running alongside this slab: inside it all the way, or never.
            if (origin[axis] < bounds.Min[axis] || origin[axis] > bounds.Max[axis])
                return std::nullopt;
            continue;
        }
        float slabEnter = (bounds.Min[axis] - origin[axis]) / direction[axis];
        float slabExit = (bounds.Max[axis] - origin[axis]) / direction[axis];
        if (slabEnter > slabExit)
            std::swap(slabEnter, slabExit); // the ray runs towards -axis
        enter = std::max(enter, slabEnter);
        exit = std::min(exit, slabExit);
        if (enter > exit)
            return std::nullopt;
    }
    return enter;
}

GameObject* PickGameObject(const Scene& scene, const Ray& ray)
{
    GameObject* nearest = nullptr;
    float nearestDistance = std::numeric_limits<float>::max();
    for (const std::unique_ptr<GameObject>& gameObject : scene.GetGameObjects()) {
        // A lambda capturing by reference ([&]) can update the variables around it.
        ForEachDrawnBox(*gameObject, false, [&](const Bounds& bounds, const glm::mat4& world) {
            const std::optional<float> distance = IntersectBounds(ray, bounds, world);
            if (distance && *distance < nearestDistance) {
                nearestDistance = *distance;
                nearest = gameObject.get();
            }
        });
    }
    return nearest;
}

std::optional<Bounds> GetWorldBounds(const GameObject& gameObject)
{
    // Widened to take in each box as it lies in the world: a turned box's corners stick out in
    // every direction, so all eight are taken in.
    std::optional<Bounds> result;
    ForEachDrawnBox(gameObject, true, [&](const Bounds& bounds, const glm::mat4& world) {
        for (int i = 0; i < 8; ++i) {
            const glm::vec3 corner = world * glm::vec4(bounds.Corner(i), 1.0f);
            if (!result)
                result = Bounds { corner, corner };
            result->Min = glm::min(result->Min, corner);
            result->Max = glm::max(result->Max, corner);
        }
    });
    return result;
}
