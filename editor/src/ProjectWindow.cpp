#include "ProjectWindow.h"

#include "Viva/FileSystem.h"
#include "Viva/Project.h"
#include "Viva/Scene.h"

#include <imgui.h>

#include <algorithm>

using namespace Viva;

void ProjectWindow::SetProject(const Project* project)
{
    m_Folders.clear();
    if (project)
        m_Folders = { NormalizePath(project->GetAssetsFolder()), project->GetScenesFolder() };
    Refresh();
}

void ProjectWindow::Refresh()
{
    m_Roots.clear();
    for (const std::string& folder : m_Folders)
        m_Roots.push_back(Read(folder, GetFileStem(folder)));
}

ProjectWindow::Entry ProjectWindow::Read(const std::string& path, const std::string& name)
{
    Entry entry { .Name = name, .Path = path, .IsFolder = IsFolder(path) };
    if (!entry.IsFolder)
        return entry;
    for (const std::string& childName : ListFolder(path)) {
        if (!childName.starts_with('.')) // hidden files, like .DS_Store on a Mac
            entry.Children.push_back(Read(path + "/" + childName, childName));
    }
    // Folders first, then files, each in alphabetical order, as in Explorer.
    std::sort(entry.Children.begin(), entry.Children.end(), [](const Entry& a, const Entry& b) {
        if (a.IsFolder != b.IsFolder)
            return a.IsFolder;
        return a.Name < b.Name;
    });
    return entry;
}

std::optional<std::string> ProjectWindow::Draw(bool* open)
{
    std::optional<std::string> openScene;
    if (ImGui::Begin("Project", open)) {
        if (ImGui::Button("Refresh"))
            Refresh();
        ImGui::SameLine();
        ImGui::TextDisabled("Double-click a .scene file to open it.");
        ImGui::Separator();
        for (const Entry& root : m_Roots)
            DrawEntry(root, openScene);
    }
    ImGui::End();
    return openScene;
}

void ProjectWindow::DrawEntry(const Entry& entry, std::optional<std::string>& openScene)
{
    if (!entry.IsFolder) {
        // A file is a leaf: a tree node with no arrow and nothing to pop (NoTreePushOnOpen).
        ImGui::TreeNodeEx(entry.Path.c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                                  ImGuiTreeNodeFlags_SpanAvailWidth, "%s", entry.Name.c_str());
        if (entry.Name.ends_with(std::string(".") + Scene::kFileExtension) && ImGui::IsItemHovered() &&
            ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            openScene = entry.Path;
        return;
    }
    if (ImGui::TreeNodeEx(entry.Path.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth, "%s/", entry.Name.c_str())) {
        for (const Entry& child : entry.Children)
            DrawEntry(child, openScene);
        ImGui::TreePop();
    }
}
