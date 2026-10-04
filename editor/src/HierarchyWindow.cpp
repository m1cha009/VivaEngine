#include "HierarchyWindow.h"

#include "Viva/GameObject.h"
#include "Viva/Scene.h"
#include "Viva/Transform.h"

#include <imgui.h>

#include <memory>

using namespace Viva;

namespace {

// One object and, if its node is open, everything below it: recursion again.
void DrawNode(GameObject& gameObject, GameObject*& selection)
{
    if (gameObject.IsDestroyed())
        return;
    const std::vector<Transform*>& children = gameObject.GetTransform().GetChildren();

    // Open by its arrow or a double-click, so a single click on the name only selects.
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                               ImGuiTreeNodeFlags_SpanAvailWidth;
    if (children.empty())
        flags |= ImGuiTreeNodeFlags_Leaf; // no arrow
    if (&gameObject == selection)
        flags |= ImGuiTreeNodeFlags_Selected;

    // Inactive objects are greyed out, as in Unity.
    const bool active = gameObject.IsActiveInHierarchy();
    if (!active)
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    // The object's address is its ID in the tree: names needn't be unique, addresses are.
    const bool open = ImGui::TreeNodeEx(&gameObject, flags, "%s", gameObject.GetName().c_str());
    if (!active)
        ImGui::PopStyleColor();

    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        selection = &gameObject;

    if (open) {
        for (Transform* child : children)
            DrawNode(child->GetGameObject(), selection);
        ImGui::TreePop();
    }
}

} // namespace

void DrawHierarchyWindow(Scene& scene, GameObject*& selection, bool* open)
{
    if (ImGui::Begin("Hierarchy", open)) {
        // The roots, in the order they were created; each brings its children.
        for (const std::unique_ptr<GameObject>& gameObject : scene.GetGameObjects()) {
            if (!gameObject->GetTransform().GetParent())
                DrawNode(*gameObject, selection);
        }
        // A click on empty space in the window clears the selection, as in Unity.
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered())
            selection = nullptr;
    }
    ImGui::End();
}
