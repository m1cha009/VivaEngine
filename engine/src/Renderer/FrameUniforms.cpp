#include "Renderer/FrameUniforms.h"

#include "Renderer/VulkanContext.h"
#include "Renderer/VulkanHelpers.h"

#include <span>

namespace Viva {

std::unique_ptr<FrameUniforms> FrameUniforms::Create(const VulkanContext& context)
{
    VkDevice device = context.GetDevice();
    auto uniforms = std::make_unique<FrameUniforms>(device);
    constexpr uint32_t kFrames = FrameResources::kFramesInFlight;

    // 1. The layout: binding 0 is one uniform buffer, read by the vertex shader. It matches
    //    "layout(set = 0, binding = 0) uniform Camera" in the shader.
    const VkDescriptorSetLayoutBinding bindings[] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        },
    };
    uniforms->m_Layout = CreateDescriptorSetLayout(device, bindings);

    // 2. The pool: room for one set per frame, each holding one uniform buffer descriptor.
    const VkDescriptorPoolSize poolSizes[] = { { .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = kFrames } };
    uniforms->m_Pool = CreateDescriptorPool(device, kFrames, poolSizes);

    // 3. Per frame: a set, a buffer, and the set's binding 0 pointed at the buffer. The buffers
    //    never change, so the sets are written once, here.
    for (uint32_t i = 0; i < kFrames; ++i) {
        uniforms->m_Sets[i] = AllocateDescriptorSet(device, uniforms->m_Pool, uniforms->m_Layout);
        uniforms->m_Buffers[i] = Buffer::Create(context, sizeof(CameraUniforms), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                MemoryLocation::CpuToGpu);
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
    // Destroying the pool frees the sets allocated from it. The buffers are destroyed after this
    // body, as members.
    vkDestroyDescriptorPool(m_Device, m_Pool, nullptr);
    vkDestroyDescriptorSetLayout(m_Device, m_Layout, nullptr);
}

void FrameUniforms::Write(uint32_t frameIndex, const CameraUniforms& camera)
{
    m_Buffers[frameIndex]->Write(std::as_bytes(std::span(&camera, 1)));
}

} // namespace Viva
