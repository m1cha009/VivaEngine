#pragma once

#include <imgui.h>

// Small UI helpers the editor's windows share.

// Popups appear in the middle of the window, the first time they open.
inline void CenterNextWindow()
{
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
}
