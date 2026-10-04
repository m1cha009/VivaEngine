#include "EditorApp.h"

#include "Builder.h"
#include "EditorUi.h"
#include "HierarchyWindow.h"
#include "InspectorWindow.h"

#include "Viva/Camera.h"
#include "Viva/FileSystem.h"
#include "Viva/GameObject.h"
#include "Viva/Log.h"
#include "Viva/Renderer.h"
#include "Viva/Scene.h"

// imgui_internal.h has the DockBuilder functions, which build a docking layout in code. They're
// ImGui's own "internal" API: public enough to use, but more likely to change between versions.
#include <imgui_internal.h>

#include <string_view>
#include <utility>

using namespace Viva;

namespace {

constexpr const char* kTitle = "VivaEditor";

// Unity's default layout, roughly: the Hierarchy on the left, the Inspector on the right, the
// Project and Console as tabs along the bottom, and the Scene in the middle. Splitting a node gives
// back the new node on one side and, through the last argument, what's left.
void BuildDefaultLayout(ImGuiID dockspace)
{
    ImGui::DockBuilderRemoveNode(dockspace);
    ImGui::DockBuilderAddNode(dockspace, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace, ImGui::GetMainViewport()->WorkSize);
    ImGuiID center = dockspace;
    const ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.18f, nullptr, &center);
    const ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.28f, nullptr, &center);
    const ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.3f, nullptr, &center);
    ImGui::DockBuilderDockWindow("Hierarchy", left);
    ImGui::DockBuilderDockWindow("Inspector", right);
    ImGui::DockBuilderDockWindow("Project", bottom);
    ImGui::DockBuilderDockWindow("Console", bottom);
    ImGui::DockBuilderDockWindow("Game", center);
    ImGui::DockBuilderDockWindow("Scene", center); // docked last, so its tab is the one in front
    ImGui::DockBuilderFinish(dockspace);
}

} // namespace

EditorApp::EditorApp(ApplicationSettings settings, EditorOptions options)
    : Application(std::move(settings))
    , m_Options(std::move(options))
{
}

void EditorApp::OnStart()
{
    // The project list lives with the user's settings, like Unity Hub's, unless a test gives a
    // folder of its own.
    std::string settingsFolder;
    if (m_Options.SettingsFolder.empty()) {
        settingsFolder = GetUserDataFolder("Viva", "VivaEditor");
    } else {
        settingsFolder = NormalizePath(m_Options.SettingsFolder) + "/";
        CreateFolder(settingsFolder);
    }
    m_ProjectManager = std::make_unique<ProjectManager>(settingsFolder);

    // The editor's windows dock into each other, Unity-style: ImGui's docking build (M15).
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // The window layout as the user left it. ImGui keeps it as .ini text, which it can only apply
    // to windows it hasn't created yet, so it's loaded now, before the first frame. It's the
    // user's, like Unity's layouts, not the project's.
    m_LayoutPath = settingsFolder + "layout.ini";
    if (PathExists(m_LayoutPath)) {
        if (const std::optional<std::string> layout = ReadTextFile(m_LayoutPath))
            ImGui::LoadIniSettingsFromMemory(layout->c_str(), layout->size());
    }

    // Edit mode: the scene is shown, but its components don't run, as in Unity before Play (M18).
    // The window shows only the UI: the scene appears in the Scene and Game views.
    SetSceneUpdating(false);
    GetRenderer().SetSceneInWindow(false);
    m_Editor = std::make_unique<SceneEditor>(*this);

    if (!m_Options.OpenFolder.empty())
        OpenProject(NormalizePath(m_Options.OpenFolder));

    // A build from the command line: build, then quit with the result.
    if (!m_Options.BuildFolder.empty()) {
        if (!m_Project)
            Log::Error("--build needs a project to build: --open <folder> (see above if it was given)");
        Quit(m_Project && BuildProject(*m_Project, m_Options.BuildFolder) ? 0 : 1);
    }
}

void EditorApp::OnShutdown()
{
    size_t size = 0;
    const char* layout = ImGui::SaveIniSettingsToMemory(&size);
    WriteTextFile(m_LayoutPath, std::string_view(layout, size));
}

