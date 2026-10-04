#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <cstddef>
#include <memory>
#include <span>

namespace Viva {

class VulkanContext;

// Who uses a buffer's memory, which decides where it should live.
enum class MemoryLocation {
    // Only the GPU: memory on the GPU itself (VRAM on a graphics card), the fastest to draw from
    // but usually out of the CPU's reach. The GPU fills it by copying from a staging buffer (see
    // CreateWithData).
    Gpu,
    // The CPU writes, the GPU reads: memory the CPU can map. VMA picks where: ordinary RAM that
    // the GPU reads over the bus (staging buffers), or, on GPUs that offer it, VRAM the CPU can
    // write directly (good for small data rewritten every frame). For data in transit, or data
    // the CPU changes often.
    CpuToGpu,
};

// A Vulkan buffer: a block of memory the GPU can use, holding vertices, indices, uniforms or
// anything else. In Unity it hides inside Mesh and GraphicsBuffer.
//
// Its memory comes from VMA (Vulkan Memory Allocator), which allocates big blocks of GPU memory
// and places many buffers inside each one. (The first M5 commit does it all by hand, one
// vkAllocateMemory per buffer: see docs/milestones/M5.md.)
class Buffer {
public:
    // A buffer of `size` bytes for `usage` (vertex buffer, index buffer, copy source...).
    static std::unique_ptr<Buffer> Create(const VulkanContext& context, VkDeviceSize size, VkBufferUsageFlags usage,
                                          MemoryLocation location);

    // A buffer in GPU memory holding `data`, copied there through a temporary staging buffer.
    // The copy has finished when this returns.
    static std::unique_ptr<Buffer> CreateWithData(const VulkanContext& context, std::span<const std::byte> data,
                                                  VkBufferUsageFlags usage);

    explicit Buffer(VmaAllocator allocator); // creates nothing: use Create()
    ~Buffer();

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    VkBuffer GetHandle() const { return m_Buffer; }
    VkDeviceSize GetSize() const { return m_Size; }

    // Copies `data` into the buffer, `offset` bytes from its start. Only for CpuToGpu buffers.
    void Write(std::span<const std::byte> data, VkDeviceSize offset = 0);

private:
    VmaAllocator m_Allocator = VK_NULL_HANDLE;
    VkBuffer m_Buffer = VK_NULL_HANDLE;
    VmaAllocation m_Allocation = VK_NULL_HANDLE; // the buffer's place inside one of VMA's blocks
    VkDeviceSize m_Size = 0;
    void* m_Mapped = nullptr; // where the CPU sees a CpuToGpu buffer's memory; null for Gpu ones
};

} // namespace Viva
