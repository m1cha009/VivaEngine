#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Viva {
class Project;
}

// One project the editor knows about.
struct ProjectEntry {
    std::string Name;
    std::string Folder;
    int64_t LastOpened = 0; // seconds since 1 January 1970 (UTC), the usual way to store a moment
    bool Missing = false;   // its folder or project file wasn't there when last checked
};

// The projects the editor knows about, newest first, kept in a JSON file in the user's settings
// folder: what Unity Hub's project list remembers, along with where new projects go. Projects
// themselves live wherever the user put them; this list only points at them.
class ProjectList {
public:
    // filePath: where the list is saved, for example %APPDATA%\Viva\VivaEditor\projects.json.
    explicit ProjectList(std::string filePath);

    // Reads the file. A missing file is an empty list (the first run).
    void Load();

    const std::vector<ProjectEntry>& GetEntries() const { return m_Entries; }

    // Adds the project, or updates its entry: its name, and when it was last opened (now, if
    // `opened`; otherwise a new entry counts as never opened). Then saves.
    void Add(const Viva::Project& project, bool opened);
    // Forgets a project (its files stay where they are). Then saves.
    void Remove(const std::string& folder);
    // Checks which projects' folders are still there.
    void RefreshMissing();

    // Where the last new project was created, which the next one suggests (empty at first). Set
    // saves.
    const std::string& GetNewProjectLocation() const { return m_NewProjectLocation; }
    void SetNewProjectLocation(std::string folder);

    // The time now, as stored in LastOpened.
    static int64_t Now();

private:
    void Sort();
    void Save() const;

    std::string m_FilePath;
    std::vector<ProjectEntry> m_Entries;
    std::string m_NewProjectLocation;
};
