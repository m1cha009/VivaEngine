#include "Builder.h"

#include "Viva/FileSystem.h"
#include "Viva/Log.h"
#include "Viva/Project.h"

using namespace Viva;

bool BuildProject(const Project& project, const std::string& outputFolder)
{
    const std::string output = NormalizePath(outputFolder);
    Log::Info("Building {} into {}", project.GetName(), output);

    // The player and the shaders sit next to the editor (the same build put them there). Every
    // step runs even if one fails, so the Console lists everything that went wrong at once; each
    // copy logs its own problem (a missing player, or a build folder inside Assets/ or Scenes/,
    // which would copy a folder into itself).
    const std::string editorFolder = GetExecutableDirectory();
    const std::string projectFile = Project::FindProjectFile(project.GetFolder());
    bool built = CreateFolder(output);
    built = CopyFileTo(editorFolder + "VivaPlayer.exe", output + "/" + project.GetName() + ".exe") && built;
    built = CopyFolderTo(editorFolder + "shaders", output + "/shaders") && built;
    built = CopyFileTo(projectFile, output + "/" + GetFileName(projectFile)) && built;
    built = CopyFolderTo(NormalizePath(project.GetAssetsFolder()), output + "/Assets") && built;
    built = CopyFolderTo(project.GetScenesFolder(), output + "/Scenes") && built;
    if (built)
        Log::Info("Build finished: {}/{}.exe", output, project.GetName());
    else
        Log::Error("Build failed: some files couldn't be copied (see above)");
    return built;
}
