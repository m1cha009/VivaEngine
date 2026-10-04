#pragma once

namespace Viva {
class Assets;
class Scene;
}

// Builds the demo world as GameObjects: a checker floor, a slowly turning ring of colored pillars
// (children of one "Pillar ring" object) with a logo box on each, a spinning crate with a small
// crate orbiting it (its child), the milk truck driving around them, and the camera you fly with.
// Call it once, from OnStart. The scene owns everything it creates: the meshes and materials live
// as long as the MeshRenderers that use them. Everything comes from Assets, so the whole scene can
// be saved to a scene file (M13).
void LoadDemoScene(Viva::Scene& scene, Viva::Assets& assets);
