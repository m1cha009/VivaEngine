#pragma once

#include "Renderer/Buffer.h"
#include "Renderer/DescriptorAllocator.h"
#include "Renderer/FrameResources.h"

#include <glm/mat4x4.hpp>
#include <vulkan/vulkan.h>

#include <array>
#include <memory>

namespace Viva {

class VulkanContext;

// The data every draw in a frame shares, laid out exactly like "uniform Camera" in
// shaders/Unlit.vert. GLSL lays out uniform blocks by the "std140" rules; a mat4 there is
// 64 bytes, column after column, the same as glm::mat4, so this struct can be copied as is.
struct CameraUniforms {
    glm::mat4 View;
    glm::mat4 Projection;
};

// The per-frame uniform buffers and the descriptor sets that hand them to the shaders.
//
// There's one buffer per frame in flight: while the GPU draws frame N from one buffer, the CPU
// fills the other one for frame N+1. With a single buffer, the CPU would overwrite the camera
// the GPU is still drawing with.
//
// Descriptors are how shaders find resources (buffers, textures):
//   VkDescriptorSetLayout  The shape: "binding 0 is a uniform buffer the vertex shader reads".
//                          Pipelines are built against it.
//   VkDescriptorPool       Memory that descriptor sets are allocated from (DescriptorAllocator).
//   VkDescriptorSet        An actual set: "binding 0 is this buffer". Bound before drawing.
class FrameUniforms {
public:
    // The sets come from `descriptors`. They live as long as the renderer, so they're never given
    // back one by one: destroying the allocator's pools frees them.
    static std::unique_ptr<FrameUniforms> Create(const VulkanContext& context, DescriptorAllocator& descriptors);

    explicit FrameUniforms(VkDevice device); // creates nothing: use Create()
    ~FrameUniforms();

    FrameUniforms(const FrameUniforms&) = delete;
    FrameUniforms& operator=(const FrameUniforms&) = delete;

    VkDescriptorSetLayout GetLayout() const { return m_Layout; }
    VkDescriptorSet GetSet(uint32_t frameIndex) const { return m_Sets[frameIndex]; }

    // Fills frame `frameIndex`'s buffer. Only once the GPU is done with that frame (after its fence).
    void Write(uint32_t frameIndex, const CameraUniforms& camera);

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_Layout = VK_NULL_HANDLE;
    std::array<std::unique_ptr<Buffer>, FrameResources::kFramesInFlight> m_Buffers;
    std::array<VkDescriptorSet, FrameResources::kFramesInFlight> m_Sets {};
};

} // namespace Viva
