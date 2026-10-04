#pragma once

#include "Viva/Input.h"
#include "Viva/Renderer.h"

#include <imgui.h>

#include <memory>

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

// Whether either Ctrl or Shift key is held, read through Input like the editor's other keys.
inline bool IsCtrlHeld()
{
    return Viva::Input::GetKey(Viva::Key::LeftControl) || Viva::Input::GetKey(Viva::Key::RightControl);
}
inline bool IsShiftHeld()
{
    return Viva::Input::GetKey(Viva::Key::LeftShift) || Viva::Input::GetKey(Viva::Key::RightShift);
}

// The Scene and Game views' picture: draws the scene into `target` (made on first use) from
// `view`, at `size` (points, as ImGui measures; the image gets as many pixels as they cover), and
// shows it there. The picture is drawn on an invisible button that takes the left mouse button:
// an ImGui item, so a click or drag on it has a start and an end (IsItemActivated, IsItemActive)
// and doesn't move the window. That button is the last item, for the caller to ask about.
// Returns false, drawing nothing, while the size is zero.
inline bool DrawSceneImage(Viva::Renderer& renderer, std::shared_ptr<Viva::RenderTarget>& target, ImVec2 size,
                           const Viva::RenderView& view)
{
    const ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale; // pixels per point: 2 on Retina
    const auto width = static_cast<uint32_t>(std::max(size.x * scale.x, 0.0f));
    const auto height = static_cast<uint32_t>(std::max(size.y * scale.y, 0.0f));
    if (width == 0 || height == 0)
        return false;
    if (!target)
        target = renderer.CreateRenderTarget();
    renderer.DrawScene(target, width, height, view);
    const ImVec2 min = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("SceneImage", size, ImGuiButtonFlags_MouseButtonLeft);
    ImGui::GetWindowDrawList()->AddImage(ImTextureRef(static_cast<ImTextureID>(Viva::Renderer::GetTexture(*target))),
                                         min, ImVec2(min.x + size.x, min.y + size.y));
    return true;
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
