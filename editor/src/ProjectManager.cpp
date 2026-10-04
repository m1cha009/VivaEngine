#include "ProjectManager.h"

#include "EditorUi.h"

#include "Viva/FileSystem.h"
#include "Viva/Project.h"

#include <imgui.h>

#include <algorithm>
#include <format>
#include <utility>

using namespace Viva;

namespace {

// "5 minutes ago", the way Unity Hub shows when a project was last opened. Relative times need no
// time zone, which keeps them simple.
std::string TimeAgo(int64_t moment)
{
    if (moment == 0)
        return "never";
    const int64_t seconds = std::max<int64_t>(0, ProjectList::Now() - moment);
    const auto ago = [](int64_t count, const char* unit) {
        return std::format("{} {}{} ago", count, unit, count == 1 ? "" : "s");
    };
    if (seconds < 60)
        return "just now";
    if (seconds < 60 * 60)
        return ago(seconds / 60, "minute");
    if (seconds < 24 * 60 * 60)
        return ago(seconds / (60 * 60), "hour");
    if (seconds < 30 * 24 * 60 * 60)
        return ago(seconds / (24 * 60 * 60), "day");
    if (seconds < 365 * 24 * 60 * 60)
        return ago(seconds / (30 * 24 * 60 * 60), "month");
    return ago(seconds / (365 * 24 * 60 * 60), "year");
}

// Where new projects go unless the user picks another place.
std::string DefaultProjectsFolder()
{
    const std::string documents = GetDocumentsFolder();
    return documents.empty() ? std::string() : NormalizePath(documents + "VivaEngine Projects");
}

// A name for a new project in `location` that isn't taken yet: "My Project", or "My Project 2" if
// that exists, and so on (Unity Hub does the same).
std::string UnusedProjectName(const std::string& location)
{
    std::string name = "My Project";
    for (int number = 2; PathExists(location + "/" + name); ++number)
        name = std::format("My Project {}", number);
    return name;
}

constexpr ImVec4 kErrorColor(1.0f, 0.45f, 0.4f, 1.0f);

} // namespace

ProjectManager::ProjectManager(const std::string& settingsFolder)
    : m_List(settingsFolder + "projects.json")
{
    m_List.Load();
}

void ProjectManager::ShowError(std::string message)
{
    m_Error = std::move(message);
    m_OpenError = true;
}

std::optional<std::string> ProjectManager::Draw()
{
    std::optional<std::string> open;

    // One window that fills the whole viewport, without a title bar: the screen itself.
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (ImGui::Begin("Project Manager", nullptr, kFlags)) {
        // The title, in a larger size of the same font (ImGui 1.92 scales fonts at any size).
        ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.8f);
        ImGui::TextUnformatted("Projects");
        ImGui::PopFont();

        // The buttons sit at the right edge: the cursor moves there first.
        const float buttonsWidth = ImGui::CalcTextSize("New Project").x + ImGui::CalcTextSize("Add Existing...").x +
                                   ImGui::GetStyle().FramePadding.x * 4.0f + ImGui::GetStyle().ItemSpacing.x;
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - buttonsWidth);
        if (ImGui::Button("New Project")) {
            const std::string& last = m_List.GetNewProjectLocation();
            const std::string location = last.empty() ? DefaultProjectsFolder() : last;
            CopyToBuffer(m_NewLocation, location);
            CopyToBuffer(m_NewName, UnusedProjectName(location));
            m_NewChanged = true;
            ImGui::OpenPopup("New Project");
        }
        ImGui::SameLine();
        if (ImGui::Button("Add Existing..."))
            AddExisting();
        ImGui::Separator();

        DrawProjectTable(open);
        DrawNewProjectPopup(open);
        DrawDeletePopup();
        DrawErrorPopup();
    }
    ImGui::End();
    return open;
}

