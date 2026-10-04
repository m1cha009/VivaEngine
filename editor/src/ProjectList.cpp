#include "ProjectList.h"

#include "Viva/FileSystem.h"
#include "Viva/Json.h"
#include "Viva/Log.h"
#include "Viva/Project.h"

#include <algorithm>
#include <chrono>
#include <utility>

using namespace Viva;

int64_t ProjectList::Now()
{
    // system_clock is the wall clock. Since C++20 its zero is 1 January 1970, UTC, so this is the
    // usual "Unix time", in seconds.
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

ProjectList::ProjectList(std::string filePath)
    : m_FilePath(std::move(filePath))
{
}

void ProjectList::Load()
{
    m_Entries.clear();
    m_NewProjectLocation.clear();
    if (!PathExists(m_FilePath))
        return;
    std::string error;
    const std::optional<Json> json = ReadJsonFile(m_FilePath, {}, 0.0, error);
    const Json* projects = json ? json->Find("Projects") : nullptr;
    if (const Json* location = json ? json->Find("NewProjectLocation") : nullptr; location && location->AsString())
        m_NewProjectLocation = *location->AsString();
    if (!projects || !projects->AsArray()) {
        Log::Warn("The project list {} can't be read{}; starting with an empty one", m_FilePath,
                  error.empty() ? "" : ": " + error);
        return;
    }

    for (const Json& project : *projects->AsArray()) {
        const Json* name = project.Find("Name");
        const Json* folder = project.Find("Folder");
        const Json* lastOpened = project.Find("LastOpened");
        if (!name || !name->AsString() || !folder || !folder->AsString())
            continue;
        m_Entries.push_back({
            .Name = *name->AsString(),
            .Folder = *folder->AsString(),
            .LastOpened = lastOpened && lastOpened->AsNumber() ? static_cast<int64_t>(*lastOpened->AsNumber()) : 0,
        });
    }
    Sort();
    RefreshMissing();
}

void ProjectList::Add(const Project& project, bool opened)
{
    // find_if returns the first entry the lambda says yes to, or end() if there's none.
    auto entry = std::find_if(m_Entries.begin(), m_Entries.end(),
                              [&](const ProjectEntry& candidate) { return candidate.Folder == project.GetFolder(); });
    if (entry == m_Entries.end()) {
        m_Entries.push_back({ .Folder = project.GetFolder() });
        entry = m_Entries.end() - 1;
    }
    entry->Name = project.GetName();
    entry->Missing = false;
    if (opened)
        entry->LastOpened = Now();
    Sort();
    Save();
}

void ProjectList::Remove(const std::string& folder)
{
    std::erase_if(m_Entries, [&](const ProjectEntry& entry) { return entry.Folder == folder; });
    Save();
}

void ProjectList::SetNewProjectLocation(std::string folder)
{
    m_NewProjectLocation = std::move(folder);
    Save();
}

void ProjectList::RefreshMissing()
{
    for (ProjectEntry& entry : m_Entries)
        entry.Missing = Project::FindProjectFile(entry.Folder).empty();
}

void ProjectList::Sort()
{
    // Newest first, like Unity Hub. stable_sort keeps the order of projects opened at the same
    // second (and of the never-opened ones).
    std::stable_sort(m_Entries.begin(), m_Entries.end(),
                     [](const ProjectEntry& a, const ProjectEntry& b) { return a.LastOpened > b.LastOpened; });
}

void ProjectList::Save() const
{
    Json::Array projects;
    for (const ProjectEntry& entry : m_Entries) {
        Json::Object project;
        project.emplace_back("Name", entry.Name);
        project.emplace_back("Folder", entry.Folder);
        // A double holds every whole number up to 2^53 exactly: seconds until long after the sun
        // burns out.
        project.emplace_back("LastOpened", static_cast<double>(entry.LastOpened));
        projects.push_back(std::move(project));
    }
    Json::Object file;
    file.emplace_back("NewProjectLocation", m_NewProjectLocation);
    file.emplace_back("Projects", std::move(projects));
    WriteTextFile(m_FilePath, WriteJson(Json(std::move(file))));
}
