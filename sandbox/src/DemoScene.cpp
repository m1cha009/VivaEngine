#include "DemoScene.h"

#include "FlyCamera.h"
#include "Spinner.h"

#include "Viva/Camera.h"
#include "Viva/MeshRenderer.h"
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
    // 24 x 24 units, with the checker texture (2x2 squares) repeated 12 times along each side:
    // one square per unit.
    const std::shared_ptr<Mesh> floorMesh = renderer.CreateMesh(Primitives::Plane(24.0f, 12.0f));
    const std::shared_ptr<Material> crateMaterial = renderer.CreateMaterial({ .Texture = renderer.LoadTexture("textures/crate.png") });
    const std::shared_ptr<Material> floorMaterial = renderer.CreateMaterial({ .Texture = renderer.LoadTexture("textures/checker.png") });
    const std::shared_ptr<Material> pillarMaterial = renderer.CreateMaterial({});

    GameObject& floor = scene.CreateGameObject("Floor");
    floor.AddComponent<MeshRenderer>(floorMesh, floorMaterial);

    // The pillars are children of one object that turns slowly: they ride along, each keeping its
    // place in the ring, without any code of their own. The ring starts turned by half a step, so
    // no pillar stands between the starting camera and the crate.
    GameObject& ring = scene.CreateGameObject("Pillar ring");
    ring.AddComponent<Spinner>(glm::vec3(0.0f, 1.0f, 0.0f), 6.0f);
    for (int i = 0; i < kPillarCount; ++i) {
        const float angle = glm::two_pi<float>() * (static_cast<float>(i) + 0.5f) / static_cast<float>(kPillarCount);
        constexpr float kRadius = 6.0f;
        constexpr float kHeight = 3.0f;
        // std::format builds the name like C#'s string interpolation: "Pillar 1", "Pillar 2"...
        GameObject& pillar = scene.CreateGameObject(std::format("Pillar {}", i + 1), &ring);
        Transform& transform = pillar.GetTransform();
        transform.LocalPosition = { kRadius * std::cos(angle), kHeight / 2.0f, kRadius * std::sin(angle) };
        transform.LocalScale = { 0.6f, kHeight, 0.6f }; // a cube stretched tall
        pillar.AddComponent<MeshRenderer>(pillarMesh, pillarMaterial);
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

    // The camera: a little above the floor and back from the middle (at +Z), looking at it. Read
    // right to left: tilted 12 degrees down (a positive turn around X tips the front down, as in
    // Unity), then turned around (180 degrees around Y) to face -Z, towards the middle.
    GameObject& camera = scene.CreateGameObject("Camera");
    camera.GetTransform().LocalPosition = { 0.0f, 3.0f, 9.0f };
    camera.GetTransform().LocalRotation = glm::angleAxis(glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f)) *
                                          glm::angleAxis(glm::radians(12.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    camera.AddComponent<Camera>();
    camera.AddComponent<FlyCamera>();
}
