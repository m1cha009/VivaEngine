#include "HierarchyWindow.h"

#include "EditorUi.h"
#include "SceneEditor.h"

#include "Viva/GameObject.h"
#include "Viva/Scene.h"
#include "Viva/Transform.h"

#include <imgui.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

using namespace Viva;

namespace {

// The drag-and-drop payload's type: ImGui only delivers a payload to targets that accept its type.
constexpr const char* kPayloadType = "GameObject";

// The object being dragged in the Hierarchy, if a drag of one is under way. The payload is the
// object's address: ImGui copies the payload's bytes, so it's the pointer itself that's copied.
GameObject* GetDraggedObject()
{
    const ImGuiPayload* payload = ImGui::GetDragDropPayload();
    if (!payload || !payload->IsDataType(kPayloadType))
        return nullptr;
    GameObject* dragged = nullptr;
    std::memcpy(&dragged, payload->Data, sizeof(dragged));
    return dragged;
}

// Makes the last item a drop target for dragged objects, if the one being dragged may go below
// `parent` (null: become a root object). Returns the object that was dropped on it.
GameObject* AcceptDrop(const GameObject* parent)
{
    GameObject* dragged = GetDraggedObject();
    if (!dragged || !SceneEditor::CanReparent(*dragged, parent) || !ImGui::BeginDragDropTarget())
        return nullptr;
    GameObject* dropped = ImGui::AcceptDragDropPayload(kPayloadType) ? dragged : nullptr;
    ImGui::EndDragDropTarget();
    return dropped;
}

} // namespace

void HierarchyWindow::StartRename(const GameObject& selection)
{
    m_Renaming = true;
    m_FocusRenameField = true;
    CopyToBuffer(m_NameBuffer, selection.GetName()); // the field starts with the current name
}

std::optional<EditCommand> HierarchyWindow::Draw(SceneEditor& editor, const glm::vec3& placement, bool* open)
{
    // A new selection: its line is revealed, and a rename in progress (of the old one) stops.
    const bool reveal = std::exchange(m_Revealed, editor.GetSelection()) != editor.GetSelection();
    if (reveal && !m_FocusRenameField)
        m_Renaming = false;

    m_Focused = false;
    if (ImGui::Begin("Hierarchy", open)) {
        m_Focused = ImGui::IsWindowFocused();
        // The roots, in the order they were created; each brings its children.
        for (const std::unique_ptr<GameObject>& gameObject : editor.GetScene().GetGameObjects()) {
            if (!gameObject->GetTransform().GetParent())
                DrawNode(editor, *gameObject, reveal);
        }
        DrawEmptySpace(editor, placement);
    }
    ImGui::End();

    // std::exchange takes the waiting edit out and leaves nothing behind, before running it.
    if (const std::function<void()> deferred = std::exchange(m_Deferred, {}))
        deferred();
    return std::exchange(m_Command, std::nullopt);
}

