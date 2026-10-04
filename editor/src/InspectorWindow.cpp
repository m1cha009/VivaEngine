// GLM's "gtx" extensions are marked experimental, so GLM asks for this define before one is
// included. Here it's for gtx/euler_angles.hpp, below.
#define GLM_ENABLE_EXPERIMENTAL

#include "InspectorWindow.h"

#include "Viva/Assets.h"
#include "Viva/Component.h"
#include "Viva/ComponentRegistry.h"
#include "Viva/FieldVisitor.h"
#include "Viva/GameObject.h"
#include "Viva/Renderer.h"
#include "Viva/Transform.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/trigonometric.hpp>
#include <imgui.h>

#include <cctype>
#include <memory>
#include <string>
#include <string_view>
#include <typeinfo>
#include <utility>
#include <vector>

using namespace Viva;

namespace {

// A field's name as a label: "DegreesPerSecond" becomes "Degrees Per Second", as Unity's inspector
// shows "degreesPerSecond" (ObjectNames.NicifyVariableName).
std::string NiceName(std::string_view name)
{
    std::string nice;
    for (size_t i = 0; i < name.size(); ++i) {
        const auto c = static_cast<unsigned char>(name[i]);
        if (i > 0 && std::isupper(c) && !std::isupper(static_cast<unsigned char>(name[i - 1])))
            nice += ' ';
        nice += static_cast<char>(c);
    }
    return nice;
}

// Fields whose names end in "Color" get a color picker. FieldVisitor only knows types, and a color
// is a vec3 or vec4 like any other; Unity knows from the field's type (Color).
bool IsColor(std::string_view name)
{
    return name.ends_with("Color");
}

// Draws a widget per field, in the window being built, and edits the field in place: ImGui's
// widgets take a pointer to the value they show, and change it when the user drags or types.
class InspectorVisitor final : public FieldVisitor {
public:
    explicit InspectorVisitor(Assets& assets)
        : m_Assets(assets)
    {
    }

    // Whether any widget changed its field.
    bool Changed() const { return m_Changed; }

    void Field(std::string_view name, float& value) override
    {
        if (Visible())
            m_Changed |= ImGui::DragFloat(Label(name), &value, 0.05f);
    }
    void Field(std::string_view name, glm::vec2& value) override
    {
        if (Visible())
            m_Changed |= ImGui::DragFloat2(Label(name), &value.x, 0.05f);
    }
    void Field(std::string_view name, glm::vec3& value) override
    {
        if (!Visible())
            return;
        // A glm::vec3's x, y and z lie one after another in memory, so &value.x is the array of
        // three floats ImGui asks for.
        m_Changed |= IsColor(name) ? ImGui::ColorEdit3(Label(name), &value.x)
                                   : ImGui::DragFloat3(Label(name), &value.x, 0.05f);
    }
    void Field(std::string_view name, glm::vec4& value) override
    {
        if (!Visible())
            return;
        m_Changed |= IsColor(name) ? ImGui::ColorEdit4(Label(name), &value.x)
                                   : ImGui::DragFloat4(Label(name), &value.x, 0.05f);
    }

    // A rotation is shown as three angles in degrees around X, Y and Z ("Euler angles"), converted
    // both ways every frame. They're taken apart in Unity's order: a turn around Z, then X, then
    // Y, so the usual turn, around Y, covers the full -180..180, and only the tilt around X is
    // limited to -90..90 (past that, the angles jump to another set meaning the same rotation;
    // Unity's inspector avoids even that by remembering the angles typed in).
    void Field(std::string_view name, glm::quat& value) override
    {
        if (!Visible())
            return;
        glm::vec3 angles; // x: around X, y: around Y, z: around Z
        glm::extractEulerAngleYXZ(glm::mat4_cast(value), angles.y, angles.x, angles.z);
        angles = glm::degrees(angles);
        if (ImGui::DragFloat3(Label(name), &angles.x, 0.5f)) {
            const glm::vec3 radians = glm::radians(angles);
            value = glm::quat_cast(glm::eulerAngleYXZ(radians.y, radians.x, radians.z));
            m_Changed = true;
        }
    }

    // Meshes and textures show their asset names. Choosing another comes with M16's pickers.
    void Field(std::string_view name, std::shared_ptr<Mesh>& value) override
    {
        if (Visible())
            ShowAssetName(name, value ? &Renderer::GetAssetName(*value) : nullptr);
    }
    void Field(std::string_view name, std::shared_ptr<Texture>& value) override
    {
        if (Visible())
            ShowAssetName(name, value ? &Renderer::GetAssetName(*value) : nullptr);
    }

