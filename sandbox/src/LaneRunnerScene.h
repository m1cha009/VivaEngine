#pragma once

namespace Viva {
class Renderer;
class Scene;
}

// Builds the Lane Runner game (M12): the endless road, the milk truck, the chase camera and the
// game's rules. The Sandbox starts with it; --demo shows LoadDemoScene's scene instead.
void LoadLaneRunnerScene(Viva::Scene& scene, Viva::Renderer& renderer);
