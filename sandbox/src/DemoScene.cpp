#include "DemoScene.h"

#include "TruckWheels.h"

#include "Viva/Assets.h"
#include "Viva/Camera.h"
#include "Viva/FlyCamera.h"
#include "Viva/MeshRenderer.h"
#include "Viva/Model.h"
#include "Viva/Primitives.h"
#include "Viva/Scene.h"
#include "Viva/Spinner.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>
#include <format>
#include <iterator>

using namespace Viva;

namespace {

constexpr float kPillarRingRadius = 6.0f;
constexpr float kPillarHeight = 3.0f;

// One pillar per color, around the ring. (Until M13, each pillar's faces had these colors, through
// a mesh with its own vertex colors. A mesh made in code can't be saved in a scene file, so now
// each pillar is a plain cube with a colored material.)
constexpr glm::vec4 kPillarColors[] = {
    { 0.90f, 0.20f, 0.20f, 1.0f }, // red
    { 0.95f, 0.55f, 0.15f, 1.0f }, // orange
    { 0.95f, 0.85f, 0.20f, 1.0f }, // yellow
    { 0.30f, 0.85f, 0.30f, 1.0f }, // green
    { 0.20f, 0.80f, 0.80f, 1.0f }, // cyan
    { 0.25f, 0.40f, 0.95f, 1.0f }, // blue
    { 0.55f, 0.30f, 0.90f, 1.0f }, // violet
    { 0.80f, 0.30f, 0.80f, 1.0f }, // magenta
};

// The milk truck drives in a circle around the pillars, in meters and seconds.
constexpr float kTruckSpeed = 3.0f;
constexpr float kTruckCircleRadius = 9.5f;

// The floor: the built-in plane, scaled up to this size, with the checker texture (2x2 squares)
// repeated so that each square is one unit.
constexpr float kFloorSize = 28.0f;

} // namespace

