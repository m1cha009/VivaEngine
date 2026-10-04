#include "EditorMenus.h"

#include "EditorUi.h"

#include <imgui.h>

using namespace Viva;

namespace {

// Unity's names. The ones with a mesh go in the 3D Object submenu.
constexpr NewObjectInfo kNewObjects[] = {
    { NewObject::Empty, "GameObject", nullptr },
    { NewObject::Cube, "Cube", "Primitives::Cube" },
    { NewObject::Plane, "Plane", "Primitives::Plane" },
    { NewObject::Sphere, "Sphere", "Primitives::Sphere" },
    { NewObject::Cylinder, "Cylinder", "Primitives::Cylinder" },
    { NewObject::Camera, "Camera", nullptr },
};

// Each Edit command with its menu label and its key: one table for the menu and the keyboard, so
// the shortcut a menu item shows is the one that works. The rest act on the selection.
struct EditCommandInfo {
    EditCommand Command;
    const char* Label;
    const char* ShortcutText;
    Key ShortcutKey;
    bool Ctrl;
    bool SeparatorBefore; // a line above it in the menu
};
constexpr EditCommandInfo kEditCommands[] = {
    { EditCommand::Undo, "Undo", "Ctrl+Z", Key::Z, true, false },
    { EditCommand::Redo, "Redo", "Ctrl+Y", Key::Y, true, false },
    { EditCommand::Duplicate, "Duplicate", "Ctrl+D", Key::D, true, true },
    { EditCommand::Rename, "Rename", "F2", Key::F2, false, false },
    { EditCommand::Delete, "Delete", "Del", Key::Delete, false, false },
    { EditCommand::FrameSelected, "Frame Selected", "F", Key::F, false, true },
};

bool IsUndoOrRedo(EditCommand command)
{
    return command == EditCommand::Undo || command == EditCommand::Redo;
}

} // namespace

const NewObjectInfo& GetNewObjectInfo(NewObject kind)
{
    for (const NewObjectInfo& info : kNewObjects) {
        if (info.Kind == kind)
            return info;
    }
    return kNewObjects[0]; // every kind is in the table
}

std::optional<NewObject> DrawCreateMenuItems()
{
    std::optional<NewObject> clicked;
    if (ImGui::MenuItem("Create Empty"))
        clicked = NewObject::Empty;
    if (ImGui::BeginMenu("3D Object")) {
        for (const NewObjectInfo& info : kNewObjects) {
            if (info.Mesh && ImGui::MenuItem(info.Name))
                clicked = info.Kind;
        }
        ImGui::EndMenu();
    }
    if (ImGui::MenuItem("Camera"))
        clicked = NewObject::Camera;
    return clicked;
}

std::optional<EditCommand> DrawEditMenuItems(const EditMenuState& state, bool selectionOnly)
{
    std::optional<EditCommand> clicked;
    bool first = true;
    for (const EditCommandInfo& info : kEditCommands) {
        if (selectionOnly && IsUndoOrRedo(info.Command))
            continue;
        if (info.SeparatorBefore && !first)
            ImGui::Separator();
        first = false;
        const bool enabled = info.Command == EditCommand::Undo   ? state.CanUndo
                             : info.Command == EditCommand::Redo ? state.CanRedo
                                                                 : state.HasSelection;
        // The last argument greys the item out when it can't do anything.
        if (ImGui::MenuItem(info.Label, info.ShortcutText, false, enabled))
            clicked = info.Command;
    }
    return clicked;
}

std::optional<EditCommand> ReadEditShortcut(bool selectionKeys)
{
    const bool ctrl = IsCtrlHeld();
    for (const EditCommandInfo& info : kEditCommands) {
        if ((selectionKeys || IsUndoOrRedo(info.Command)) && Input::GetKeyDown(info.ShortcutKey) && info.Ctrl == ctrl)
            return info.Command;
    }
    return std::nullopt;
}
