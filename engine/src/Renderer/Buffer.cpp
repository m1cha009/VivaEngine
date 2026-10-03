#include "Renderer/Buffer.h"

#include "Renderer/VulkanCheck.h"
#include "Renderer/VulkanContext.h"
#include "Viva/Assert.h"

#include <cstring>

namespace Viva {

namespace {

// A GPU offers its memory as a few "memory types", each with property flags:
//   DEVICE_LOCAL   On the GPU itself (VRAM on a graphics card): the fastest for the GPU, but the
//                  CPU usually can't touch it.
//   HOST_VISIBLE   The CPU can map it (get a pointer to it) and write into it. Usually ordinary
//                  RAM, which the GPU reads over the PCIe bus, so it's slower to draw from.
//   HOST_COHERENT  The CPU's writes reach the GPU without an explicit "flush" call.
// (In Debug builds the log lists this GPU's memory types at startup.) A buffer reports which types
// it can live in as a bit mask, allowedTypes; we pick the first of those with all the properties
// we want.
uint32_t FindMemoryType(VkPhysicalDevice gpu, uint32_t allowedTypes, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memory {};
    vkGetPhysicalDeviceMemoryProperties(gpu, &memory);
    for (uint32_t i = 0; i < memory.memoryTypeCount; ++i) {
        const bool allowed = (allowedTypes & (1u << i)) != 0;
        const bool hasProperties = (memory.memoryTypes[i].propertyFlags & properties) == properties;
        if (allowed && hasProperties)
            return i;
    }
    return UINT32_MAX;
}

} // namespace

std::unique_ptr<Buffer> Buffer::Create(const VulkanContext& context, VkDeviceSize size, VkBufferUsageFlags usage,
                                       MemoryLocation location)
{
    VIVA_ASSERT(size > 0, "a Vulkan buffer can't be empty");
    VkDevice device = context.GetDevice();
    auto buffer = std::make_unique<Buffer>(device);
    buffer->m_Size = size;

    // 1. The buffer object: its size and what it will be used for. It has no memory yet.
    const VkBufferCreateInfo bufferInfo {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE, // used by one queue family at a time
    };
    VK_CHECK(vkCreateBuffer(device, &bufferInfo, nullptr, &buffer->m_Buffer));

    // 2. The buffer says what memory it needs: how many bytes (can be more than its size), the
    //    alignment, and which memory types it can live in.
    VkMemoryRequirements requirements {};
    vkGetBufferMemoryRequirements(device, buffer->m_Buffer, &requirements);

    // 3. Pick a memory type with the right properties for the location, and allocate a block of it.
    const VkMemoryPropertyFlags properties = location == MemoryLocation::Gpu
        ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        : VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    const uint32_t memoryType = FindMemoryType(context.GetPhysicalDevice(), requirements.memoryTypeBits, properties);
    VIVA_ASSERT(memoryType != UINT32_MAX, "no memory type has the properties this buffer needs");

    const VkMemoryAllocateInfo allocateInfo {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size,
        .memoryTypeIndex = memoryType,
    };
    VK_CHECK(vkAllocateMemory(device, &allocateInfo, nullptr, &buffer->m_Memory));

    // 4. Attach the memory to the buffer. Offset 0: the buffer gets the whole block.
    VK_CHECK(vkBindBufferMemory(device, buffer->m_Buffer, buffer->m_Memory, 0));

    // 5. Memory the CPU can see is mapped once and stays mapped: m_Mapped is the address where the
    //    CPU sees it, and Write() copies straight there.
    if (location == MemoryLocation::CpuToGpu)
        VK_CHECK(vkMapMemory(device, buffer->m_Memory, 0, VK_WHOLE_SIZE, 0, &buffer->m_Mapped));
    return buffer;
}

std::unique_ptr<Buffer> Buffer::CreateWithData(const VulkanContext& context, std::span<const std::byte> data,
                                               VkBufferUsageFlags usage)
{
    // The CPU can't write GPU memory, so the data makes two hops. First the CPU copies it into a
    // staging buffer, in memory both can reach...
    std::unique_ptr<Buffer> staging = Create(context, data.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                             MemoryLocation::CpuToGpu);
    staging->Write(data);

    // ...then the GPU copies it from there into the real buffer. TRANSFER_DST: copies can write
    // into it.
    std::unique_ptr<Buffer> buffer = Create(context, data.size(), usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                            MemoryLocation::Gpu);

    context.ImmediateSubmit([&](VkCommandBuffer cmd) {
        const VkBufferCopy region { .size = data.size() };
        vkCmdCopyBuffer(cmd, staging->GetHandle(), buffer->GetHandle(), 1, &region);

        // A barrier orders later commands too, including those submitted later on this queue: the
        // copy's writes must be finished and visible before anything reads the buffer. "Anything"
        // (ALL_COMMANDS, MEMORY_READ) is broader than needed, but this only runs while loading.
        const VkBufferMemoryBarrier2 barrier {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = buffer->GetHandle(),
            .size = VK_WHOLE_SIZE,
        };
        const VkDependencyInfo dependency {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .bufferMemoryBarrierCount = 1,
            .pBufferMemoryBarriers = &barrier,
        };
        vkCmdPipelineBarrier2(cmd, &dependency);
    });

    // ImmediateSubmit waited for the GPU, so the staging buffer is no longer in use: it's
    // destroyed when this function returns.
    return buffer;
}

Buffer::Buffer(VkDevice device)
    : m_Device(device)
{
}

Buffer::~Buffer()
{
    // Freeing mapped memory unmaps it too.
    vkDestroyBuffer(m_Device, m_Buffer, nullptr);
    vkFreeMemory(m_Device, m_Memory, nullptr);
}

void Buffer::Write(std::span<const std::byte> data)
{
    VIVA_ASSERT(m_Mapped, "only CpuToGpu buffers can be written by the CPU");
    VIVA_ASSERT(data.size() <= m_Size, "writing {} bytes into a {}-byte buffer", data.size(), m_Size);
    // The memory is HOST_COHERENT, so the GPU sees these bytes without a flush.
    std::memcpy(m_Mapped, data.data(), data.size());
}

} // namespace Viva