void EditorApp::OnUpdate(float dt)
{
    if (m_Project)
        DrawEditor(dt);
    else if (const std::optional<std::string> folder = m_ProjectManager->Draw())
        OpenProject(*folder);
    UpdateTitle();
}

bool EditorApp::OnQuitRequested()
{
    if (!m_Project || !m_Editor->IsDirty())
        return true;
    AskToSaveThen([this] { Quit(); });
    return false;
}

void EditorApp::OpenProject(const std::string& folder)
{
    std::string error;
    std::optional<Project> project = Project::Open(folder, error);
    if (!project) {
        m_ProjectManager->ShowError(error);
        return;
    }

    // A fresh start: a fresh Assets, rooted in the project's Assets folder, so nothing loaded for
    // one project is shared with the next.
    CloseProject();
    m_Project = std::move(project);
    SetAssetsFolder(m_Project->GetAssetsFolder());
    m_ProjectManager->GetList().Add(*m_Project, true);
    m_ProjectWindow.SetProject(&*m_Project);
    LoadScene(m_Project->GetStartupScenePath());
    Log::Info("Project opened: {} ({})", m_Project->GetName(), m_Project->GetFolder());
}

void EditorApp::CloseProject()
{
    if (!m_Project)
        return;
    Log::Info("Project closed: {}", m_Project->GetName());
    GetScene().Clear();
    // Back to the editor's own (empty) assets folder, so the project's models can go.
    SetAssetsFolder(GetExecutableDirectory() + "assets/");
    m_ProjectWindow.SetProject(nullptr);
    m_Project.reset();
    SceneReplaced({});
    // A project may have been deleted or moved while it was open.
    m_ProjectManager->GetList().RefreshMissing();
}

void EditorApp::NewScene()
{
    AskToSaveThen([this] {
        // Like Unity's new scene: nothing but a camera, not saved anywhere yet.
        Scene& scene = GetScene();
        scene.Clear();
        GameObject& camera = scene.CreateGameObject("Main Camera");
        camera.GetTransform().LocalPosition = { 0.0f, 1.0f, -10.0f };
        camera.AddComponent<Camera>();
        SceneReplaced({});
    });
}

void EditorApp::OpenScene(const std::string& path)
{
    AskToSaveThen([this, path] { LoadScene(path); });
}

void EditorApp::ShowOpenSceneDialog()
{
    ShowOpenFileDialog("Open Scene", m_Project->GetScenesFolder(), "Scene files", Scene::kFileExtension,
                       [this](const std::string& path) {
                           if (m_Project) // it may have been closed while the dialog was open
                               OpenScene(path);
                       });
}

void EditorApp::LoadScene(const std::string& path)
{
    // Clear, then Load: Unity's LoadScene in its default, single mode. If the file doesn't load
    // (the Console says why), the scene stays empty and unnamed.
    Scene& scene = GetScene();
    scene.Clear();
    SceneReplaced(scene.Load(path, GetAssets()) ? path : std::string());
}

void EditorApp::SceneReplaced(std::string path)
{
    m_ScenePath = std::move(path);
    m_Editor->SceneReplaced();
    m_SceneView.StartAtMainCamera(GetScene());
}

void EditorApp::SaveScene(std::function<void()> then)
{
    if (!m_Project)
        return;
    if (m_ScenePath.empty()) {
        SaveSceneAs(std::move(then));
        return;
    }
    if (!GetScene().Save(m_ScenePath))
        return; // the Console says why; whatever was waiting on the save doesn't happen
    m_Editor->MarkClean();
    m_ProjectWindow.Refresh();
    if (then)
        then();
}

void EditorApp::SaveSceneAs(std::function<void()> then)
{
    if (!m_Project)
        return;
    const std::string extension = std::string(".") + Scene::kFileExtension;
    const std::string start =
        m_ScenePath.empty() ? m_Project->GetScenesFolder() + "/" + GetSceneName() + extension : m_ScenePath;
    // The dialog answers in a later frame, so what to do after saving travels with the callback.
    ShowSaveFileDialog("Save Scene As", start, "Scene files", Scene::kFileExtension,
                       [this, extension, then = std::move(then)](std::string path) mutable {
                           if (!path.ends_with(extension))
                               path += extension;
                           m_ScenePath = std::move(path);
                           SaveScene(std::move(then));
                       });
}

