#include "Renderer/FrameUniforms.h"

#include "Renderer/VulkanCheck.h"
#include "Renderer/VulkanContext.h"

#include <span>

namespace Viva {

std::unique_ptr<FrameUniforms> FrameUniforms::Create(const VulkanContext& context)
{
    VkDevice device = context.GetDevice();
    auto uniforms = std::make_unique<FrameUniforms>(device);
    constexpr uint32_t kFrames = FrameResources::kFramesInFlight;

    // 1. The layout: binding 0 is one uniform buffer, read by the vertex shader. It matches
    //    "layout(set = 0, binding = 0) uniform Camera" in the shader.
    const VkDescriptorSetLayoutBinding binding {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    };
    const VkDescriptorSetLayoutCreateInfo layoutInfo {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &binding,
    };
    VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &uniforms->m_Layout));

    // 2. The pool: room for one set per frame, each holding one uniform buffer descriptor.
    const VkDescriptorPoolSize poolSize { .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = kFrames };
    const VkDescriptorPoolCreateInfo poolInfo {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = kFrames,
        .poolSizeCount = 1,
        .pPoolSizes = &poolSize,
    };
    VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &uniforms->m_Pool));

    // 3. The sets, all with the same layout (the allocate call takes one layout per set).
    std::array<VkDescriptorSetLayout, kFrames> layouts;
    layouts.fill(uniforms->m_Layout);
    const VkDescriptorSetAllocateInfo allocateInfo {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = uniforms->m_Pool,
        .descriptorSetCount = kFrames,
        .pSetLayouts = layouts.data(),
    };
    VK_CHECK(vkAllocateDescriptorSets(device, &allocateInfo, uniforms->m_Sets.data()));

    // 4. A buffer per frame, and each set's binding 0 pointed at its frame's buffer. The buffers
    //    never change, so the sets are written once, here.
    for (uint32_t i = 0; i < kFrames; ++i) {
        uniforms->m_Buffers[i] = Buffer::Create(context, sizeof(CameraUniforms), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                MemoryLocation::CpuToGpu);
        const VkDescriptorBufferInfo bufferInfo {
            .buffer = uniforms->m_Buffers[i]->GetHandle(),
            .offset = 0,
            .range = sizeof(CameraUniforms),
        };
        const VkWriteDescriptorSet write {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = uniforms->m_Sets[i],
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &bufferInfo,
        };
        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
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
