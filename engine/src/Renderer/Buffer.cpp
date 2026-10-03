#include "Renderer/Buffer.h"

#include "Renderer/VulkanCheck.h"
#include "Renderer/VulkanContext.h"
#include "Viva/Assert.h"

#include <cstring>

namespace Viva {

std::unique_ptr<Buffer> Buffer::Create(const VulkanContext& context, VkDeviceSize size, VkBufferUsageFlags usage,
                                       MemoryLocation location)
{
    VIVA_ASSERT(size > 0, "a Vulkan buffer can't be empty");
    auto buffer = std::make_unique<Buffer>(context.GetAllocator());
    buffer->m_Size = size;

    // The buffer itself: its size and what it will be used for.
    const VkBufferCreateInfo bufferInfo {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE, // used by one queue family at a time
    };

    // Instead of naming memory properties, we tell VMA how the memory will be used, and it picks
    // the best memory type on this GPU. AUTO: decide from the buffer's usage. For CpuToGpu we add
    // that the CPU writes it front to back and never reads it (so uncached memory is fine), and
    // that it should stay mapped for as long as it exists.
    VmaAllocationCreateInfo allocationInfo { .usage = VMA_MEMORY_USAGE_AUTO };
    if (location == MemoryLocation::CpuToGpu)
        allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    // One call does all the manual steps: create the buffer, find a memory type, find room for it
    // in one of VMA's blocks (allocating a new block only when they're full), and bind.
    VmaAllocationInfo allocated {};
    VK_CHECK(vmaCreateBuffer(buffer->m_Allocator, &bufferInfo, &allocationInfo, &buffer->m_Buffer,
                             &buffer->m_Allocation, &allocated));
    buffer->m_Mapped = allocated.pMappedData; // null unless mapped
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

Buffer::Buffer(VmaAllocator allocator)
    : m_Allocator(allocator)
{
}

Buffer::~Buffer()
{
    // Destroys the buffer and gives its place in the block back to VMA (unmapping it if mapped).
    vmaDestroyBuffer(m_Allocator, m_Buffer, m_Allocation);
}

void Buffer::Write(std::span<const std::byte> data)
{
    VIVA_ASSERT(m_Mapped, "only CpuToGpu buffers can be written by the CPU");
    VIVA_ASSERT(data.size() <= m_Size, "writing {} bytes into a {}-byte buffer", data.size(), m_Size);
    std::memcpy(m_Mapped, data.data(), data.size());
    // VMA may have picked memory that isn't HOST_COHERENT. Then the CPU's writes only reach the
    // GPU after a flush. On coherent memory this does nothing.
    VK_CHECK(vmaFlushAllocation(m_Allocator, m_Allocation, 0, data.size()));
}

} // namespace Viva
