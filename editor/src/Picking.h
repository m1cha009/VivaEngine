#pragma once

#include "Viva/MeshData.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <functional>
#include <optional>

namespace Viva {
class GameObject;
class Scene;
}

// A half-line: a starting point and a direction, like Unity's Ray. Clicking in the Scene view
// shoots one from the camera through the mouse pointer into the scene.
struct Ray {
    glm::vec3 Origin { 0.0f };
    glm::vec3 Direction { 0.0f, 0.0f, 1.0f }; // length 1
};

// Calls `visit` with the bounding box of each mesh a GameObject draws (its MeshRenderer's parts),
// and the world matrix that places it, if the object is drawn at all: active, and not destroyed.
// With `withChildren`, the same for every object below it. Picking, framing and the selection
// outline all go through here, so they agree on what counts.
void ForEachDrawnBox(const Viva::GameObject& gameObject, bool withChildren,
                     const std::function<void(const Viva::Bounds& bounds, const glm::mat4& world)>& visit);

// How far along `ray` it first meets `bounds`, placed in the world by `world` (a GameObject's world
// matrix), or std::nullopt if it misses: the same distance a RaycastHit gives in Unity. A ray
// starting inside the box meets it at distance 0.
std::optional<float> IntersectBounds(const Ray& ray, const Viva::Bounds& bounds, const glm::mat4& world);

// The GameObject the ray hits first: the nearest one with a drawn mesh whose bounding box the ray
// meets. Unity's Scene view picks the same way, except that it tests the mesh's triangles; boxes
// are coarser (a click just outside a sphere, inside its box, picks it). Objects that draw nothing
// (cameras, empty objects) can't be clicked.
Viva::GameObject* PickGameObject(const Viva::Scene& scene, const Ray& ray);

// The box, lined up with the world's axes, around everything a GameObject and the objects below it
// draw, or std::nullopt if they draw nothing: what F (Frame Selected) zooms to.
std::optional<Viva::Bounds> GetWorldBounds(const Viva::GameObject& gameObject);
