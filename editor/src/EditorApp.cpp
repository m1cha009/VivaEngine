#include "EditorApp.h"

#include "Viva/Assets.h"
#include "Viva/Camera.h"
#include "Viva/FileSystem.h"
#include "Viva/FlyCamera.h"
#include "Viva/GameObject.h"
#include "Viva/Log.h"
#include "Viva/Scene.h"

#include <imgui.h>

#include <utility>

using namespace Viva;

namespace {

constexpr const char* kTitle = "VivaEditor";

} // namespace

EditorApp::EditorApp(ApplicationSettings settings, EditorOptions options)
    : Application(std::move(settings))
    , m_Options(std::move(options))
{
}

void EditorApp::OnStart()
{
    // The project list lives with the user's settings, like Unity Hub's, unless a test gives a
    // folder of its own.
    std::string settingsFolder;
    if (m_Options.SettingsFolder.empty()) {
        settingsFolder = GetUserDataFolder("Viva", "VivaEditor");
    } else {
        settingsFolder = NormalizePath(m_Options.SettingsFolder) + "/";
        CreateFolder(settingsFolder);
    }
    m_ProjectManager = std::make_unique<ProjectManager>(settingsFolder);

    if (!m_Options.OpenFolder.empty())
        OpenProject(NormalizePath(m_Options.OpenFolder));
}

void EditorApp::OnUpdate(float /*dt*/)
{
    if (m_Project) {
        DrawProjectPanel();
    } else if (const std::optional<std::string> folder = m_ProjectManager->Draw()) {
        OpenProject(*folder);
    }
}

void EditorApp::OpenProject(const std::string& folder)
{
    std::string error;
    std::optional<Project> project = Project::Open(folder, error);
    if (!project) {
        m_ProjectManager->ShowError(error);
        return;
    }

    // A fresh start: the old scene goes (at the end of this frame), and with it, once nothing uses
    // them, the old project's assets. A fresh Assets, rooted in the project's Assets folder, means
    // nothing loaded for one project is shared with the next.
    CloseProject();
    m_Project = std::move(project);
    SetAssetsFolder(m_Project->GetAssetsFolder());
    m_ProjectManager->GetList().Add(*m_Project, true);

    // The scene opens even if it fails to load: the project is still the project, and the log
    // (and, from M15, the Console) says what went wrong.
    Scene& scene = GetScene();
    if (!scene.Load(m_Project->GetStartupScenePath(), GetAssets()))
        Log::Warn("{}'s startup scene didn't load: the scene is empty", m_Project->GetName());

    // For now the scene is only looked at: the main camera gets a FlyCamera, if it hasn't got one,
    // so you can look around. (M15 gives the editor a camera of its own instead.)
    if (Camera* camera = scene.GetMainCamera()) {
        if (!camera->GetGameObject().GetComponent<FlyCamera>())
            camera->GetGameObject().AddComponent<FlyCamera>();
    }

    SetWindowTitle(std::string(kTitle) + " - " + m_Project->GetName());
    Log::Info("Project opened: {} ({})", m_Project->GetName(), m_Project->GetFolder());
}

void EditorApp::CloseProject()
{
    if (!m_Project)
        return;
    Log::Info("Project closed: {}", m_Project->GetName());
    GetScene().Clear();
    // Back to the editor's own (empty) assets folder, so the project's models can go.
    SetAssetsFolder(GetExecutableDirectory() + "assets/");
    m_Project.reset();
    SetWindowTitle(kTitle);
    // A project may have been deleted or moved while it was open.
    m_ProjectManager->GetList().RefreshMissing();
}

void EditorApp::DrawProjectPanel()
{
    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
    ImGui::TextUnformatted(m_Project->GetName().c_str());
    ImGui::TextDisabled("%s", m_Project->GetFolder().c_str());
    ImGui::TextDisabled("%zu GameObjects", GetScene().GetGameObjects().size());
    ImGui::Separator();
    ImGui::TextDisabled("Fly: W/A/S/D, Q/E, Shift; look: right mouse button");
    // Closing is done after the panel is finished: the panel shows the project's name.
    const bool close = ImGui::Button("Close Project");
    ImGui::End();
    if (close)
        CloseProject();
}
