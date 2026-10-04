#pragma once

#include <span>
#include <string>

class SceneEditor;

// The Inspector window: the selected GameObject's name, Transform and components, field by field,
// drawn from their VisitFields (M13) by a visitor that makes a widget per field. Editing a value
// changes the object right away. Below the components, Add Component; right-clicking a
// component's header removes it. `textureNames`: the project's image files, for texture pickers.
void DrawInspectorWindow(SceneEditor& editor, std::span<const std::string> textureNames, bool* open);