void LoadDemoScene(Scene& scene, Assets& assets)
{
    // Meshes, textures and materials come from Assets by name, so each is loaded once and
    // shared: every crate uses the same cube mesh and crate material. If a texture fails to load
    // (the log says why), GetTexture returns null and that material is plain white.
    const std::shared_ptr<Mesh> cubeMesh = assets.GetMesh("Primitives::Cube");
    const std::shared_ptr<Material> crateMaterial = assets.GetMaterial({ .Texture = assets.GetTexture("textures/crate.png") });

    // Models (M11) are loaded once, then placed in the scene with Instantiate, as often as needed:
    // every instance shares the model's meshes, materials and textures. GetModel returns null if a
    // file is missing or broken (the log says why), and the scene does without that model.
    const std::shared_ptr<Model> logoBox = assets.GetModel("models/BoxTextured/BoxTextured.gltf");
    const std::shared_ptr<Model> truck = assets.GetModel("models/CesiumMilkTruck.glb");

    GameObject& floor = scene.CreateGameObject("Floor");
    floor.GetTransform().LocalScale = glm::vec3(kFloorSize / Primitives::kPlaneSize, 1.0f, kFloorSize / Primitives::kPlaneSize);
    floor.AddComponent<MeshRenderer>(
        assets.GetMesh("Primitives::Plane"),
        assets.GetMaterial({ .Texture = assets.GetTexture("textures/checker.png"), .Tiling = glm::vec2(kFloorSize / 2.0f) }));

    // The pillars are children of one object that turns slowly: they ride along, each keeping its
    // place in the ring, without any code of their own. The ring starts turned by half a step, so
    // no pillar stands between the starting camera and the crate.
    GameObject& ring = scene.CreateGameObject("Pillar ring");
    ring.AddComponent<Spinner>(glm::vec3(0.0f, 1.0f, 0.0f), 6.0f);
    // std::size gives an array's length, so the ring has as many pillars as there are colors.
    constexpr size_t kPillarCount = std::size(kPillarColors);
    for (size_t i = 0; i < kPillarCount; ++i) {
        const float angle = glm::two_pi<float>() * (static_cast<float>(i) + 0.5f) / static_cast<float>(kPillarCount);
        const float x = kPillarRingRadius * std::cos(angle);
        const float z = kPillarRingRadius * std::sin(angle);
        // std::format builds the name like C#'s string interpolation: "Pillar 1", "Pillar 2"...
        GameObject& pillar = scene.CreateGameObject(std::format("Pillar {}", i + 1), &ring);
        Transform& transform = pillar.GetTransform();
        transform.LocalPosition = { x, kPillarHeight / 2.0f, z };
        transform.LocalScale = { 0.6f, kPillarHeight, 0.6f }; // a cube stretched tall
        pillar.AddComponent<MeshRenderer>(cubeMesh, assets.GetMaterial({ .Color = kPillarColors[i] }));

        // A logo box on top of each pillar: eight instances of one model. It's the ring's child,
        // not the pillar's, because the pillar's stretched scale would stretch it too.
        if (logoBox) {
            Transform& box = logoBox->Instantiate(scene, &ring).GetTransform();
            box.LocalPosition = { x, kPillarHeight + 0.3f, z };
            box.LocalScale = glm::vec3(0.6f); // the model is a 1 m cube
        }
    }

    // The crate in the middle: hovering above the floor and turning around a tilted axis.
    GameObject& crate = scene.CreateGameObject("Crate");
    crate.GetTransform().LocalPosition = { 0.0f, 1.5f, 0.0f };
    crate.GetTransform().LocalScale = glm::vec3(1.2f);
    crate.AddComponent<MeshRenderer>(cubeMesh, crateMaterial);
    crate.AddComponent<Spinner>(glm::vec3(0.4f, 1.0f, 0.2f), 46.0f);

    // A small crate, a child of the big one: it orbits as the big crate turns, and the big crate's
    // scale applies to it too. Its position and size are in the big crate's space, so 1.5 units
    // from the center are 1.8 in the world (1.5 x 1.2). It also spins on its own.
    GameObject& smallCrate = scene.CreateGameObject("Small crate", &crate);
    smallCrate.GetTransform().LocalPosition = { 1.5f, 0.0f, 0.0f };
    smallCrate.GetTransform().LocalScale = glm::vec3(0.3f);
    smallCrate.AddComponent<MeshRenderer>(cubeMesh, crateMaterial);
    smallCrate.AddComponent<Spinner>(glm::vec3(0.0f, 1.0f, 0.0f), 180.0f);

    // The milk truck drives around everything. An empty object in the middle turns, and carries
    // its child, the truck, around with it, like a carousel. Turning the positive way around Y
    // moves a point at +X towards -Z, so the truck, whose front faces +Z like every glTF model,
    // is turned around to face that way. kTruckSpeed / kTruckCircleRadius is how fast the circle
    // must turn, in radians per second, for the truck to drive kTruckSpeed meters per second.
    if (truck) {
        GameObject& pivot = scene.CreateGameObject("Truck pivot");
        pivot.AddComponent<Spinner>(glm::vec3(0.0f, 1.0f, 0.0f), glm::degrees(kTruckSpeed / kTruckCircleRadius));
        GameObject& milkTruck = truck->Instantiate(scene, &pivot);
        milkTruck.GetTransform().LocalPosition = { kTruckCircleRadius, 0.0f, 0.0f };
        milkTruck.GetTransform().LocalRotation = glm::angleAxis(glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));

        // The file animates the wheels, but the engine doesn't play animations yet. TruckWheels
        // turns them instead, at the speed the truck drives, so they roll rather than slide.
        milkTruck.AddComponent<TruckWheels>().Speed = kTruckSpeed;
    }

    // The camera: above the floor and back from the middle (at +Z), outside the truck's circle,
    // looking at the middle. Read right to left: tilted 18 degrees down (a positive turn around X
    // tips the front down, as in Unity), then turned around (180 degrees around Y) to face -Z.
    GameObject& camera = scene.CreateGameObject("Camera");
    camera.GetTransform().LocalPosition = { 0.0f, 5.5f, 15.0f };
    camera.GetTransform().LocalRotation = glm::angleAxis(glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f)) *
                                          glm::angleAxis(glm::radians(18.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    camera.AddComponent<Camera>();
    camera.AddComponent<FlyCamera>();
}
