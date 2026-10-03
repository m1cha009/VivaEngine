#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace Viva {

class VulkanContext;

// The renderer's half of Dear ImGui: ImGui's Vulkan backend (imgui_impl_vulkan), which turns the
// UI's vertex lists into draws. It builds its own pipeline, descriptor sets and font texture, and
// streams the UI's vertices into buffers of its own each frame. (The other half, which feeds ImGui
// the window's input, lives in Platform/Window.)
//
// ImGui keeps the backend's state globally, so there can only be one of these; creating it starts
// the backend and destroying it shuts the backend down.
class ImGuiRenderer {
public:
    // colorFormat and depthFormat: the attachments of the rendering the UI is drawn in, which
    // ImGui's pipeline must match. framesInFlight: how many frames the GPU may still be drawing,
    // so ImGui knows how long to keep each frame's vertex buffers. Returns nullptr (after logging
    // why) if the shader file is missing.
    static std::unique_ptr<ImGuiRenderer> Create(const VulkanContext& context, VkFormat colorFormat,
                                                 VkFormat depthFormat, uint32_t framesInFlight);

    ImGuiRenderer() = default; // starts nothing: use Create(), whose backend the destructor shuts down
    ~ImGuiRenderer();

    ImGuiRenderer(const ImGuiRenderer&) = delete;
    ImGuiRenderer& operator=(const ImGuiRenderer&) = delete;

    // The backend's part of starting an ImGui frame, before ImGui::NewFrame().
    void NewFrame();
    // Records the UI that the last ImGui::Render() produced into `cmd`, which must be inside the
    // frame's rendering. Returns how many draws that took. On the first frame, and whenever new
    // letters are needed, ImGui uploads its font texture first, waiting for the GPU to finish.
    uint32_t Record(VkCommandBuffer cmd);

private:
    // The SPIR-V of our version of ImGui's fragment shader (shaders/ImGui.frag). The backend
    // reads it whenever it builds its shaders, so it must live as long as the backend.
    std::vector<uint8_t> m_FragmentShaderCode;
};

} // namespace Viva
