#pragma once

#include <optional>
#include <string>

namespace Viva {

// A project made with the engine: a folder with everything one game needs, like a Unity project.
//
//     MyGame/
//     ├── MyGame.vivaproject   the project file (JSON): name, engine version, startup scene
//     ├── Assets/              textures, models... Asset names start here ("textures/crate.png")
//     └── Scenes/
//         └── Main.scene       a scene file (M13), the one that opens first
//
// The editor (VivaEditor) creates and opens projects; the player (M18) will run them. A Project
// only describes the folder, so it's a plain value to copy around, not a resource to own.
class Project {
public:
    // Creates a new project in parentFolder/name/: the folders, the project file, and a first
    // scene with a camera, a floor and a cube. The folder mustn't exist yet. Returns std::nullopt if
    // it can't, with `error` saying why (it's also logged).
    static std::optional<Project> Create(const std::string& parentFolder, const std::string& name, std::string& error);

    // Reads the project in `folder`. Returns std::nullopt if there's no valid project file there,
    // with `error` saying why (it's also logged).
    static std::optional<Project> Open(const std::string& folder, std::string& error);

    // Why a new project can't be created as parentFolder/name, or "" if it can. The name is also
    // the folder's, and Windows doesn't allow some characters in file names (\ / : * ? " < > |), a
    // name that ends with a dot or a space, or names like CON and NUL. The folder mustn't exist
    // yet. Create checks the same, so a UI can show the problem before the user clicks Create.
    static std::string CheckNewProject(const std::string& parentFolder, const std::string& name);

    // The project file in `folder` (the first *.vivaproject), or "" if there's none.
    static std::string FindProjectFile(const std::string& folder);

    const std::string& GetName() const { return m_Name; }
    // The project's folder, without a separator at the end.
    const std::string& GetFolder() const { return m_Folder; }
    // The folder asset names start from, ending with a separator: what the project's Assets use.
    std::string GetAssetsFolder() const { return m_Folder + "/Assets/"; }
    // The scene that opens first, as a full path.
    std::string GetStartupScenePath() const { return m_Folder + "/" + m_StartupScene; }

private:
    std::string m_Name;
    std::string m_Folder;
    std::string m_StartupScene;
};

} // namespace Viva
