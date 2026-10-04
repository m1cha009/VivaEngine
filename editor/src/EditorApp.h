#pragma once

#include "ConsoleWindow.h"
#include "GameView.h"
#include "HierarchyWindow.h"
#include "ProjectManager.h"
#include "ProjectWindow.h"
#include "SceneEditor.h"
#include "SceneView.h"

#include "Viva/Application.h"
#include "Viva/Project.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>

// What the editor does at startup (see main for the options).
struct EditorOptions {
    std::string SettingsFolder; // where the project list lives; the user's settings folder if empty
    std::string OpenFolder;     // a project to open right away, or empty for the Project Manager
    // With OpenFolder: build the project into this folder, then quit (exit code 1 if the build
    // failed). Unity's batch-mode builds, for scripts and tests.
    std::string BuildFolder;
};

// VivaEditor: an Application like the Sandbox, whose content is projects. With no project open it
// shows the Project Manager (Unity Hub, M14). With one open, it's the editor (M15): a menu bar, and
// the Scene, Game, Hierarchy, Inspector, Project and Console windows, docked into one layout.
//
// The scene being edited is the Application's own scene. The editor keeps it in Edit mode (its
// components don't run) until Play (M18), and draws it into the Scene window through its own
// camera, and into the Game window through the scene's. Every change to it goes through the
// SceneEditor (M16).
class EditorApp : public Viva::Application {
public:
    EditorApp(Viva::ApplicationSettings settings, EditorOptions options);

protected:
    void OnStart() override;
    void OnUpdate(float dt) override;
    bool OnQuitRequested() override;
    void OnShutdown() override;

private:
    // Which of the editor's windows are open (the Window menu).
    struct WindowVisibility {
        bool Scene = true;
        bool Game = true;
        bool Hierarchy = true;
        bool Inspector = true;
        bool Project = true;
        bool Console = true;
    };

    // Projects. A project that can't be opened is reported in the Project Manager's Problem popup.
    void OpenProject(const std::string& folder);
    void CloseProject();

    // Scenes. Those that leave the current scene first ask about unsaved changes.
    void NewScene();
    void OpenScene(const std::string& path);
    void ShowOpenSceneDialog(); // File > Open Scene...
    void LoadScene(const std::string& path); // replaces the scene, without asking
    // After the scene was replaced: remembers its file ("" for none), and starts it clean.
    void SceneReplaced(std::string path);
    // Saves the scene, then calls `then`, if given. A scene that was never saved asks where first.
    void SaveScene(std::function<void()> then = {});
    void SaveSceneAs(std::function<void()> then = {});
    // Runs `action` now if the scene has no unsaved changes; otherwise asks Save / Don't Save /
    // Cancel first, and runs it after Save or Don't Save.
    void AskToSaveThen(std::function<void()> action);

    void DrawEditor(float dt);
    // Returns the Edit menu's command clicked, if any, which runs after the windows have drawn.
    std::optional<EditCommand> DrawMenuBar();
    // An Edit menu command: Undo, Redo, Play, Pause, or one on the selection (nothing without one).
    void RunEditCommand(EditCommand command);
    // Play mode (M18): Play starts the scene's components from a snapshot of the scene, Stop puts
    // the snapshot back (see SceneEditor::BeginPlay). Pause stops the updates without leaving.
    void Play();
    void Stop();
    // Returns the button's command (Play or Pause), run after the windows like the Edit menu's.
    std::optional<EditCommand> DrawPlayButtons();
    // File > Build...: asks to save, then for a folder, then builds into it (see Builder.h).
    void ShowBuildDialog();
    // A file double-clicked in the Project window: a scene opens, a model is placed.
    void OpenFile(const std::string& path);
    void DrawSavePrompt();
    void UpdateTitle();
    std::string GetSceneName() const;

    // First, so it's created first: it listens to the log from then on.
    ConsoleWindow m_Console;
    EditorOptions m_Options;
    std::string m_LayoutPath; // where the window layout is kept between runs
    std::unique_ptr<ProjectManager> m_ProjectManager; // created in OnStart, when the settings folder is known
    std::optional<Viva::Project> m_Project;            // the open project, if any

    std::string m_ScenePath; // the scene's file, or "" for one never saved
    // The selection, unsaved changes, and every edit (created in OnStart, when the scene exists).
    std::unique_ptr<SceneEditor> m_Editor;

    SceneView m_SceneView;
    GameView m_GameView;
    HierarchyWindow m_Hierarchy;
    ProjectWindow m_ProjectWindow;
    WindowVisibility m_Show;
    bool m_ResetLayout = false;

    std::function<void()> m_AfterSavePrompt; // what to do once the unsaved-changes prompt is answered
    bool m_OpenSavePrompt = false;
    std::string m_Title; // the window title last set
};
