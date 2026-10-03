#pragma once

#include "Viva/Renderer.h"

#include <memory>

// The scene from M6 and M7, now built by the game through the engine's public API: a checker
// floor, a ring of colored pillars and a spinning crate.
class DemoScene {
public:
    // Creates the meshes and materials. Call it once, from OnStart.
    void Load(Viva::Renderer& renderer);
    // Submits this frame's draws. Call it every frame, from OnUpdate.
    void Draw(Viva::Renderer& renderer, float seconds) const;

private:
    // Shared pointers, like references in C#: the scene keeps these resources alive, and the
    // renderer frees them (a few frames later) once nothing holds them any more.
    std::shared_ptr<Viva::Mesh> m_CubeMesh;
    std::shared_ptr<Viva::Mesh> m_PillarMesh;
    std::shared_ptr<Viva::Mesh> m_FloorMesh;
    std::shared_ptr<Viva::Material> m_CrateMaterial;
    std::shared_ptr<Viva::Material> m_PillarMaterial;
    std::shared_ptr<Viva::Material> m_FloorMaterial;
};
