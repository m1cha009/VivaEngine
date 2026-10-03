#pragma once

namespace Viva {
class Renderer;
class Scene;
}

// Builds the demo world as GameObjects: a checker floor, a slowly turning ring of colored pillars
// (children of one "Pillar ring" object), a spinning crate with a small crate orbiting it (its
// child), and the camera you fly with. Call it once, from OnStart. The scene owns everything it
// creates: the meshes and materials live as long as the MeshRenderers that use them.
void LoadDemoScene(Viva::Scene& scene, Viva::Renderer& renderer);
