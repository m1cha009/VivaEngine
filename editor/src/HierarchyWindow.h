#pragma once

namespace Viva {
class GameObject;
class Scene;
}

// The Hierarchy window: the scene's GameObjects as a tree, children below their parents, as in
// Unity. Clicking one selects it; clicking empty space clears the selection.
void DrawHierarchyWindow(Viva::Scene& scene, Viva::GameObject*& selection, bool* open);
