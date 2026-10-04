#pragma once

#include <optional>
#include <string>
#include <vector>

namespace Viva {
class Project;
}

// The Project window: the project's Assets and Scenes folders as a tree, like Unity's Project
// window. Double-clicking a file asks to open it: a scene is opened, a model placed in the scene.
// The folders are read when the project opens and on Refresh (and after saving), not every frame:
// reading them is disk work.
class ProjectWindow {
public:
    // Shows this project's folders, or nothing for null.
    void SetProject(const Viva::Project* project);
    // Reads the folders again.
    void Refresh();

    // Draws the window. Returns the path of a file the user double-clicked.
    std::optional<std::string> Draw(bool* open);

    // The asset name of a file in the Assets folder: its path from there ("models/Box.gltf"), the
    // name Assets loads it by. std::nullopt for a file outside it.
    std::optional<std::string> GetAssetName(const std::string& path) const;
    // The asset names of the image files in the Assets folder, as last read.
    const std::vector<std::string>& GetTextureNames() const { return m_TextureNames; }

    // Whether a file is a scene, or a model Assets can load (.gltf or .glb).
    static bool IsSceneFile(const std::string& path);
    static bool IsModelFile(const std::string& path);

private:
    // A file, or a folder with what's inside it.
    struct Entry {
        std::string Name;
        std::string Path;
        bool IsFolder = false;
        std::vector<Entry> Children;
    };

    static Entry Read(const std::string& path, const std::string& name);
    void CollectAssetNames(const Entry& entry);
    static void DrawEntry(const Entry& entry, std::optional<std::string>& doubleClicked);

    std::vector<std::string> m_Folders; // the project's Assets and Scenes folders, in that order
    std::vector<Entry> m_Roots;         // what's in them
    std::vector<std::string> m_TextureNames;
};
