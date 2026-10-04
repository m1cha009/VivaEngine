#pragma once

#include <optional>
#include <string>
#include <vector>

namespace Viva {
class Project;
}

// The Project window: the project's Assets and Scenes folders as a tree, like Unity's Project
// window. Double-clicking a scene file asks to open it. The folders are read when the project opens
// and on Refresh (and after saving), not every frame: reading them is disk work.
class ProjectWindow {
public:
    // Shows this project's folders, or nothing for null.
    void SetProject(const Viva::Project* project);
    // Reads the folders again.
    void Refresh();

    // Draws the window. Returns the path of a scene file the user double-clicked.
    std::optional<std::string> Draw(bool* open);

private:
    // A file, or a folder with what's inside it.
    struct Entry {
        std::string Name;
        std::string Path;
        bool IsFolder = false;
        std::vector<Entry> Children;
    };

    static Entry Read(const std::string& path, const std::string& name);
    static void DrawEntry(const Entry& entry, std::optional<std::string>& openScene);

    std::vector<std::string> m_Folders; // the project's Assets and Scenes folders
    std::vector<Entry> m_Roots;         // what's in them
};