void ProjectManager::DrawProjectTable(std::optional<std::string>& open)
{
    if (m_List.GetEntries().empty()) {
        ImGui::Spacing();
        ImGui::TextDisabled("No projects yet. Create one with New Project, or find one you have with Add Existing.");
        return;
    }

    // A removal waits until the loop is done: erasing from the list while walking it would pull
    // the entries out from under the loop.
    std::string remove;
    constexpr ImGuiTableFlags kTableFlags =
        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY | ImGuiTableFlags_PadOuterX;
    if (ImGui::BeginTable("Projects", 4, kTableFlags)) {
        ImGui::TableSetupScrollFreeze(0, 1); // the header row stays put while the rest scrolls
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Location", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn("Last opened", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();

        for (const ProjectEntry& entry : m_List.GetEntries()) {
            // Every row has buttons labelled "Open": ImGui tells widgets apart by their label, so
            // each row gets its own ID scope, named after the project's folder.
            ImGui::PushID(entry.Folder.c_str());
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            if (entry.Missing)
                ImGui::TextDisabled("%s (missing)", entry.Name.c_str());
            else
                ImGui::TextUnformatted(entry.Name.c_str());
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", entry.Folder.c_str());
            ImGui::SetItemTooltip("%s", entry.Folder.c_str()); // the whole path, when the column cuts it off
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(TimeAgo(entry.LastOpened).c_str());

            // A missing project can only be removed from the list.
            ImGui::TableNextColumn();
            ImGui::BeginDisabled(entry.Missing);
            if (ImGui::Button("Open"))
                open = entry.Folder;
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Remove"))
                remove = entry.Folder;
            ImGui::SetItemTooltip("Remove from the list. The project's files stay where they are.");
            ImGui::SameLine();
            ImGui::BeginDisabled(entry.Missing);
            if (ImGui::Button("Delete...")) {
                m_DeleteFolder = entry.Folder;
                m_OpenDelete = true;
            }
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (!remove.empty())
        m_List.Remove(remove);
}

void ProjectManager::DrawNewProjectPopup(std::optional<std::string>& open)
{
    CenterNextWindow();
    // A modal popup blocks the rest of the UI until it's closed, like a dialog box.
    if (!ImGui::BeginPopupModal("New Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere(); // start typing the name right away
    // InputText returns true in the frame its text changes.
    ImGui::SetNextItemWidth(420.0f);
    m_NewChanged |= ImGui::InputText("Name", m_NewName.data(), m_NewName.size());
    ImGui::SetNextItemWidth(420.0f);
    m_NewChanged |= ImGui::InputText("Location", m_NewLocation.data(), m_NewLocation.size());
    ImGui::SameLine();
    if (ImGui::Button("Browse...")) {
        // The dialog answers in a later frame; the lambda captures `this` to fill in the field then.
        ShowFolderDialog("Choose where to create the project", m_NewLocation.data(), [this](const std::string& folder) {
            CopyToBuffer(m_NewLocation, folder);
            m_NewChanged = true;
        });
    }

    // Checked as the user types, so the problem shows before Create is clicked.
    const std::string name = m_NewName.data();
    const std::string location = NormalizePath(m_NewLocation.data());
    if (m_NewChanged) {
        m_NewProblem = Project::CheckNewProject(location, name);
        m_NewChanged = false;
    }
    const std::string& problem = m_NewProblem;

    // Wrapped at the width of the fields, so a long path doesn't stretch the popup.
    ImGui::Spacing();
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 420.0f);
    if (problem.empty())
        ImGui::TextDisabled("The project will be created in %s/%s", location.c_str(), name.c_str());
    else
        ImGui::TextColored(kErrorColor, "%s", problem.c_str());
    ImGui::PopTextWrapPos();
    ImGui::Spacing();

    ImGui::BeginDisabled(!problem.empty());
    if (ImGui::Button("Create", ImVec2(120.0f, 0.0f))) {
        ImGui::CloseCurrentPopup();
        std::string error;
        if (const std::optional<Project> project = Project::Create(location, name, error)) {
            m_List.SetNewProjectLocation(location);
            open = project->GetFolder();
        } else {
            ShowError(error);
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void ProjectManager::DrawDeletePopup()
{
    if (m_OpenDelete) {
        ImGui::OpenPopup("Delete Project");
        m_OpenDelete = false;
    }
    CenterNextWindow();
    if (!ImGui::BeginPopupModal("Delete Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    ImGui::TextUnformatted("Delete this project's folder, and everything in it?");
    ImGui::TextColored(kErrorColor, "%s", m_DeleteFolder.c_str());
    ImGui::TextDisabled("The files are deleted for good: they don't go to the Recycle Bin.");
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.15f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.75f, 0.2f, 0.2f, 1.0f));
    const bool confirmed = ImGui::Button("Delete", ImVec2(120.0f, 0.0f));
    ImGui::PopStyleColor(2);
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
        ImGui::CloseCurrentPopup();

    if (confirmed) {
        ImGui::CloseCurrentPopup();
        // One more check before deleting for good: the folder must still be a project, so a list
        // entry that has come to point somewhere else can never delete that.
        if (Project::FindProjectFile(m_DeleteFolder).empty())
            ShowError(std::format("{} isn't a project any more, so it wasn't deleted.", m_DeleteFolder));
        else if (DeleteFolder(m_DeleteFolder))
            m_List.Remove(m_DeleteFolder);
        else
            ShowError("Some files couldn't be deleted (the log says which). The project stays in the list.");
    }
    ImGui::EndPopup();
}

void ProjectManager::DrawErrorPopup()
{
    if (m_OpenError) {
        ImGui::OpenPopup("Problem");
        m_OpenError = false;
    }
    CenterNextWindow();
    if (!ImGui::BeginPopupModal("Problem", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;
    ImGui::PushTextWrapPos(560.0f);
    ImGui::TextUnformatted(m_Error.c_str());
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
    if (ImGui::Button("OK", ImVec2(120.0f, 0.0f)))
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void ProjectManager::AddExisting()
{
    // Like Unity Hub's Add: the project joins the list, and opens when the user clicks Open.
    ShowFolderDialog("Choose a project folder", DefaultProjectsFolder(), [this](const std::string& folder) {
        std::string error;
        if (const std::optional<Project> project = Project::Open(folder, error))
            m_List.Add(*project, false);
        else
            ShowError(error);
    });
}
