#pragma once

#include "EditorMenus.h"

#include <glm/vec3.hpp>

#include <string>
#include <string_view>

namespace Viva {
class Application;
class Assets;
class Component;
class GameObject;
class Scene;
}

// Everything the editor changes in the scene goes through here: creating, duplicating, deleting,
// renaming, reparenting, adding and removing components. The windows (Hierarchy, Inspector, Scene
// view, menus) only ask for these edits; none of them changes the scene's structure itself. So
// there is one place that knows about every edit: today it selects what was made and marks the
// scene as changed, and M17's undo will record each edit here.
//
// It also holds the selection (Unity's Selection.activeGameObject) and whether the scene has
// unsaved changes.
class SceneEditor {
public:
    // The application's scene is the one edited, with its assets.
    explicit SceneEditor(Viva::Application& application);

    Viva::Scene& GetScene() const;
    // Asked for each time: the Application makes a new Assets for each project.
    Viva::Assets& GetAssets() const;

    // The selected GameObject, or null.
    Viva::GameObject* GetSelection() const { return m_Selection; }
    void Select(Viva::GameObject* gameObject) { m_Selection = gameObject; }

    // Whether the scene changed since it was loaded or saved (the * in the title).
    bool IsDirty() const { return m_Dirty; }
    // For changes made directly, rather than through the functions below: the Inspector's fields.
    void MarkDirty() { m_Dirty = true; }
    // The scene matches its file again: it was saved, or its changes were dropped (Don't Save).
    void MarkClean() { m_Dirty = false; }
    // The scene was replaced (new, opened, closed): nothing is selected, and nothing has changed.
    void SceneReplaced();

    // Called once a frame: forgets the selection once its object is gone. Other windows keep no
    // GameObject pointers of their own, so this is the one place that checks.
    void Update();

    // The edits. Each one selects what it made and marks the scene as changed.

    // A new GameObject below `parent`, at its parent's origin, or as a root object at `position`
    // (in front of the editor's camera, as Unity places new objects at its Scene view's pivot).
    void Create(NewObject kind, Viva::GameObject* parent, const glm::vec3& position);
    // An instance of a model (a .gltf or .glb file in Assets, by its asset name) at `position`.
    // Nothing happens (the Console says why) if it can't be loaded.
    void PlaceModel(const std::string& assetName, const glm::vec3& position);
    // A copy next to the original, named like Unity's: "Cube (1)", "Cube (2)", ...
    void Duplicate(Viva::GameObject& gameObject);
    void Delete(Viva::GameObject& gameObject);
    // An empty name isn't taken (the object keeps its old one), and the same name changes nothing.
    void Rename(Viva::GameObject& gameObject, std::string name);
    void SetActive(Viva::GameObject& gameObject, bool active);
    // Whether `child` can go below `parent` (null: become a root object). Not below itself or
    // one of its own children: the hierarchy would become a loop.
    static bool CanReparent(const Viva::GameObject& child, const Viva::GameObject* parent);
    // Moves `child` below `parent`, keeping where it is in the world, as dragging in Unity's
    // Hierarchy does.
    void Reparent(Viva::GameObject& child, Viva::GameObject* parent);
    // Adds a component of the type registered as `type` (see Viva/ComponentRegistry.h).
    void AddComponent(Viva::GameObject& gameObject, std::string_view type);
    void RemoveComponent(Viva::Component& component);

private:
    Viva::Application& m_Application;
    Viva::GameObject* m_Selection = nullptr;
    bool m_Dirty = false;
};
