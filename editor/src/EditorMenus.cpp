#include "EditorMenus.h"

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
    bool Ctrl;
};
constexpr EditCommandInfo kEditCommands[] = {
    { EditCommand::Duplicate, "Duplicate", "Ctrl+D", Key::D, true },
    { EditCommand::Rename, "Rename", "F2", Key::F2, false },
    { EditCommand::Delete, "Delete", "Del", Key::Delete, false },
    { EditCommand::FrameSelected, "Frame Selected", "F", Key::F, false },
};

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

std::optional<EditCommand> DrawEditMenuItems(bool hasSelection)
{
    std::optional<EditCommand> clicked;
    for (const EditCommandInfo& info : kEditCommands) {
        if (info.Command == EditCommand::FrameSelected)
            ImGui::Separator();
        // The last argument greys the item out: these need a selection.
        if (ImGui::MenuItem(info.Label, info.ShortcutText, false, hasSelection))
            clicked = info.Command;
    }
    return clicked;
}

std::optional<EditCommand> ReadEditShortcut()
{
    const bool ctrl = Input::GetKey(Key::LeftControl) || Input::GetKey(Key::RightControl);
    for (const EditCommandInfo& info : kEditCommands) {
        if (Input::GetKeyDown(info.ShortcutKey) && info.Ctrl == ctrl)
            return info.Command;
    }
    return std::nullopt;
}
