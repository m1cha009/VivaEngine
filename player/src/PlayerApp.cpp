#include "PlayerApp.h"

#include "Viva/FileSystem.h"
#include "Viva/Log.h"
#include "Viva/Project.h"
#include "Viva/Scene.h"

#include <optional>
#include <utility>

using namespace Viva;

PlayerApp::PlayerApp(ApplicationSettings settings, std::string projectFolder)
    : Application(std::move(settings))
    , m_ProjectFolder(std::move(projectFolder))
{
}

void PlayerApp::OnStart()
{
    // A built game has its project file right next to the executable (see the editor's Build).
    const std::string folder = NormalizePath(m_ProjectFolder.empty() ? GetExecutableDirectory() : m_ProjectFolder);
    std::string error;
    const std::optional<Project> project = Project::Open(folder, error);
    if (!project) {
        Log::Error("Nothing to play: {}", error);
        Quit(1);
        return;
    }

    // The project's assets, then its startup scene, running from the first frame: unlike the
    // editor, the player is always in "Play mode".
    SetAssetsFolder(project->GetAssetsFolder());
    if (!GetScene().Load(project->GetStartupScenePath(), GetAssets())) {
        Quit(1);
        return;
    }
    SetWindowTitle(project->GetName());
    Log::Info("Playing {}", project->GetName());
}