void HierarchyWindow::DrawNode(SceneEditor& editor, GameObject& gameObject, bool reveal)
{
    if (gameObject.IsDestroyed())
        return;
    const std::vector<Transform*>& children = gameObject.GetTransform().GetChildren();
    const GameObject* selection = editor.GetSelection();
    const bool renaming = m_Renaming && &gameObject == selection;

    // A newly selected object's ancestors open, so it can be seen.
    if (reveal && selection && selection->GetTransform().IsBelow(gameObject.GetTransform()))
        ImGui::SetNextItemOpen(true);

    // Open by its arrow or a double-click, so a single click on the name only selects. While it's
    // being renamed, the node doesn't span the window, so the text field fits beside its arrow.
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (!renaming)
        flags |= ImGuiTreeNodeFlags_SpanAvailWidth;
    if (children.empty())
        flags |= ImGuiTreeNodeFlags_Leaf; // no arrow
    if (&gameObject == selection)
        flags |= ImGuiTreeNodeFlags_Selected;

    // Inactive objects are greyed out, as in Unity.
    const bool active = gameObject.IsActiveInHierarchy();
    if (!active)
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    // The object's address is its ID in the tree: names needn't be unique, addresses are.
    const bool open = ImGui::TreeNodeEx(&gameObject, flags, "%s", renaming ? "" : gameObject.GetName().c_str());
    if (!active)
        ImGui::PopStyleColor();
    if (reveal && &gameObject == selection && !ImGui::IsItemVisible())
        ImGui::SetScrollHereY(); // scrolls the window so this line shows

    // A click selects, with either button: the right one also opens the menu below.
    if ((ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right)) &&
        !ImGui::IsItemToggledOpen())
        editor.Select(&gameObject);

    // Dragging: the node is a drag source carrying the object, and a target that takes dragged
    // objects as its children. While dragging, a tooltip with the name follows the mouse.
    if (ImGui::BeginDragDropSource()) {
        GameObject* dragged = &gameObject;
        ImGui::SetDragDropPayload(kPayloadType, &dragged, sizeof(dragged));
        ImGui::TextUnformatted(gameObject.GetName().c_str());
        ImGui::EndDragDropSource();
    }
    if (GameObject* dropped = AcceptDrop(&gameObject))
        m_Deferred = [&editor, dropped, parent = &gameObject] { editor.Reparent(*dropped, parent); };

    // The right-click menu: Create makes the new object a child of this one, as in Unity, and the
    // Edit commands act on it (the right-click selected it).
    if (ImGui::BeginPopupContextItem()) {
        if (const std::optional<NewObject> kind = DrawCreateMenuItems())
            m_Deferred = [&editor, kind, parent = &gameObject] { editor.Create(*kind, parent, {}); };
        ImGui::Separator();
        if (const std::optional<EditCommand> command = DrawEditMenuItems(true))
            m_Command = command;
        ImGui::EndPopup();
    }

    if (renaming)
        DrawRenameField(editor, gameObject);

    if (open) {
        for (Transform* child : children)
            DrawNode(editor, child->GetGameObject(), reveal);
        ImGui::TreePop();
    }
}

void HierarchyWindow::DrawRenameField(SceneEditor& editor, GameObject& gameObject)
{
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.0f); // to the window's right edge
    if (std::exchange(m_FocusRenameField, false))
        ImGui::SetKeyboardFocusHere(); // the next widget gets the keyboard, with its text selected
    // No vertical padding: the field is as tall as the tree's lines, so it sits on its own line.
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 0.0f));
    const bool enter = ImGui::InputText("##Rename", m_NameBuffer.data(), m_NameBuffer.size(),
                                        ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopStyleVar();
    // Enter, or clicking elsewhere, keeps the new name, as in Unity; Escape keeps the old one.
    if (enter || ImGui::IsItemDeactivated()) {
        if (!ImGui::IsKeyPressed(ImGuiKey_Escape))
            editor.Rename(gameObject, m_NameBuffer.data());
        m_Renaming = false;
    }
}

void HierarchyWindow::DrawEmptySpace(SceneEditor& editor, const glm::vec3& placement)
{
    // The rest of the window, below the tree, as an invisible button: an item of its own, so it can
    // take clicks, a right-click menu and dropped objects like the nodes do. It's at least one line
    // high, so there's always somewhere to drop.
    const ImVec2 size(std::max(ImGui::GetContentRegionAvail().x, 1.0f),
                      std::max(ImGui::GetContentRegionAvail().y, ImGui::GetFrameHeight()));
    ImGui::InvisibleButton("##EmptySpace", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

    // A click on empty space clears the selection, as in Unity.
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right))
        editor.Select(nullptr);
    // Dropped here, an object becomes a root object.
    if (GameObject* dropped = AcceptDrop(nullptr))
        m_Deferred = [&editor, dropped] { editor.Reparent(*dropped, nullptr); };
    // Right-click: the Create menu, making root objects in front of the camera.
    if (ImGui::BeginPopupContextItem()) {
        if (const std::optional<NewObject> kind = DrawCreateMenuItems())
            m_Deferred = [&editor, kind, placement] { editor.Create(*kind, nullptr, placement); };
        ImGui::EndPopup();
    }
}
