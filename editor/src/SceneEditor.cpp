#include "SceneEditor.h"

#include "Viva/Application.h"
#include "Viva/Assert.h"
#include "Viva/Assets.h"
#include "Viva/Camera.h"
#include "Viva/ComponentRegistry.h"
#include "Viva/GameObject.h"
#include "Viva/Json.h"
#include "Viva/Log.h"
#include "Viva/MeshRenderer.h"
#include "Viva/Model.h"
#include "Viva/Scene.h"

#include <charconv>
#include <format>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

using namespace Viva;

namespace {

// A parent's children, or the root objects (for null), leaving out destroyed ones: the objects
// next to where a new one goes.
std::vector<GameObject*> GetSiblings(const Scene& scene, const GameObject* parent)
{
    std::vector<GameObject*> children;
    if (parent) {
        for (const Transform* child : parent->GetTransform().GetChildren()) {
            if (!child->GetGameObject().IsDestroyed())
                children.push_back(&child->GetGameObject());
        }
    } else {
        for (const std::unique_ptr<GameObject>& gameObject : scene.GetGameObjects()) {
            if (!gameObject->GetTransform().GetParent() && !gameObject->IsDestroyed())
                children.push_back(gameObject.get());
        }
    }
    return children;
}

// How many undo steps are kept. A step holds the whole scene as text, about half a kilobyte per
// GameObject; the oldest steps go first.
constexpr size_t kMaxUndoSteps = 100;

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

    const std::vector<GameObject*> siblings =
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
    m_Undo.clear();
    m_Redo.clear();
    m_Changing = false;
    m_Current = TakeSnapshot();
    m_SavedStep = m_Current.Step;
}

void SceneEditor::MarkClean()
{
    FinishChange();
    m_SavedStep = m_Current.Step;
}

void SceneEditor::MarkChanged()
{
    if (m_Playing)
        return; // undone by EndPlay anyway
    // The first change of an edit: the step before it remembers what was selected until now.
    if (!m_Changing) {
        m_Current.Selection = SelectionId();
        m_Changing = true;
    }
}

void SceneEditor::EndFrame(bool userIsEditing)
{
    // A raw pointer can't tell by itself that its object is gone, so the scene is asked (see
    // Scene::Contains).
    if (m_Selection && (!GetScene().Contains(m_Selection) || m_Selection->IsDestroyed()))
        m_Selection = nullptr;
    if (!userIsEditing)
        FinishChange();
}

void SceneEditor::FinishChange()
{
    if (!m_Changing)
        return;
    m_Changing = false;
    Snapshot snapshot = TakeSnapshot();
    // An "edit" that changed nothing (a value typed back to what it was) isn't a step.
    if (snapshot.Scene == m_Current.Scene)
        return;
    m_Undo.push_back(std::move(m_Current));
    if (m_Undo.size() > kMaxUndoSteps)
        m_Undo.erase(m_Undo.begin());
    m_Current = std::move(snapshot);
    // A new edit makes the undone steps unreachable, as in every editor.
    m_Redo.clear();
}

void SceneEditor::BeginPlay()
{
    FinishChange();
    m_Playing = true;
    SetPaused(false);
}

void SceneEditor::SetPaused(bool paused)
{
    m_Paused = paused;
    m_Application.SetSceneUpdating(m_Playing && !m_Paused);
}

void SceneEditor::EndPlay()
{
    m_Playing = false;
    SetPaused(false);
    const uint64_t selected = SelectionId();
    Restore(m_Current);
    if (GameObject* again = GetScene().FindById(selected))
        m_Selection = again;
}

void SceneEditor::Undo()
{
    if (m_Playing)
        return;
    FinishChange(); // an edit still in progress is undone too
    if (m_Undo.empty())
        return;
    m_Redo.push_back(std::move(m_Current));
    m_Current = std::move(m_Undo.back());
    m_Undo.pop_back();
    Restore(m_Current);
}

