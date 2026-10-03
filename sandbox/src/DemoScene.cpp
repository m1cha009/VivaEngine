#include "DemoScene.h"

#include "Viva/Primitives.h"

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

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

// Model matrices place a mesh in the world. They're built here as translate * rotate * scale,
// which applies to the mesh right to left: scale it, then turn it, then move it into place, just
// like a Unity Transform's scale, rotation and position.

// The crate in the middle: hovering above the floor and turning around a tilted axis.
glm::mat4 SpinningCubeTransform(float seconds)
{
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.5f, 0.0f));
    model = glm::rotate(model, 0.8f * seconds, glm::normalize(glm::vec3(0.4f, 1.0f, 0.2f)));
    return glm::scale(model, glm::vec3(1.2f));
}

// Pillar i, standing in a ring around the middle: a cube stretched tall. The ring is turned by
// half a step, so no pillar stands between the starting camera and the crate.
glm::mat4 PillarTransform(int i)
{
    const float angle = glm::two_pi<float>() * (static_cast<float>(i) + 0.5f) / static_cast<float>(kPillarCount);
    constexpr float kRadius = 6.0f;
    constexpr float kHeight = 3.0f;
    const glm::mat4 model = glm::translate(glm::mat4(1.0f),
                                           glm::vec3(kRadius * std::cos(angle), kHeight / 2.0f, kRadius * std::sin(angle)));
    return glm::scale(model, glm::vec3(0.6f, kHeight, 0.6f));
}

} // namespace

void DemoScene::Load(Renderer& renderer)
{
    m_CubeMesh = renderer.CreateMesh(Primitives::Cube());
    m_PillarMesh = renderer.CreateMesh(ColoredCube());
    // 24 x 24 units, with the checker texture (2x2 squares) repeated 12 times along each side:
    // one square per unit.
    m_FloorMesh = renderer.CreateMesh(Primitives::Plane(24.0f, 12.0f));

    // Materials without a shader use the engine's Unlit one. Without a texture they're plain
    // white, so the pillars show only their vertex colors. If a texture fails to load (the log
    // says why), LoadTexture returns null and that material is white too.
    m_CrateMaterial = renderer.CreateMaterial({ .Texture = renderer.LoadTexture("textures/crate.png") });
    m_FloorMaterial = renderer.CreateMaterial({ .Texture = renderer.LoadTexture("textures/checker.png") });
    m_PillarMaterial = renderer.CreateMaterial({});
}

void DemoScene::Draw(Renderer& renderer, float seconds) const
{
    renderer.Submit(m_FloorMesh, m_FloorMaterial, glm::mat4(1.0f)); // the identity: the floor as built
    for (int i = 0; i < kPillarCount; ++i)
        renderer.Submit(m_PillarMesh, m_PillarMaterial, PillarTransform(i));
    renderer.Submit(m_CubeMesh, m_CrateMaterial, SpinningCubeTransform(seconds));
}