void EditorApp::AskToSaveThen(std::function<void()> action)
{
    // Everything that leaves the scene (New, Open, Close Project, Build, Quit) comes through here:
    // Play mode ends first, throwing away what changed while playing, as Stop does.
    if (m_Editor->IsPlaying())
        Stop();
    if (!m_Editor->IsDirty()) {
        action();
        return;
    }
    m_AfterSavePrompt = std::move(action);
    m_OpenSavePrompt = true;
}

void EditorApp::DrawEditor(float dt)
{
    std::optional<EditCommand> command = DrawMenuBar();
    if (!m_Project)
        return; // the menu closed it

    // The dock space fills the window below the menu bar; the windows dock into it. Its layout is
    // built in code the first time, and on Window > Reset Layout.
    const ImGuiID dockspace = ImGui::GetID("Editor");
    if (m_ResetLayout || !ImGui::DockBuilderGetNode(dockspace)) {
        BuildDefaultLayout(dockspace);
        m_ResetLayout = false;
    }
    ImGui::DockSpaceOverViewport(dockspace, ImGui::GetMainViewport());

    m_SceneView.Update(dt);
    if (m_Show.Scene)
        m_SceneView.Draw(GetRenderer(), *m_Editor, &m_Show.Scene);
    if (m_Show.Game)
        m_GameView.Draw(GetRenderer(), GetScene(), &m_Show.Game);
    if (m_Show.Hierarchy) {
        if (const std::optional<EditCommand> clicked = m_Hierarchy.Draw(*m_Editor, m_SceneView.GetPlacement(), &m_Show.Hierarchy))
            command = clicked;
    }
    if (m_Show.Inspector)
        DrawInspectorWindow(*m_Editor, m_ProjectWindow.GetTextureNames(), &m_Show.Inspector);
    if (m_Show.Project) {
        if (const std::optional<std::string> path = m_ProjectWindow.Draw(&m_Show.Project))
            OpenFile(*path);
    }
    if (m_Show.Console)
        m_Console.Draw(&m_Show.Console);

    // The keys, as in Unity: the Edit commands (see ReadEditShortcut), and W, E, R and X for the
    // gizmo, which act on the selection only while the Hierarchy or the Scene view has the focus.
    // Not while a widget is held: in the middle of a gizmo drag, Ctrl+Z or Del would pull the
    // object out from under it. Commands run after the windows have drawn, so none of them
    // changes the scene in the middle of a window.
    const bool selectionKeys = m_Hierarchy.IsFocused() || m_SceneView.IsFocused();
    if (!ImGui::IsAnyItemActive()) {
        if (!command)
            command = ReadEditShortcut(selectionKeys);
        if (selectionKeys)
            m_SceneView.ReadToolKeys();
    }
    if (command)
        RunEditCommand(*command);

    DrawSavePrompt();

    // The frame's edits are done: once nothing is held any more, they become one undo step.
    m_Editor->EndFrame(ImGui::IsAnyItemActive());
}

