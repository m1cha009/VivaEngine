#include "Renderer/Pipeline.h"

#include "Platform/FileSystem.h"
#include "Renderer/VulkanCheck.h"

#include <iterator>
#include <optional>
#include <vector>

namespace Viva {

namespace {

// A shader module wraps SPIR-V code for the pipeline to use. It's only needed while the pipeline
// is being built.
VkShaderModule CreateShaderModule(VkDevice device, const std::vector<uint8_t>& code)
{
    const VkShaderModuleCreateInfo info {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = code.size(),
        .pCode = reinterpret_cast<const uint32_t*>(code.data()), // SPIR-V is a list of 32-bit words
    };
    VkShaderModule module = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(device, &info, nullptr, &module));
    return module;
}

} // namespace

std::optional<std::vector<uint8_t>> ReadCompiledShader(const std::string& name)
{
    // Where the build puts a compiled shader: shaders/<name>.spv next to the executable (see
    // cmake/Shaders.cmake). This is the only place that knows. ("/" works as a separator on
    // Windows too.)
    return ReadBinaryFile(GetExecutableDirectory() + "shaders/" + name + ".spv");
}

std::unique_ptr<Pipeline> Pipeline::Create(VkDevice device, const PipelineSettings& settings)
{
    // Read both files first: the only step here that can fail on a healthy machine (a missing
    // file). ReadBinaryFile logs which one.
    const std::optional<std::vector<uint8_t>> vertexCode = ReadCompiledShader(settings.VertexShader);
    const std::optional<std::vector<uint8_t>> fragmentCode = ReadCompiledShader(settings.FragmentShader);
    if (!vertexCode || !fragmentCode)
        return nullptr;

    const VkShaderModule vertexModule = CreateShaderModule(device, *vertexCode);
    const VkShaderModule fragmentModule = CreateShaderModule(device, *fragmentCode);

    // The programmable stages: which shader runs for vertices and which for pixels, starting at
    // their main() functions.
    const VkPipelineShaderStageCreateInfo stages[] = {
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vertexModule, .pName = "main" },
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = fragmentModule, .pName = "main" },
    };

    // Where the vertex shader's inputs come from: which vertex buffers ("bindings"), and where
    // each input ("attribute") sits inside a vertex.
    const VkPipelineVertexInputStateCreateInfo vertexInput {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = static_cast<uint32_t>(settings.VertexBindings.size()),
        .pVertexBindingDescriptions = settings.VertexBindings.data(),
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(settings.VertexAttributes.size()),
        .pVertexAttributeDescriptions = settings.VertexAttributes.data(),
    };

    // Every three vertices form one triangle.
    const VkPipelineInputAssemblyStateCreateInfo inputAssembly {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };

    // One viewport (the part of the image that maps to clip space) and one scissor rectangle
    // (pixels outside it are skipped). Their actual sizes are "dynamic state": set while recording
    // each frame, so the pipeline doesn't need rebuilding when the window is resized.
    const VkPipelineViewportStateCreateInfo viewport {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };
    const VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    const VkPipelineDynamicStateCreateInfo dynamic {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<uint32_t>(std::size(dynamicStates)),
        .pDynamicStates = dynamicStates,
    };

    // Filled triangles, culled as the settings say. A triangle's front is the side from which its
    // corners appear counter-clockwise on screen: the convention of glTF and OpenGL-style tools.
    // (Unity uses clockwise.)
    const VkPipelineRasterizationStateCreateInfo rasterization {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = settings.CullMode,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.0f,
    };

    // One sample per pixel: no anti-aliasing.
    const VkPipelineMultisampleStateCreateInfo multisample {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    // No blending: each pixel's color replaces what was there (Unity's "Blend Off").
    const VkPipelineColorBlendAttachmentState blendAttachment {
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                          VK_COLOR_COMPONENT_A_BIT,
    };
    const VkPipelineColorBlendStateCreateInfo colorBlend {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blendAttachment,
    };

    // Depth testing, when there's a depth buffer: each pixel stores the depth of what was drawn
    // there, and a new pixel is only drawn (and its depth stored) if it's at least as near, that
    // is LESS_OR_EQUAL (Unity's default "ZTest LEqual"). Without a depth buffer this is off, and
    // later draws simply cover earlier ones.
    const bool depth = settings.DepthFormat != VK_FORMAT_UNDEFINED;
    const VkPipelineDepthStencilStateCreateInfo depthStencil {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = depth ? VK_TRUE : VK_FALSE,
        .depthWriteEnable = depth ? VK_TRUE : VK_FALSE,
        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
    };

    // Dynamic rendering: instead of pointing at a VkRenderPass, the pipeline just states the
    // formats of the images it will draw into.
    const VkPipelineRenderingCreateInfo rendering {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &settings.ColorFormat,
        .depthAttachmentFormat = settings.DepthFormat,
    };

    auto pipeline = std::make_unique<Pipeline>(device);

    const VkGraphicsPipelineCreateInfo createInfo {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &rendering,
        .stageCount = static_cast<uint32_t>(std::size(stages)),
        .pStages = stages,
        .pVertexInputState = &vertexInput,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewport,
        .pRasterizationState = &rasterization,
        .pMultisampleState = &multisample,
        .pDepthStencilState = &depthStencil,
        .pColorBlendState = &colorBlend,
        .pDynamicState = &dynamic,
        .layout = settings.Layout,
    };
    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline->m_Pipeline));

    // The pipeline keeps its own compiled copy of the shaders, so the modules can go.
    vkDestroyShaderModule(device, vertexModule, nullptr);
    vkDestroyShaderModule(device, fragmentModule, nullptr);
    return pipeline;
}

Pipeline::Pipeline(VkDevice device)
    : m_Device(device)
{
}

Pipeline::~Pipeline()
{
    vkDestroyPipeline(m_Device, m_Pipeline, nullptr);
}

} // namespace Viva
