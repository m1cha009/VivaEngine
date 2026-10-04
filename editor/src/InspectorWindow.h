#pragma once

namespace Viva {
class Assets;
class GameObject;
}

// The Inspector window: the selected GameObject's Transform and components, field by field, drawn
// from their VisitFields (M13) by a visitor that makes a widget per field. Editing a value changes
// the object right away, and returns true for that frame: the scene has unsaved changes. `assets`
// makes the material a changed material setting asks for.
bool DrawInspectorWindow(Viva::GameObject* selection, Viva::Assets& assets, bool* open);