    // A material is its settings: shown in a tree node of their own, and a change asks Assets for
    // the material with the new settings. Materials are shared (M13), so changing one object's
    // color makes a new material for it, rather than recoloring every object that used the old one.
    void Field(std::string_view name, std::shared_ptr<Material>& value) override
    {
        if (!Visible())
            return;
        if (!value) {
            ImGui::LabelText(Label(name), "(none)");
            return;
        }
        if (!ImGui::TreeNodeEx(Label(name), ImGuiTreeNodeFlags_DefaultOpen))
            return;
        MaterialSettings settings = Renderer::GetSettings(*value);
        const bool changedBefore = std::exchange(m_Changed, false);
        VisitFields(*this, settings);
        if (m_Changed)
            value = m_Assets.GetMaterial(settings);
        m_Changed |= changedBefore;
        ImGui::TreePop();
    }

protected:
    // A list is a tree node, and each element a numbered one inside it. While a node is closed,
    // its fields are still visited (FieldVisitor::List visits every element), but nothing is drawn.
    size_t BeginList(std::string_view name, size_t size) override
    {
        m_Open.push_back(Visible() && ImGui::TreeNodeEx(Label(name), ImGuiTreeNodeFlags_DefaultOpen));
        return size;
    }
    void BeginElement(size_t index) override
    {
        const bool parentOpen = m_Open.back();
        ImGui::PushID(static_cast<int>(index));
        m_Open.push_back(parentOpen && ImGui::TreeNodeEx("Element", ImGuiTreeNodeFlags_DefaultOpen, "%zu", index));
    }
    void EndElement() override
    {
        if (m_Open.back())
            ImGui::TreePop();
        m_Open.pop_back();
        ImGui::PopID();
    }
    void EndList() override
    {
        if (m_Open.back())
            ImGui::TreePop();
        m_Open.pop_back();
    }

private:
    bool Visible() const { return m_Open.empty() || m_Open.back(); }

    // ImGui labels are C strings, and also IDs. The label is kept in a member, so the pointer stays
    // valid while the widget uses it.
    const char* Label(std::string_view name)
    {
        m_Label = NiceName(name);
        return m_Label.c_str();
    }

    void ShowAssetName(std::string_view name, const std::string* assetName)
    {
        const char* text = !assetName ? "(none)" : assetName->empty() ? "(made in code)" : assetName->c_str();
        ImGui::LabelText(Label(name), "%s", text);
    }

    Assets& m_Assets;
    std::vector<bool> m_Open; // for each list and element being visited: is its tree node open?
    std::string m_Label;
    bool m_Changed = false;
};

} // namespace

bool DrawInspectorWindow(GameObject* selection, Assets& assets, bool* open)
{
    // Begin returns false when the window is collapsed or its tab hidden; End is called either way.
    if (!ImGui::Begin("Inspector", open)) {
        ImGui::End();
        return false;
    }
    if (!selection) {
        ImGui::TextDisabled("Select a GameObject in the Hierarchy.");
        ImGui::End();
        return false;
    }
    bool changed = false;

    // Widgets end this far from the window's right edge, leaving room for their labels (ImGui puts
    // labels on the right). A negative item width means "the available width minus this".
    ImGui::PushItemWidth(-ImGui::GetFontSize() * 9.0f);

    // The name, and the object's own Active switch: below an inactive parent it stays hidden
    // either way. (Renaming comes with M16.)
    bool active = selection->IsActiveSelf();
    if (ImGui::Checkbox("##Active", &active)) {
        selection->SetActive(active);
        changed = true;
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(selection->GetName().c_str());
    ImGui::Separator();

    InspectorVisitor visitor(assets);
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
        selection->GetTransform().VisitFields(visitor);

    // Every component, as a header named after its registered type. Fields with the same name in
    // two components ("Color") would be the same widget to ImGui, so each component gets an ID
    // scope of its own.
    const std::vector<std::unique_ptr<Component>>& components = selection->GetComponents();
    for (size_t i = 0; i < components.size(); ++i) {
        Component& component = *components[i];
        const std::string_view type = ComponentRegistry::NameOf(component);
        const std::string title = type.empty() ? typeid(component).name() : NiceName(type);
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::CollapsingHeader(title.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            if (type.empty())
                ImGui::TextDisabled("Not registered, so scene files leave it out (see ComponentRegistry).");
            component.VisitFields(visitor);
        }
        ImGui::PopID();
    }
    changed |= visitor.Changed();
    ImGui::PopItemWidth();
    ImGui::End();
    return changed;
}
