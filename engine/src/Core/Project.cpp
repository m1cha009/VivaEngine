#include "Viva/Project.h"

#include "Viva/FileSystem.h"
#include "Viva/Json.h"
#include "Viva/Log.h"
#include "Viva/Version.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <format>
#include <string_view>

namespace Viva {

namespace {

// The first members of every project file, as in scene files (see SceneFile.cpp).
constexpr const char* kFormatName = "VivaEngine Project";
constexpr double kFormatVersion = 1.0;
constexpr const char* kExtension = ".vivaproject";
constexpr const char* kStartupScene = "Scenes/Main.scene";

// The scene a new project starts with: a camera looking at a floor and a turning cube, so there's
// something to see. It's a scene file (M13), written here as a "raw string literal": R"( starts
// it and )" ends it, and everything in between, quotes and line breaks included, is the text as
// it stands, with nothing to escape (like a C# verbatim string, @"...").
// The camera is 8 m back and 3.5 m up, tilted 18 degrees down: the rotation is a quaternion for
// 18 degrees around X, [sin 9°, 0, 0, cos 9°]. There are no textures, because a new project's
// Assets folder is empty: the colors come from the materials.
constexpr std::string_view kTemplateScene = R"({
  "Format": "VivaEngine Scene",
  "Version": 1,
  "GameObjects": [
    {
      "Name": "Main Camera",
      "Transform": { "Position": [0, 3.5, -8], "Rotation": [0.15643448, 0, 0, 0.98768836] },
      "Components": [ { "Type": "Camera", "BackgroundColor": [0.19, 0.3, 0.47] } ]
    },
    {
      "Name": "Floor",
      "Components": [
        { "Type": "MeshRenderer", "Parts": [ { "Mesh": "Primitives::Plane", "Material": { "Color": [0.45, 0.47, 0.5, 1] } } ] }
      ]
    },
    {
      "Name": "Cube",
      "Transform": { "Position": [0, 1, 0] },
      "Components": [
        { "Type": "MeshRenderer", "Parts": [ { "Mesh": "Primitives::Cube", "Material": { "Color": [0.95, 0.6, 0.2, 1] } } ] },
        { "Type": "Spinner", "DegreesPerSecond": 45 }
      ]
    }
  ]
}
)";

// Logs a message and keeps it for the caller, then returns "nothing": the failure path of Create
// and Open.
std::nullopt_t Fail(std::string& error, std::string message)
{
    Log::Error("{}", message);
    error = std::move(message);
    return std::nullopt;
}

// Why `name` can't be a file or folder name on Windows, or "".
std::string CheckName(const std::string& name)
{
    if (name.empty())
        return "The name is empty.";
    for (const char c : name) {
        if (static_cast<unsigned char>(c) < 0x20 || std::string_view(R"(\/:*?"<>|)").find(c) != std::string_view::npos)
            return std::format("A name can't contain {}.", c < 0x20 ? std::string("control characters") : std::string(1, c));
    }
    if (name.back() == '.' || name.back() == ' ')
        return "A name can't end with a dot or a space.";
    if (name.front() == ' ')
        return "A name can't start with a space.";

    // Names Windows keeps for devices, with or without an extension: "CON", "nul.txt", "COM1".
    std::string upper = name.substr(0, name.find('.'));
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    constexpr std::array kReserved = { "CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6",
                                       "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7",
                                       "LPT8", "LPT9" };
    if (std::find(kReserved.begin(), kReserved.end(), upper) != kReserved.end())
        return std::format("{} is a name Windows keeps for itself.", name);
    return {};
}

} // namespace

std::string Project::CheckNewProject(const std::string& parentFolder, const std::string& name)
{
    if (std::string problem = CheckName(name); !problem.empty())
        return problem;
    const std::string location = NormalizePath(parentFolder);
    if (location.empty())
        return "Choose a location.";
    if (PathExists(location + "/" + name))
        return std::format("{}/{} exists already.", location, name);
    return {};
}

std::string Project::FindProjectFile(const std::string& folder)
{
    for (const std::string& name : ListFolder(folder)) {
        if (name.size() > std::string_view(kExtension).size() && name.ends_with(kExtension))
            return NormalizePath(folder) + "/" + name;
    }
    return {};
}

std::optional<Project> Project::Create(const std::string& parentFolder, const std::string& name, std::string& error)
{
    if (const std::string problem = CheckNewProject(parentFolder, name); !problem.empty())
        return Fail(error, problem);
    const std::string folder = NormalizePath(parentFolder) + "/" + name;

    // The project file, written through the same JSON writer as scene files.
    Json::Object file;
    file.emplace_back("Format", kFormatName);
    file.emplace_back("Version", kFormatVersion);
    file.emplace_back("Name", name);
    file.emplace_back("EngineVersion", GetEngineVersion().ToString());
    file.emplace_back("StartupScene", kStartupScene);

    // If any step fails, the half-made folder goes again (it didn't exist before).
    const bool created = CreateFolder(folder + "/Assets") && CreateFolder(folder + "/Scenes") &&
                         WriteTextFile(folder + "/" + kStartupScene, kTemplateScene) &&
                         WriteTextFile(folder + "/" + name + kExtension, WriteJson(Json(std::move(file))));
    if (!created) {
        if (PathExists(folder))
            DeleteFolder(folder);
        return Fail(error, std::format("Couldn't create the project in {} (the log says why).", folder));
    }
    Log::Info("Project created: {}", folder);
    return Open(folder, error);
}

std::optional<Project> Project::Open(const std::string& folder, std::string& error)
{
    if (!IsFolder(folder))
        return Fail(error, std::format("The folder {} doesn't exist.", folder));
    const std::string path = FindProjectFile(folder);
    if (path.empty())
        return Fail(error, std::format("{} isn't a project: it has no {} file.", folder, kExtension));

    std::optional<Json> json = ReadJsonFile(path, kFormatName, kFormatVersion, error);
    if (!json)
        return Fail(error, error);
    const Json* name = json->Find("Name");
    if (!name || !name->AsString())
        return Fail(error, std::format("{} has no project name.", path));

    Project project;
    project.m_Name = *name->AsString();
    project.m_Folder = NormalizePath(folder);
    const Json* startupScene = json->Find("StartupScene");
    project.m_StartupScene = startupScene && startupScene->AsString() ? *startupScene->AsString() : kStartupScene;
    // Like Unity opening a project made with another version, it opens anyway, with a note.
    const Json* engineVersion = json->Find("EngineVersion");
    const std::string madeWith = engineVersion && engineVersion->AsString() ? *engineVersion->AsString() : "unknown";
    if (madeWith != GetEngineVersion().ToString())
        Log::Warn("{} was made with engine version {}, and this is {}", project.m_Name, madeWith,
                  GetEngineVersion().ToString());
    return project;
}

} // namespace Viva
