#pragma once

#include <vulkan/vulkan.h>

#include <memory>
#include <string>

namespace Viva {

// What to build a pipeline from. Fill it with designated initializers, like ApplicationSettings:
//     Pipeline::Create(device, { .VertexShader = "Triangle.vert", .FragmentShader = "Triangle.frag",
//                                .ColorFormat = format });
// Later milestones add fields with defaults (the vertex layout in M5, depth and culling in M6),
// so existing callers keep working.
struct PipelineSettings {
    // Shader names as in the shaders/ folder. "Triangle.vert" loads the compiled
    // shaders/Triangle.vert.spv from next to the executable.
    std::string VertexShader;
    std::string FragmentShader;
    // The format of the image the pipeline draws into (the swapchain's).
    VkFormat ColorFormat = VK_FORMAT_UNDEFINED;
};

// A graphics pipeline: everything about *how* the GPU draws, baked into one object. That's the
// compiled shaders, plus the fixed-function settings around them (triangles or lines, culling,
// blending, the formats of the images it draws into). In Unity terms it's close to a compiled
// shader pass with its render state ("Cull Off", "Blend ...", ZWrite).
//
// Vulkan builds all of that up front, so the driver can compile it once instead of guessing
// and recompiling while you draw.
class Pipeline {
public:
    // Loads the shaders and builds the pipeline. Returns nullptr (after logging why) on failure.
    static std::unique_ptr<Pipeline> Create(VkDevice device, const PipelineSettings& settings);

    explicit Pipeline(VkDevice device);
    ~Pipeline();

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    VkPipeline GetHandle() const { return m_Pipeline; }
    VkPipelineLayout GetLayout() const { return m_Layout; }

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    // The layout lists what the shaders get from outside besides vertices (descriptor sets, push
    // constants). Empty for now: M6 adds the camera and model matrices.
    VkPipelineLayout m_Layout = VK_NULL_HANDLE;
    VkPipeline m_Pipeline = VK_NULL_HANDLE;
};

} // namespace Viva
