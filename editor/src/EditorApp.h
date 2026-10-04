#pragma once

#include "ProjectManager.h"

#include "Viva/Application.h"
#include "Viva/Project.h"

#include <memory>
#include <optional>
#include <string>

// What the editor does at startup (see main for the options).
struct EditorOptions {
    std::string SettingsFolder; // where the project list lives; the user's settings folder if empty
    std::string OpenFolder;     // a project to open right away, or empty for the Project Manager
};

// VivaEditor: an Application like the Sandbox, whose content is projects. With no project open it
// shows the Project Manager (Unity Hub). With one open, it shows the project's startup scene,
// which you can fly through (M14: viewing only; M15 adds the editor's windows).
class EditorApp : public Viva::Application {
public:
    EditorApp(Viva::ApplicationSettings settings, EditorOptions options);

protected:
    void OnStart() override;
    void OnUpdate(float dt) override;

private:
    // A project that can't be opened is reported in the Project Manager's Problem popup.
    void OpenProject(const std::string& folder);
    void CloseProject();
    // The small window shown while a project is open.
    void DrawProjectPanel();

    EditorOptions m_Options;
    std::unique_ptr<ProjectManager> m_ProjectManager; // created in OnStart, when the settings folder is known
    std::optional<Viva::Project> m_Project;            // the open project, if any
};