std::optional<EditCommand> EditorApp::DrawMenuBar()
{
    std::optional<EditCommand> command; // the Edit menu's item clicked, run after the windows
    // Saving would save what Play mode changed, which Unity doesn't allow either: Save waits for
    // Stop. (New, Open, Close Project and Build stop Play mode first: see AskToSaveThen.)
    const bool editing = !m_Editor->IsPlaying();

    // Keyboard shortcuts work anywhere in the editor (RouteGlobal), menu open or not.
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_N, ImGuiInputFlags_RouteGlobal))
        NewScene();
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_O, ImGuiInputFlags_RouteGlobal))
        ShowOpenSceneDialog();
    if (editing && ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteGlobal))
        SaveScene();
    if (editing && ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S, ImGuiInputFlags_RouteGlobal))
        SaveSceneAs();

    // In Play mode the menu bar turns a darker blue, so it's hard to miss that changes won't last
    // (Unity tints the whole editor).
    if (!editing)
        ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0.1f, 0.25f, 0.5f, 1.0f));
    const bool menuBar = ImGui::BeginMainMenuBar();
    if (!editing)
        ImGui::PopStyleColor();
    if (!menuBar)
        return command;
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Scene", "Ctrl+N"))
            NewScene();
        if (ImGui::MenuItem("Open Scene...", "Ctrl+O"))
            ShowOpenSceneDialog();
        ImGui::Separator();
        // The last argument of MenuItem greys the item out while it's false.
        if (ImGui::MenuItem("Save", "Ctrl+S", false, editing))
            SaveScene();
        if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S", false, editing))
            SaveSceneAs();
        ImGui::Separator();
        if (ImGui::MenuItem("Build..."))
            ShowBuildDialog();
        ImGui::Separator();
        if (ImGui::MenuItem("Close Project"))
            AskToSaveThen([this] { CloseProject(); });
        ImGui::EndMenu();
    }
    // Edit and GameObject, as in Unity. The Edit menu's items are shared with the Hierarchy's
    // right-click menu (see EditorMenus.h).
    if (ImGui::BeginMenu("Edit")) {
        const EditMenuState state {
            .HasSelection = m_Editor->GetSelection() != nullptr,
            .CanUndo = m_Editor->CanUndo(),
            .CanRedo = m_Editor->CanRedo(),
            .Playing = m_Editor->IsPlaying(),
            .Paused = m_Editor->IsPaused(),
        };
        command = DrawEditMenuItems(state, false);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("GameObject")) {
        // New objects go in front of the Scene view's camera, as root objects.
        if (const std::optional<NewObject> kind = DrawCreateMenuItems())
            m_Editor->Create(*kind, nullptr, m_SceneView.GetPlacement());
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Window")) {
        // A menu item with a bool* shows a check mark and flips the bool when clicked.
        ImGui::MenuItem("Scene", nullptr, &m_Show.Scene);
        ImGui::MenuItem("Game", nullptr, &m_Show.Game);
        ImGui::MenuItem("Hierarchy", nullptr, &m_Show.Hierarchy);
        ImGui::MenuItem("Inspector", nullptr, &m_Show.Inspector);
        ImGui::MenuItem("Project", nullptr, &m_Show.Project);
        ImGui::MenuItem("Console", nullptr, &m_Show.Console);
        ImGui::Separator();
        if (ImGui::MenuItem("Reset Layout")) {
            m_Show = {};
            m_ResetLayout = true;
        }
        ImGui::EndMenu();
    }
    if (const std::optional<EditCommand> clicked = DrawPlayButtons())
        command = clicked;
    ImGui::EndMainMenuBar();
    return command;
}

std::optional<EditCommand> EditorApp::DrawPlayButtons()
{
    // Play and Pause in the middle of the menu bar, where Unity has them. While playing, Play
    // becomes Stop, and the active one is drawn pressed.
    const bool playing = m_Editor->IsPlaying();
    const float width = ImGui::CalcTextSize("Stop").x + ImGui::CalcTextSize("Pause").x +
                        ImGui::GetStyle().FramePadding.x * 4.0f + ImGui::GetStyle().ItemSpacing.x;
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), (ImGui::GetWindowWidth() - width) * 0.5f));
    const auto button = [](const char* label, bool pressed) {
        if (pressed)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        const bool clicked = ImGui::Button(label);
        if (pressed)
            ImGui::PopStyleColor();
        return clicked;
    };
    std::optional<EditCommand> clicked;
    if (button(playing ? "Stop" : "Play", playing))
        clicked = EditCommand::Play;
    ImGui::BeginDisabled(!playing); // greyed out, and not clickable, until Play
    if (button("Pause", m_Editor->IsPaused()))
        clicked = EditCommand::Pause;
    ImGui::EndDisabled();
    return clicked;
}

void EditorApp::Play()
{
    m_Editor->BeginPlay();
    ImGui::SetWindowFocus("Game"); // as Unity does, show the game
    Log::Info("Play mode: on (changes made now are undone by Stop)");
}

