#pragma once

#include "EditorMenus.h"

#include <glm/vec3.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Viva {
class Application;
class Assets;
class Component;
class GameObject;
class Scene;
}

// Everything the editor changes in the scene goes through here: creating, duplicating, deleting,
// renaming, reparenting, adding and removing components. The windows (Hierarchy, Inspector, Scene
// view, menus) only ask for these edits, or say when they changed a field themselves
// (MarkChanged). So this is the one place that knows about every edit, which is what undo needs.
//
// It also holds the selection (Unity's Selection.activeGameObject), whether the scene has unsaved
// changes, and the undo history (M17).
//
// Undo works with snapshots: after each finished edit, the whole scene is written down as the
// text a scene file holds (Scene::Serialize, in memory, with the objects' Ids). Undo replaces the
// scene with the snapshot before; Redo with the one after. Like Unity's Undo, an edit is finished
// when the user lets go: one drag of a value or a gizmo, or one name typed, is one step.
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

    // Whether the scene differs from its file (the * in the title). Undoing back to the saved
    // state makes it clean again.
    bool IsDirty() const { return m_Changing || m_Current.Step != m_SavedStep; }
    // The scene matches its file again: it was saved, or its changes were dropped (Don't Save).
    void MarkClean();
    // The scene was replaced (new, opened, closed): nothing is selected, nothing has changed, and
    // there's nothing to undo.
    void SceneReplaced();

    // Says the scene is being changed. The edits below call it themselves; the Inspector and the
    // gizmo call it when they change a field directly. Call it before the change if the change
    // also changes the selection: the step remembers what was selected before it.
    void MarkChanged();
    // Called once a frame, after every window: forgets the selection once its object is gone, and
    // ends the edit in progress (taking its snapshot) once `userIsEditing` is false, that is, when
    // no widget is held (ImGui::IsAnyItemActive).
    void EndFrame(bool userIsEditing);

    // Undo and Redo (Ctrl+Z, Ctrl+Y). Each replaces the scene with a snapshot, and selects what
    // was selected then. Each first finishes an edit still in progress. Not in Play mode.
    bool CanUndo() const { return !m_Playing && (m_Changing || !m_Undo.empty()); }
    bool CanRedo() const { return !m_Playing && !m_Redo.empty(); }

    // Play mode (M18), like Unity's: BeginPlay keeps the scene as it is (the current snapshot,
    // after finishing any edit) and starts its components (Application::SetSceneUpdating); EndPlay
    // stops them and puts the snapshot back, so whatever the game and the user changed while
    // playing is gone. In between, changes aren't undo steps and don't count as unsaved. The
    // selection stays on the same object if it existed before Play. Paused, the components stop
    // updating but Play mode goes on.
    void BeginPlay();
    void EndPlay();
    void SetPaused(bool paused);
    bool IsPlaying() const { return m_Playing; }
    bool IsPaused() const { return m_Paused; }
    void Undo();
    void Redo();

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
    // The scene at one point in the history: its JSON as text (a tree of Json values would take
    // many times the memory), and the Id of the object selected then (0 for none). Restoring it
    // creates new GameObjects, so a pointer couldn't say which was selected; the Id can.
    struct Snapshot {
        std::string Scene;
        uint64_t Selection = 0;
        uint64_t Step = 0; // which state of the scene this is, for IsDirty
    };

    Snapshot TakeSnapshot();
    void Restore(const Snapshot& snapshot);
    void FinishChange(); // ends the edit in progress, if any, with a new snapshot
    uint64_t SelectionId() const;

    Viva::Application& m_Application;
    Viva::GameObject* m_Selection = nullptr;

    Snapshot m_Current;             // the scene as of the last finished edit (or load)
    std::vector<Snapshot> m_Undo;   // the states before it, oldest first
    std::vector<Snapshot> m_Redo;   // the states undone, most recently undone last
    bool m_Changing = false;        // the scene changed since m_Current: an edit is in progress
    bool m_Playing = false;         // in Play mode: m_Current is the scene to go back to
    bool m_Paused = false;          // in Play mode, with the components' updates stopped
    uint64_t m_NextStep = 0;
    uint64_t m_SavedStep = 0;       // the step that matches the file
};
