#include "Renderer/ImGuiRenderer.h"

#include "Renderer/Pipeline.h"
#include "Renderer/VulkanCheck.h"
#include "Renderer/VulkanContext.h"

#include <imgui.h>
#include <imgui_impl_vulkan.h>

#include <optional>
#include <source_location>
#include <utility>

namespace Viva {

std::unique_ptr<ImGuiRenderer> ImGuiRenderer::Create(const VulkanContext& context, VkFormat colorFormat,
                                                     VkFormat depthFormat, uint32_t framesInFlight)
{
    // ImGui draws with our version of its fragment shader, which decodes its colors for the sRGB
    // swapchain (see shaders/ImGui.frag).
    std::optional<std::vector<uint8_t>> fragmentShaderCode = ReadCompiledShader("ImGui.frag");
    if (!fragmentShaderCode)
        return nullptr;
    auto renderer = std::make_unique<ImGuiRenderer>();
    renderer->m_FragmentShaderCode = std::move(*fragmentShaderCode);

    // Into an sRGB image the GPU also blends in linear space, where a nearly opaque dark window
    // still lets a bright scene show through clearly. ImGui's colors were designed for blending
    // sRGB values directly, which hides it, so window backgrounds are made opaque.
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    style.Colors[ImGuiCol_PopupBg].w = 1.0f;

    ImGui_ImplVulkan_InitInfo info {
        .ApiVersion = VulkanContext::kApiVersion,
        .Instance = context.GetInstance(),
        .PhysicalDevice = context.GetPhysicalDevice(),
        .Device = context.GetDevice(),
        .QueueFamily = context.GetGraphicsQueueFamily(),
        .Queue = context.GetGraphicsQueue(),
        // ImGui creates a small descriptor pool of its own, for its textures.
        .DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE,
        // Despite the names, ImGui uses these to know how many frames the GPU may still be working
        // on: it rotates through that many vertex buffers, so it never overwrites one in use.
        .MinImageCount = framesInFlight,
        .ImageCount = framesInFlight,
        // Dynamic rendering, like the rest of our drawing: instead of a VkRenderPass, ImGui's
        // pipeline is built for the formats of the attachments it will draw into.
        .PipelineInfoMain = {
            .PipelineRenderingCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                .colorAttachmentCount = 1,
                .pColorAttachmentFormats = &colorFormat, // ImGui keeps a copy
                .depthAttachmentFormat = depthFormat,
            },
        },
        .UseDynamicRendering = true,
        // A Vulkan call that fails inside ImGui is as fatal as one of ours (see VK_CHECK). Results
        // above zero, like VK_SUBOPTIMAL_KHR, are statuses rather than errors.
        .CheckVkResultFn = [](VkResult result) {
            if (result < 0)
                Detail::VulkanCallFailed(result, "a call in Dear ImGui's Vulkan backend", std::source_location::current());
        },
        .CustomShaderFragCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = renderer->m_FragmentShaderCode.size(),
            .pCode = reinterpret_cast<const uint32_t*>(renderer->m_FragmentShaderCode.data()),
        },
    };
    // Builds ImGui's pipeline, sampler and descriptor pool. It always returns true: a mistake in
    // the info above fails an IM_ASSERT instead.
    ImGui_ImplVulkan_Init(&info);
    return renderer;
}

ImGuiRenderer::~ImGuiRenderer()
{
    // Destroys everything the backend made, its textures included. By now the renderer has waited
    // for the GPU to finish all its work.
    ImGui_ImplVulkan_Shutdown();
}

void ImGuiRenderer::NewFrame()
{
    ImGui_ImplVulkan_NewFrame();
}

uint32_t ImGuiRenderer::Record(VkCommandBuffer cmd)
{
    ImDrawData* drawData = ImGui::GetDrawData();
    if (!drawData)
        return 0; // ImGui::Render() wasn't called this frame

    // Uploads this frame's vertices and indices, binds ImGui's pipeline and draws each command,
    // clipped to its rectangle with the scissor. It also sets the viewport and scissor, so it must
    // come after the scene's draws.
    ImGui_ImplVulkan_RenderDrawData(drawData, cmd);

    // ImGui batches the UI into one list per window, of draw commands: each is one draw, except
    // callbacks (code to run in between, which the UI doesn't use).
    uint32_t drawCalls = 0;
    for (const ImDrawList* list : drawData->CmdLists) {
        for (const ImDrawCmd& command : list->CmdBuffer) {
            if (!command.UserCallback)
                ++drawCalls;
        }
    }
    return drawCalls;
}

VkDescriptorSet ImGuiRenderer::AddTexture(VkImageView view)
{
    return ImGui_ImplVulkan_AddTexture(view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void ImGuiRenderer::RemoveTexture(VkDescriptorSet texture)
{
    ImGui_ImplVulkan_RemoveTexture(texture);
}

} // namespace Viva
