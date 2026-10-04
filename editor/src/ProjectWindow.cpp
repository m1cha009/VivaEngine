#include "ProjectWindow.h"

#include "Viva/FileSystem.h"
#include "Viva/Project.h"
#include "Viva/Scene.h"

#include <imgui.h>

#include <algorithm>

using namespace Viva;

namespace {

// The image formats Assets can load (stb_image, M7).
bool IsImageFile(const std::string& path)
{
    const std::string extension = GetFileExtension(path);
    return extension == "png" || extension == "jpg" || extension == "jpeg";
}

} // namespace

bool ProjectWindow::IsSceneFile(const std::string& path)
{
    return GetFileExtension(path) == Scene::kFileExtension;
}

bool ProjectWindow::IsModelFile(const std::string& path)
{
    const std::string extension = GetFileExtension(path);
    return extension == "gltf" || extension == "glb";
}

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
    m_TextureNames.clear();
    for (const std::string& folder : m_Folders)
        m_Roots.push_back(Read(folder, GetFileStem(folder)));
    if (!m_Roots.empty())
        CollectAssetNames(m_Roots.front()); // the Assets folder's tree
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

void ProjectWindow::CollectAssetNames(const Entry& entry)
{
    if (!entry.IsFolder) {
        if (IsImageFile(entry.Path))
            m_TextureNames.push_back(*GetAssetName(entry.Path));
        return;
    }
    for (const Entry& child : entry.Children)
        CollectAssetNames(child);
}

std::optional<std::string> ProjectWindow::GetAssetName(const std::string& path) const
{
    // Below the Assets folder: the path from there on, past the separator.
    if (m_Folders.empty())
        return std::nullopt;
    const std::string prefix = m_Folders.front() + "/";
    if (!path.starts_with(prefix))
        return std::nullopt;
    return path.substr(prefix.size());
}

std::optional<std::string> ProjectWindow::Draw(bool* open)
{
    std::optional<std::string> doubleClicked;
    if (ImGui::Begin("Project", open)) {
        if (ImGui::Button("Refresh"))
            Refresh();
        ImGui::SameLine();
        ImGui::TextDisabled("Double-click a scene to open it, a .gltf or .glb model to place it.");
        ImGui::Separator();
        for (const Entry& root : m_Roots)
            DrawEntry(root, doubleClicked);
    }
    ImGui::End();
    return doubleClicked;
}

void ProjectWindow::DrawEntry(const Entry& entry, std::optional<std::string>& doubleClicked)
{
    if (!entry.IsFolder) {
        // A file is a leaf: a tree node with no arrow and nothing to pop (NoTreePushOnOpen).
        ImGui::TreeNodeEx(entry.Path.c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                                  ImGuiTreeNodeFlags_SpanAvailWidth, "%s", entry.Name.c_str());
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            doubleClicked = entry.Path;
        return;
    }
    if (ImGui::TreeNodeEx(entry.Path.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth, "%s/", entry.Name.c_str())) {
        for (const Entry& child : entry.Children)
            DrawEntry(child, doubleClicked);
        ImGui::TreePop();
    }
}
