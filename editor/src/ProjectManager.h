#pragma once

#include "ProjectList.h"

#include <array>
#include <optional>
#include <string>

// The editor's start screen, like Unity Hub: the list of projects, with buttons to create, add,
// open, remove and delete them. It fills the window while no project is open.
//
// It draws with Dear ImGui, which is an "immediate mode" UI: there are no widget objects to create
// and keep. Every frame, Draw() calls ImGui::Button("Open") and friends again, and a function
// returns true in the frame the button is clicked. What the UI shows comes straight from this
// class's members each frame, so there's nothing to keep in sync. (Unity's old OnGUI works the same
// way; UI Toolkit and uGUI are "retained mode", with objects.)
class ProjectManager {
public:
    // settingsFolder: where the project list is saved.
    explicit ProjectManager(const std::string& settingsFolder);

    ProjectList& GetList() { return m_List; }

    // Draws the screen. Returns the folder of a project the user chose to open (or just created),
    // or std::nullopt.
    std::optional<std::string> Draw();

    // Shows a message in a popup, for problems the caller runs into, like a project that won't
    // open.
    void ShowError(std::string message);

private:
    void DrawProjectTable(std::optional<std::string>& open);
    void DrawNewProjectPopup(std::optional<std::string>& open);
    void DrawDeletePopup();
    void DrawErrorPopup();
    void AddExisting();

    ProjectList m_List;

    // The New Project popup's text fields. ImGui edits text in place, in a buffer of chars that
    // the caller owns: a std::array, filled with a C string (text ending with a '\0').
    std::array<char, 128> m_NewName {};
    std::array<char, 512> m_NewLocation {};

    // Why the project can't be created as typed ("" if it can). Checking looks at the disk, so it
    // happens when the fields change, not every frame.
    std::string m_NewProblem;
    bool m_NewChanged = false;

    std::string m_DeleteFolder; // the project the Delete popup asks about
    std::string m_Error;        // the message the error popup shows

    // A popup opens with ImGui::OpenPopup, called where the popup is drawn (in the same ID scope).
    // Delete is asked for inside a table row, and errors come from anywhere, so these remember to
    // open them in the next Draw.
    bool m_OpenDelete = false;
    bool m_OpenError = false;
};
