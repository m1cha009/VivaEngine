#pragma once

#include "Renderer/Buffer.h"
#include "Renderer/DescriptorAllocator.h"
#include "Renderer/FrameResources.h"

#include <glm/mat4x4.hpp>
#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <memory>

namespace Viva {

class VulkanContext;

// The data every draw of one view shares, laid out exactly like "uniform Camera" in
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
// A frame can show the scene from several cameras (M18: the window, the editor's Scene view, its
// Game view). Each buffer has a slot per camera, and the descriptor is a *dynamic* uniform buffer:
// binding the set takes an offset, which picks the slot. One set serves every camera; only the
// offset changes. (The other way, a set per camera, would need as many sets.)
//
// Descriptors are how shaders find resources (buffers, textures):
//   VkDescriptorSetLayout  The shape: "binding 0 is a uniform buffer the vertex shader reads".
//                          Pipelines are built against it.
//   VkDescriptorPool       Memory that descriptor sets are allocated from (DescriptorAllocator).
//   VkDescriptorSet        An actual set: "binding 0 is this buffer". Bound before drawing.
class FrameUniforms {
public:
    // How many cameras one frame can draw from.
    static constexpr uint32_t kMaxViews = 4;

    // The sets come from `descriptors`. They live as long as the renderer, so they're never given
    // back one by one: destroying the allocator's pools frees them.
    static std::unique_ptr<FrameUniforms> Create(const VulkanContext& context, DescriptorAllocator& descriptors);

    explicit FrameUniforms(VkDevice device); // creates nothing: use Create()
    ~FrameUniforms();

    FrameUniforms(const FrameUniforms&) = delete;
    FrameUniforms& operator=(const FrameUniforms&) = delete;

    VkDescriptorSetLayout GetLayout() const { return m_Layout; }
    VkDescriptorSet GetSet(uint32_t frameIndex) const { return m_Sets[frameIndex]; }
    // The dynamic offset that makes the set read camera `view`'s slot.
    uint32_t GetOffset(uint32_t view) const { return view * m_SlotSize; }

    // Fills camera `view`'s slot in frame `frameIndex`'s buffer. Only once the GPU is done with
    // that frame (after its fence).
    void Write(uint32_t frameIndex, uint32_t view, const CameraUniforms& camera);

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_Layout = VK_NULL_HANDLE;
    uint32_t m_SlotSize = 0; // sizeof(CameraUniforms), rounded up to the GPU's offset alignment
    std::array<std::unique_ptr<Buffer>, FrameResources::kFramesInFlight> m_Buffers;
    std::array<VkDescriptorSet, FrameResources::kFramesInFlight> m_Sets {};
};

} // namespace Viva
