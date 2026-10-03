#include "Renderer/FrameUniforms.h"

#include "Renderer/VulkanContext.h"
#include "Renderer/VulkanHelpers.h"

#include <span>

namespace Viva {

std::unique_ptr<FrameUniforms> FrameUniforms::Create(const VulkanContext& context, DescriptorAllocator& descriptors)
{
    VkDevice device = context.GetDevice();
    auto uniforms = std::make_unique<FrameUniforms>(device);

    // 1. The layout: binding 0 is one uniform buffer, read by the vertex shader. It matches
    //    "layout(set = 0, binding = 0) uniform Camera" in the shaders.
    const VkDescriptorSetLayoutBinding bindings[] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        },
    };
    uniforms->m_Layout = CreateDescriptorSetLayout(device, bindings);

    // 2. Per frame: a buffer, a set from the renderer's descriptor allocator, and the set's
    //    binding 0 pointed at the buffer. The buffers never change, so the sets are written once.
    for (uint32_t i = 0; i < FrameResources::kFramesInFlight; ++i) {
        uniforms->m_Buffers[i] = Buffer::Create(context, sizeof(CameraUniforms), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                MemoryLocation::CpuToGpu);
        uniforms->m_Sets[i] = descriptors.Allocate(uniforms->m_Layout).Set;
        WriteUniformBufferDescriptor(device, uniforms->m_Sets[i], 0, uniforms->m_Buffers[i]->GetHandle(),
                                     sizeof(CameraUniforms));
    }
    return uniforms;
}

FrameUniforms::FrameUniforms(VkDevice device)
    : m_Device(device)
{
}

FrameUniforms::~FrameUniforms()
{
    vkDestroyDescriptorSetLayout(m_Device, m_Layout, nullptr);
}

void FrameUniforms::Write(uint32_t frameIndex, const CameraUniforms& camera)
{
    m_Buffers[frameIndex]->Write(std::as_bytes(std::span(&camera, 1)));
}

} // namespace Viva
