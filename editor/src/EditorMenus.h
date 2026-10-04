#pragma once

#include "Viva/Input.h"

#include <optional>

// The menus that appear in more than one place: in the menu bar, and in the Hierarchy's
// right-click menu. Each is a table, drawn by one function, so both places stay the same.

// The kinds of GameObject the editor creates, like Unity's GameObject menu.
enum class NewObject {
    Empty,
    Cube,
    Plane,
    Sphere,
    Cylinder,
    Camera,
};

// What a kind of GameObject is called, and the built-in mesh it shows (null for none).
struct NewObjectInfo {
    NewObject Kind;
    const char* Name;
    const char* Mesh;
};
const NewObjectInfo& GetNewObjectInfo(NewObject kind);

// The menu items that create GameObjects: Create Empty, 3D Object > Cube, Plane, Sphere,
// Cylinder, and Camera. Draw them inside an open menu or popup; returns the item clicked.
std::optional<NewObject> DrawCreateMenuItems();

// The Edit menu's commands, as in Unity's Edit menu: Undo and Redo, Play and Pause (M18), and
// those that act on the selection.
enum class EditCommand {
    Undo,
    Redo,
    Play,  // Play, or Stop while playing
    Pause, // pause or resume Play mode
    Duplicate,
    Rename,
    Delete,
    FrameSelected,
};

// What the Edit menu's items can do right now: the others are greyed out. Play and Pause show a
// check mark while playing and paused.
struct EditMenuState {
    bool HasSelection = false;
    bool CanUndo = false;
    bool CanRedo = false;
    bool Playing = false;
    bool Paused = false;
};

// Whether the command acts on the selection (and does nothing without one).
bool ActsOnSelection(EditCommand command);

// The Edit menu's items. `selectionOnly` keeps only those that act on the selection, for the
// Hierarchy's right-click menu, which is about the object clicked. Returns the item clicked.
std::optional<EditCommand> DrawEditMenuItems(const EditMenuState& state, bool selectionOnly);

// The command whose key was pressed this frame (read through Input), if any. Undo, Redo, Play and
// Pause work from any window; the others only if `selectionKeys` (the Hierarchy or the Scene view
// has the focus), so Del in another window doesn't delete the selection. Input doesn't see keys
// while a text field has the keyboard (M9), so typing an F or pressing Delete in a field edits the
// text instead, and Ctrl+Z there undoes the typing.
std::optional<EditCommand> ReadEditShortcut(bool selectionKeys);
