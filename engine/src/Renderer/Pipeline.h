#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Viva {

// Reads a compiled shader, named as in the shaders/ folder: "Unlit.vert" reads the SPIR-V file
// shaders/Unlit.vert.spv from next to the executable. Returns std::nullopt (after logging why)
// if it can't.
std::optional<std::vector<uint8_t>> ReadCompiledShader(const std::string& name);

// What to build a pipeline from. Fill it with designated initializers, like ApplicationSettings:
//     Pipeline::Create(device, { .VertexShader = "Unlit.vert", .FragmentShader = "Unlit.frag",
//                                .ColorFormat = format });
// Fields left out keep their defaults (the fields must be named in the order they're declared).
struct PipelineSettings {
    // Shader names as in the shaders/ folder. "Unlit.vert" loads the compiled
    // shaders/Unlit.vert.spv from next to the executable.
    std::string VertexShader;
    std::string FragmentShader;
    // How the vertex shader's inputs are read from vertex buffers: kVertexBindings and
    // kVertexAttributes (Mesh.h) for meshes. Left empty, the pipeline reads no vertex buffers and
    // the vertex shader makes up its own vertices, like M4's triangle did.
    std::span<const VkVertexInputBindingDescription> VertexBindings;
    std::span<const VkVertexInputAttributeDescription> VertexAttributes;
    // The pipeline layout: what the shaders receive besides vertices (descriptor sets, push
    // constants). The pipeline uses it but doesn't own it: the renderer shares one layout among
    // all its pipelines, so descriptor sets stay bound when the pipeline changes.
    VkPipelineLayout Layout = VK_NULL_HANDLE;
    // The format of the image the pipeline draws into (the swapchain's).
    VkFormat ColorFormat = VK_FORMAT_UNDEFINED;
    // The depth buffer's format. Set, it turns on depth testing: a pixel is only drawn if it's at
    // least as near as what's already there (Unity's "ZTest LEqual" + "ZWrite On").
    VkFormat DepthFormat = VK_FORMAT_UNDEFINED;
    // Which triangles to skip: VK_CULL_MODE_BACK_BIT skips those facing away from the camera
    // (Unity's "Cull Back"). Front faces are the ones whose corners appear counter-clockwise.
    VkCullModeFlags CullMode = VK_CULL_MODE_NONE;
    // Whether drawn pixels store their depth (Unity's "ZWrite On"). Something see-through, like
    // the grid, tests against the depth buffer but leaves it alone, so it can't hide what's
    // drawn after it. Only matters with a DepthFormat.
    bool DepthWrite = true;
    // Moves every depth this far towards the camera, in steps of the smallest depth difference
    // the buffer can tell apart (plus as much again per unit of slope, for surfaces seen at a
    // grazing angle). Negative values pull nearer. Something drawn exactly on another surface,
    // like the grid on a floor at y = 0, wins the depth test with it instead of flickering
    // ("z-fighting"; Unity's "Offset -1, -1").
    float DepthBias = 0.0f;
    // Blends with what's already in the image by the fragment shader's alpha: alpha 1 covers it,
    // 0 leaves it (Unity's "Blend SrcAlpha OneMinusSrcAlpha"). Off, each pixel replaces what was
    // there.
    bool AlphaBlend = false;
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

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    VkPipeline m_Pipeline = VK_NULL_HANDLE;
};

} // namespace Viva
