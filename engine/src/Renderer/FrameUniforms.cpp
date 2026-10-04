#include "Renderer/FrameUniforms.h"

#include "Renderer/VulkanContext.h"
#include "Renderer/VulkanHelpers.h"

#include <span>

namespace Viva {

std::unique_ptr<FrameUniforms> FrameUniforms::Create(const VulkanContext& context, DescriptorAllocator& descriptors)
{
    VkDevice device = context.GetDevice();
    auto uniforms = std::make_unique<FrameUniforms>(device);

    // 1. The layout: binding 0 is one uniform buffer, read by the vertex shader, at an offset
    //    given when the set is bound ("dynamic"). It matches "layout(set = 0, binding = 0) uniform
    //    Camera" in the shaders.
    const VkDescriptorSetLayoutBinding bindings[] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        },
    };
    uniforms->m_Layout = CreateDescriptorSetLayout(device, bindings);

    // 2. A slot per camera. The GPU only takes offsets that are multiples of its
    //    minUniformBufferOffsetAlignment (often 64 or 256 bytes), so a slot is the struct's size
    //    rounded up to that. The alignment is a power of two, so rounding up is adding
    //    (alignment - 1) and clearing the low bits.
    const uint32_t alignment = context.GetUniformBufferAlignment();
    uniforms->m_SlotSize = (static_cast<uint32_t>(sizeof(CameraUniforms)) + alignment - 1) & ~(alignment - 1);

    // 3. Per frame: a buffer with all the slots, a set from the renderer's descriptor allocator,
    //    and the set's binding 0 pointed at one slot's worth of the buffer (the offset picks which).
    //    The buffers never change, so the sets are written once.
    for (uint32_t i = 0; i < FrameResources::kFramesInFlight; ++i) {
        uniforms->m_Buffers[i] = Buffer::Create(context, VkDeviceSize { uniforms->m_SlotSize } * kMaxViews,
                                                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, MemoryLocation::CpuToGpu);
        uniforms->m_Sets[i] = descriptors.Allocate(uniforms->m_Layout).Set;
        WriteDynamicUniformBufferDescriptor(device, uniforms->m_Sets[i], 0, uniforms->m_Buffers[i]->GetHandle(),
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

void FrameUniforms::Write(uint32_t frameIndex, uint32_t view, const CameraUniforms& camera)
{
    m_Buffers[frameIndex]->Write(std::as_bytes(std::span(&camera, 1)), GetOffset(view));
}

} // namespace Viva
