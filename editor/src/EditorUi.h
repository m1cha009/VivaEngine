#pragma once

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

// Small UI helpers the editor's windows share.

// Popups appear in the middle of the window, the first time they open.
inline void CenterNextWindow()
{
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
}

// Copies text into an ImGui text buffer, cutting it short if it doesn't fit, and ends it with the
// '\0' that marks the end of a C string. (A template on N, the buffer's size, so it fits any
// std::array of chars.)
template <size_t N>
void CopyToBuffer(std::array<char, N>& buffer, const std::string& text)
{
    const size_t count = std::min(text.size(), N - 1);
    std::copy_n(text.begin(), count, buffer.begin());
    buffer[count] = '\0';
}