void EditorApp::Stop()
{
    m_Editor->EndPlay();
    Log::Info("Play mode: off");
}

void EditorApp::ShowBuildDialog()
{
    // The build copies the scene files as they are on disk, so unsaved changes are saved (or not)
    // first. The suggested folder is next to the project's.
    AskToSaveThen([this] {
        ShowFolderDialog("Build " + m_Project->GetName() + " into...", GetParentFolder(m_Project->GetFolder()),
                         [this](const std::string& output) {
                             if (m_Project) // it may have been closed while the dialog was open
                                 BuildProject(*m_Project, output);
                         });
    });
}

void EditorApp::DrawSavePrompt()
{
    if (m_OpenSavePrompt) {
        ImGui::OpenPopup("Unsaved Changes");
        m_OpenSavePrompt = false;
    }
    CenterNextWindow();
    if (!ImGui::BeginPopupModal("Unsaved Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;
    ImGui::Text("%s has unsaved changes. Save them first?", GetSceneName().c_str());
    ImGui::Spacing();
    // std::exchange takes the waiting action out, leaving nothing behind.
    if (ImGui::Button("Save", ImVec2(120.0f, 0.0f))) {
        ImGui::CloseCurrentPopup();
        SaveScene(std::exchange(m_AfterSavePrompt, {}));
    }
    ImGui::SameLine();
    if (ImGui::Button("Don't Save", ImVec2(120.0f, 0.0f))) {
        ImGui::CloseCurrentPopup();
        m_Editor->MarkClean();
        if (std::function<void()> action = std::exchange(m_AfterSavePrompt, {}))
            action();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f))) {
        ImGui::CloseCurrentPopup();
        m_AfterSavePrompt = {};
    }
    ImGui::EndPopup();
}

void EditorApp::UpdateTitle()
{
    // "VivaEditor - MyGame - Main*", where * means unsaved changes, as in Unity's title bar.
    std::string title = kTitle;
    if (m_Project)
        title += " - " + m_Project->GetName() + " - " + GetSceneName() + (m_Editor->IsDirty() ? "*" : "") +
                 (!m_Editor->IsPlaying() ? "" : m_Editor->IsPaused() ? " [Paused]" : " [Playing]");
    if (title != m_Title) {
        SetWindowTitle(title);
        m_Title = std::move(title);
    }
}

void EditorApp::RunEditCommand(EditCommand command)
{
    switch (command) {
    case EditCommand::Undo:
        m_Editor->Undo();
        return;
    case EditCommand::Redo:
        m_Editor->Redo();
        return;
    case EditCommand::Play:
        if (m_Editor->IsPlaying())
            Stop();
        else
            Play();
        return;
    case EditCommand::Pause:
        if (m_Editor->IsPlaying())
            m_Editor->SetPaused(!m_Editor->IsPaused());
        return;
    default:
        break;
    }
    GameObject* selection = m_Editor->GetSelection();
    if (!selection || !ActsOnSelection(command))
        return;
    switch (command) {
    case EditCommand::Duplicate:
        m_Editor->Duplicate(*selection);
        break;
    case EditCommand::Rename:
        // The name is edited in the Hierarchy, which must be showing for that.
        m_Show.Hierarchy = true;
        m_Hierarchy.StartRename(*selection);
        break;
    case EditCommand::Delete:
        m_Editor->Delete(*selection);
        break;
    case EditCommand::FrameSelected:
        m_SceneView.Frame(*selection);
        break;
    default:
        break;
    }
}

void EditorApp::OpenFile(const std::string& path)
{
    if (ProjectWindow::IsSceneFile(path)) {
        OpenScene(path);
        return;
    }
    if (ProjectWindow::IsModelFile(path)) {
        if (const std::optional<std::string> assetName = m_ProjectWindow.GetAssetName(path))
            m_Editor->PlaceModel(*assetName, m_SceneView.GetPlacement());
    }
}

std::string EditorApp::GetSceneName() const
{
    return m_ScenePath.empty() ? "Untitled" : GetFileStem(m_ScenePath);
}
