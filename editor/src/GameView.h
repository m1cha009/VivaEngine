#pragma once

#include <memory>

namespace Viva {
class Renderer;
class RenderTarget;
class Scene;
}

// The Game window: the scene as the game sees it, through the scene's main camera (Unity's Game
// view), next to the Scene view's editor camera. Both are render targets: the renderer draws the
// same scene into each, from its own camera, in the same frame. In Play mode, the mouse and
// keyboard over it go to the game (through Input), as they do in a built game.
class GameView {
public:
    void Draw(Viva::Renderer& renderer, const Viva::Scene& scene, bool* open);

private:
    std::shared_ptr<Viva::RenderTarget> m_Target; // made on first use, when the renderer exists
};
