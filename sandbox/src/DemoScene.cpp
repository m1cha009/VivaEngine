#include "DemoScene.h"

#include "FlyCamera.h"
#include "Spinner.h"
#include "TruckWheels.h"

#include "Viva/Camera.h"
#include "Viva/MeshRenderer.h"
#include "Viva/Model.h"
#include "Viva/Primitives.h"
#include "Viva/Renderer.h"
#include "Viva/Scene.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>
#include <format>

using namespace Viva;

namespace {

constexpr int kPillarCount = 8;
constexpr float kPillarRingRadius = 6.0f;
constexpr float kPillarHeight = 3.0f;

// The milk truck drives in a circle around the pillars, in meters and seconds.
constexpr float kTruckSpeed = 3.0f;
constexpr float kTruckCircleRadius = 9.5f;

// A cube whose six faces have their own colors: the built-in cube, with its vertex colors
// changed. Its faces come in the order +X, -X, +Y, -Y, +Z, -Z, four vertices each.
MeshData ColoredCube()
{
    constexpr glm::vec3 kFaceColors[] = {
        { 0.90f, 0.20f, 0.20f }, // +X red
        { 0.20f, 0.80f, 0.80f }, // -X cyan
        { 0.30f, 0.85f, 0.30f }, // +Y green
        { 0.80f, 0.30f, 0.80f }, // -Y magenta
        { 0.25f, 0.40f, 0.95f }, // +Z blue
        { 0.95f, 0.85f, 0.20f }, // -Z yellow
    };
    MeshData cube = Primitives::Cube();
    for (size_t i = 0; i < cube.Vertices.size(); ++i)
        cube.Vertices[i].Color = kFaceColors[i / 4];
    return cube;
}

} // namespace

void LoadDemoScene(Scene& scene, Renderer& renderer)
{
    // Meshes and materials, as in M8. They're shared_ptrs, so several MeshRenderers can use one,
    // and each lives as long as something uses it. If a texture fails to load (the log says why),
    // LoadTexture returns null and that material is plain white.
    const std::shared_ptr<Mesh> cubeMesh = renderer.CreateMesh(Primitives::Cube());
    const std::shared_ptr<Mesh> pillarMesh = renderer.CreateMesh(ColoredCube());
    // 28 x 28 units, with the checker texture (2x2 squares) repeated 14 times along each side:
    // one square per unit.
    const std::shared_ptr<Mesh> floorMesh = renderer.CreateMesh(Primitives::Plane(28.0f, 28.0f, glm::vec2(14.0f)));
    const std::shared_ptr<Material> crateMaterial = renderer.CreateMaterial({ .Texture = renderer.LoadTexture("textures/crate.png") });
    const std::shared_ptr<Material> floorMaterial = renderer.CreateMaterial({ .Texture = renderer.LoadTexture("textures/checker.png") });
    const std::shared_ptr<Material> pillarMaterial = renderer.CreateMaterial({});

    // Models (M11) are loaded once, then placed in the scene with Instantiate, as often as needed:
    // every instance shares the model's meshes, materials and textures. Load returns null if a
    // file is missing or broken (the log says why), and the scene does without that model.
    // These unique_ptrs let go of the models at the end of this function; the instances keep the
    // meshes and materials they use alive.
    const std::unique_ptr<Model> logoBox = Model::Load(renderer, "models/BoxTextured/BoxTextured.gltf");
    const std::unique_ptr<Model> truck = Model::Load(renderer, "models/CesiumMilkTruck.glb");

    GameObject& floor = scene.CreateGameObject("Floor");
    floor.AddComponent<MeshRenderer>(floorMesh, floorMaterial);

    // The pillars are children of one object that turns slowly: they ride along, each keeping its
    // place in the ring, without any code of their own. The ring starts turned by half a step, so
    // no pillar stands between the starting camera and the crate.
    GameObject& ring = scene.CreateGameObject("Pillar ring");
    ring.AddComponent<Spinner>(glm::vec3(0.0f, 1.0f, 0.0f), 6.0f);
    for (int i = 0; i < kPillarCount; ++i) {
        const float angle = glm::two_pi<float>() * (static_cast<float>(i) + 0.5f) / static_cast<float>(kPillarCount);
        const float x = kPillarRingRadius * std::cos(angle);
        const float z = kPillarRingRadius * std::sin(angle);
        // std::format builds the name like C#'s string interpolation: "Pillar 1", "Pillar 2"...
        GameObject& pillar = scene.CreateGameObject(std::format("Pillar {}", i + 1), &ring);
        Transform& transform = pillar.GetTransform();
        transform.LocalPosition = { x, kPillarHeight / 2.0f, z };
        transform.LocalScale = { 0.6f, kPillarHeight, 0.6f }; // a cube stretched tall
        pillar.AddComponent<MeshRenderer>(pillarMesh, pillarMaterial);

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