void SceneEditor::Redo()
{
    if (m_Playing)
        return;
    FinishChange(); // a real edit in progress makes the undone steps unreachable
    if (m_Redo.empty())
        return;
    m_Undo.push_back(std::move(m_Current));
    m_Current = std::move(m_Redo.back());
    m_Redo.pop_back();
    Restore(m_Current);
}

SceneEditor::Snapshot SceneEditor::TakeSnapshot()
{
    return { .Scene = WriteJson(GetScene().Serialize(true)), .Selection = SelectionId(), .Step = ++m_NextStep };
}

void SceneEditor::Restore(const Snapshot& snapshot)
{
    // Clear and load, as when a scene file opens: the old objects are marked destroyed (they go at
    // the end of the frame), and new ones are made from the snapshot. Their meshes, textures and
    // materials are still loaded, so this is quick.
    std::string error;
    const std::optional<Json> json = ParseJson(snapshot.Scene, error);
    VIVA_ASSERT(json, "An undo snapshot doesn't parse: {}", error); // we wrote it ourselves
    if (!json)
        return;
    Scene& scene = GetScene();
    scene.Clear();
    scene.Deserialize(*json, GetAssets(), "the undo history");
    // The objects came back with their Ids (Serialize(true)), so the selection is found again.
    m_Selection = snapshot.Selection != 0 ? scene.FindById(snapshot.Selection) : nullptr;
}

uint64_t SceneEditor::SelectionId() const
{
    return m_Selection ? m_Selection->GetId() : 0;
}

void SceneEditor::Create(NewObject kind, GameObject* parent, const glm::vec3& position)
{
    // Unity's names and contents (see EditorMenus.cpp). The primitives get a plain white
    // material, which Assets shares between all of them until one is given another color.
    MarkChanged();
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
}

void SceneEditor::PlaceModel(const std::string& assetName, const glm::vec3& position)
{
    const std::shared_ptr<Model> model = GetAssets().GetModel(assetName);
    if (!model)
        return;
    MarkChanged();
    GameObject& instance = model->Instantiate(GetScene());
    instance.GetTransform().LocalPosition = position;
    m_Selection = &instance;
}

void SceneEditor::Duplicate(GameObject& gameObject)
{
    MarkChanged();
    Transform* parent = gameObject.GetTransform().GetParent();
    GameObject& copy = GetScene().Instantiate(gameObject, GetAssets(), parent ? &parent->GetGameObject() : nullptr);
    copy.SetName(CopyName(GetScene(), gameObject));
    m_Selection = &copy;
}

void SceneEditor::Delete(GameObject& gameObject)
{
    // Gone at the end of the frame; EndFrame forgets the selection then.
    MarkChanged();
    GetScene().Destroy(gameObject);
}

void SceneEditor::Rename(GameObject& gameObject, std::string name)
{
    // A nameless object would be a blank line in the Hierarchy, hard to click.
    if (name.empty() || name == gameObject.GetName())
        return;
    MarkChanged();
    gameObject.SetName(std::move(name));
}

void SceneEditor::SetActive(GameObject& gameObject, bool active)
{
    MarkChanged();
    gameObject.SetActive(active);
}

bool SceneEditor::CanReparent(const GameObject& child, const GameObject* parent)
{
    return child.GetTransform().CanSetParent(parent ? &parent->GetTransform() : nullptr);
}

void SceneEditor::Reparent(GameObject& child, GameObject* parent)
{
    if (!CanReparent(child, parent))
        return;
    MarkChanged();
    child.GetTransform().SetParent(parent ? &parent->GetTransform() : nullptr, true);
    m_Selection = &child;
}

void SceneEditor::AddComponent(GameObject& gameObject, std::string_view type)
{
    MarkChanged();
    if (!ComponentRegistry::Add(gameObject, type))
        Log::Error("No component type is registered as {}", type);
}

void SceneEditor::RemoveComponent(Component& component)
{
    // Like Delete: it leaves its GameObject at the end of the frame.
    MarkChanged();
    GetScene().Destroy(component);
}
