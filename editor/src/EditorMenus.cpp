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
// the shortcut a menu item shows is the one that works.
struct EditCommandInfo {
    EditCommand Command;
    const char* Label;
    const char* ShortcutText;
    Key ShortcutKey;
    bool Ctrl = false;
    bool Shift = false;
    bool OnSelection = false;     // acts on the selection: needs one, and its key only works in some windows
    bool SeparatorBefore = false; // a line above it in the menu
};
constexpr EditCommandInfo kEditCommands[] = {
    { .Command = EditCommand::Undo, .Label = "Undo", .ShortcutText = "Ctrl+Z", .ShortcutKey = Key::Z,
      .Ctrl = true },
    { .Command = EditCommand::Redo, .Label = "Redo", .ShortcutText = "Ctrl+Y", .ShortcutKey = Key::Y,
      .Ctrl = true },
    { .Command = EditCommand::Play, .Label = "Play", .ShortcutText = "Ctrl+P", .ShortcutKey = Key::P,
      .Ctrl = true, .SeparatorBefore = true },
    { .Command = EditCommand::Pause, .Label = "Pause", .ShortcutText = "Ctrl+Shift+P", .ShortcutKey = Key::P,
      .Ctrl = true, .Shift = true },
    { .Command = EditCommand::Duplicate, .Label = "Duplicate", .ShortcutText = "Ctrl+D", .ShortcutKey = Key::D,
      .Ctrl = true, .OnSelection = true, .SeparatorBefore = true },
    { .Command = EditCommand::Rename, .Label = "Rename", .ShortcutText = "F2", .ShortcutKey = Key::F2,
      .OnSelection = true },
    { .Command = EditCommand::Delete, .Label = "Delete", .ShortcutText = "Del", .ShortcutKey = Key::Delete,
      .OnSelection = true },
    { .Command = EditCommand::FrameSelected, .Label = "Frame Selected", .ShortcutText = "F", .ShortcutKey = Key::F,
      .OnSelection = true, .SeparatorBefore = true },
};

const EditCommandInfo& GetInfo(EditCommand command)
{
    for (const EditCommandInfo& info : kEditCommands) {
        if (info.Command == command)
            return info;
    }
    return kEditCommands[0]; // every command is in the table
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

bool ActsOnSelection(EditCommand command)
{
    return GetInfo(command).OnSelection;
}

std::optional<EditCommand> DrawEditMenuItems(const EditMenuState& state, bool selectionOnly)
{
    std::optional<EditCommand> clicked;
    bool first = true;
    for (const EditCommandInfo& info : kEditCommands) {
        if (selectionOnly && !info.OnSelection)
            continue;
        if (info.SeparatorBefore && !first)
            ImGui::Separator();
        first = false;
        bool enabled = state.HasSelection;
        bool checked = false;
        switch (info.Command) {
        case EditCommand::Undo:
            enabled = state.CanUndo;
            break;
        case EditCommand::Redo:
            enabled = state.CanRedo;
            break;
        case EditCommand::Play:
            enabled = true;
            checked = state.Playing;
            break;
        case EditCommand::Pause:
            enabled = state.Playing;
            checked = state.Paused;
            break;
        default:
            break;
        }
        // A check mark when `checked`; greyed out when it can't do anything.
        if (ImGui::MenuItem(info.Label, info.ShortcutText, checked, enabled))
            clicked = info.Command;
    }
    return clicked;
}

std::optional<EditCommand> ReadEditShortcut(bool selectionKeys)
{
    const bool ctrl = IsCtrlHeld();
    const bool shift = IsShiftHeld();
    for (const EditCommandInfo& info : kEditCommands) {
        if ((selectionKeys || !info.OnSelection) && Input::GetKeyDown(info.ShortcutKey) && info.Ctrl == ctrl &&
            info.Shift == shift)
            return info.Command;
    }
    return std::nullopt;
}
