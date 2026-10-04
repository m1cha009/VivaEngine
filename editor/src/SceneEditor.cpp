#include "SceneEditor.h"

#include "Viva/Application.h"
#include "Viva/Assets.h"
#include "Viva/Camera.h"
#include "Viva/ComponentRegistry.h"
#include "Viva/GameObject.h"
#include "Viva/Log.h"
#include "Viva/MeshRenderer.h"
#include "Viva/Model.h"
#include "Viva/Scene.h"

#include <charconv>
#include <format>
#include <memory>
#include <utility>
#include <vector>

using namespace Viva;

namespace {

// The objects next to where a new one goes: its parent's children, or the root objects.
std::vector<const GameObject*> GetSiblings(const Scene& scene, const GameObject* parent)
{
    std::vector<const GameObject*> siblings;
    if (parent) {
        for (const Transform* child : parent->GetTransform().GetChildren())
            siblings.push_back(&child->GetGameObject());
    } else {
        for (const std::unique_ptr<GameObject>& gameObject : scene.GetGameObjects()) {
            if (!gameObject->GetTransform().GetParent())
                siblings.push_back(gameObject.get());
        }
    }
    return siblings;
}

// Unity's names for copies: "Cube" becomes "Cube (1)", or the next number its siblings don't use.
// A copy of "Cube (1)" is numbered from "Cube" too.
std::string CopyName(const Scene& scene, const GameObject& original)
{
    std::string_view base = original.GetName();
    // Strip a " (n)" the original already has. from_chars reads a number without exceptions.
    if (base.ends_with(')')) {
        const size_t open = base.rfind(" (");
        int number = 0;
        if (open != std::string_view::npos) {
            const std::string_view digits = base.substr(open + 2, base.size() - open - 3);
            const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), number);
            if (error == std::errc() && end == digits.data() + digits.size())
                base = base.substr(0, open);
        }
    }

    const std::vector<const GameObject*> siblings =
        GetSiblings(scene, original.GetTransform().GetParent() ? &original.GetTransform().GetParent()->GetGameObject() : nullptr);
    for (int number = 1;; ++number) {
        std::string name = std::format("{} ({})", base, number);
        bool taken = false;
        for (const GameObject* sibling : siblings)
            taken |= sibling->GetName() == name;
        if (!taken)
            return name;
    }
}

} // namespace

SceneEditor::SceneEditor(Application& application)
    : m_Application(application)
{
}

Scene& SceneEditor::GetScene() const
{
    return m_Application.GetScene();
}

Assets& SceneEditor::GetAssets() const
{
    return m_Application.GetAssets();
}

void SceneEditor::SceneReplaced()
{
    m_Selection = nullptr;
    m_Dirty = false;
}

void SceneEditor::Update()
{
    // A raw pointer can't tell by itself that its object is gone, so the scene is asked (see
    // Scene::Contains).
    if (m_Selection && (!GetScene().Contains(m_Selection) || m_Selection->IsDestroyed()))
        m_Selection = nullptr;
}

void SceneEditor::Create(NewObject kind, GameObject* parent, const glm::vec3& position)
{
    // Unity's names and contents (see EditorMenus.cpp). The primitives get a plain white
    // material, which Assets shares between all of them until one is given another color.
    const NewObjectInfo& info = GetNewObjectInfo(kind);
    GameObject& created = GetScene().CreateGameObject(info.Name, parent);
    if (info.Mesh)
        created.AddComponent<MeshRenderer>(GetAssets().GetMesh(info.Mesh), GetAssets().GetMaterial({}));
    if (kind == NewObject::Camera)
        created.AddComponent<Camera>();
    // A child starts at its parent's origin, as in Unity; a root object where it was asked to go.
    if (!parent)
        created.GetTransform().LocalPosition = position;
    m_Selection = &created;
    m_Dirty = true;
}

void SceneEditor::PlaceModel(const std::string& assetName, const glm::vec3& position)
{
    const std::shared_ptr<Model> model = GetAssets().GetModel(assetName);
    if (!model)
        return;
    GameObject& instance = model->Instantiate(GetScene());
    instance.GetTransform().LocalPosition = position;
    m_Selection = &instance;
    m_Dirty = true;
}

void SceneEditor::Duplicate(GameObject& gameObject)
{
    Transform* parent = gameObject.GetTransform().GetParent();
    GameObject& copy = GetScene().Instantiate(gameObject, GetAssets(), parent ? &parent->GetGameObject() : nullptr);
    copy.SetName(CopyName(GetScene(), gameObject));
    m_Selection = &copy;
    m_Dirty = true;
}

void SceneEditor::Delete(GameObject& gameObject)
{
    // Gone at the end of the frame; Update forgets the selection then.
    GetScene().Destroy(gameObject);
    m_Dirty = true;
}

void SceneEditor::Rename(GameObject& gameObject, std::string name)
{
    // A nameless object would be a blank line in the Hierarchy, hard to click.
    if (name.empty() || name == gameObject.GetName())
        return;
    gameObject.SetName(std::move(name));
    m_Dirty = true;
}

void SceneEditor::SetActive(GameObject& gameObject, bool active)
{
    gameObject.SetActive(active);
    m_Dirty = true;
}

bool SceneEditor::CanReparent(const GameObject& child, const GameObject* parent)
{
    return child.GetTransform().CanSetParent(parent ? &parent->GetTransform() : nullptr);
}

void SceneEditor::Reparent(GameObject& child, GameObject* parent)
{
    if (!CanReparent(child, parent))
        return;
    child.GetTransform().SetParent(parent ? &parent->GetTransform() : nullptr, true);
    m_Selection = &child;
    m_Dirty = true;
}

void SceneEditor::AddComponent(GameObject& gameObject, std::string_view type)
{
    if (!ComponentRegistry::Add(gameObject, type)) {
        Log::Error("No component type is registered as {}", type);
        return;
    }
    m_Dirty = true;
}

void SceneEditor::RemoveComponent(Component& component)
{
    // Like Delete: it leaves its GameObject at the end of the frame.
    GetScene().Destroy(component);
    m_Dirty = true;
}
