#pragma once

#include "EditorMenus.h"

#include <glm/vec3.hpp>

#include <array>
#include <functional>
#include <optional>

class SceneEditor;

namespace Viva {
class GameObject;
}

// The Hierarchy window: the scene's GameObjects as a tree, like Unity's. Click to select, drag one
// onto another to make it a child (or onto empty space to make it a root object), F2 (Edit >
// Rename) to rename the selection in place, and right-click for the Create menu and the Edit
// menu's commands. The edits go through the SceneEditor.
class HierarchyWindow {
public:
    // `placement`: where new root objects go (in front of the Scene view's camera). Returns the
    // Edit command picked in the right-click menu, if any, for the selection (a right-click
    // selects first), which the editor carries out like the Edit menu's.
    std::optional<EditCommand> Draw(SceneEditor& editor, const glm::vec3& placement, bool* open);

    // Shows a text field in place of the selection's name (F2, Edit > Rename).
    void StartRename(const Viva::GameObject& selection);

    // Whether the window had the keyboard focus this frame: the Edit keys act on the selection
    // only while the Hierarchy or the Scene view has it, not while typing in the Console's filter.
    bool IsFocused() const { return m_Focused; }

private:
    void DrawNode(SceneEditor& editor, Viva::GameObject& gameObject, bool reveal);
    void DrawRenameField(SceneEditor& editor, Viva::GameObject& gameObject);
    void DrawEmptySpace(SceneEditor& editor, const glm::vec3& placement);

    // Edits that add or move objects can't happen while the tree is being drawn: the loops walking
    // the scene's lists would find them changed under their feet. They wait here, and run once
    // the tree is done.
    std::function<void()> m_Deferred;
    std::optional<EditCommand> m_Command; // picked in a right-click menu this frame

    // The selection the tree last revealed. When the selection changes (a new object, a click in
    // the Scene view), the nodes above it open and the tree scrolls to it, as in Unity; after that
    // they can be closed again. Only compared, never followed: it may point at a deleted object.
    const Viva::GameObject* m_Revealed = nullptr;

    // Renaming always concerns the selection, and stops when the selection changes.
    bool m_Renaming = false;
    bool m_FocusRenameField = false; // the field appears this frame: give it the keyboard
    std::array<char, 256> m_NameBuffer {}; // the name being typed
    bool m_Focused = false;
};
