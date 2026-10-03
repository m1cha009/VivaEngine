#pragma once

#include <vulkan/vulkan.h>

#include <cstddef>
#include <memory>
#include <span>

namespace Viva {

class VulkanContext;

// Where a buffer's memory lives, which decides who can use it quickly.
enum class MemoryLocation {
    // GPU memory (VRAM on a graphics card): the fastest for drawing from, but usually out of the
    // CPU's reach. The GPU fills it by copying from a staging buffer (see CreateWithData).
    Gpu,
    // Memory the CPU can write and the GPU can read, usually ordinary RAM that the GPU reads over
    // the bus. For data in transit (staging buffers), or data the CPU rewrites often.
    CpuToGpu,
};

// A Vulkan buffer: a block of memory the GPU can use, holding vertices, indices, uniforms or
// anything else. In Unity it hides inside Mesh and GraphicsBuffer.
//
// This version manages the memory by hand: it asks the buffer what memory it needs, picks a
// suitable memory type, allocates it with vkAllocateMemory and binds it to the buffer.
class Buffer {
public:
    // A buffer of `size` bytes for `usage` (vertex buffer, index buffer, copy source...).
    static std::unique_ptr<Buffer> Create(const VulkanContext& context, VkDeviceSize size, VkBufferUsageFlags usage,
                                          MemoryLocation location);

    // A buffer in GPU memory holding `data`, copied there through a temporary staging buffer.
    // The copy has finished when this returns.
    static std::unique_ptr<Buffer> CreateWithData(const VulkanContext& context, std::span<const std::byte> data,
                                                  VkBufferUsageFlags usage);

    explicit Buffer(VkDevice device); // creates nothing: use Create()
    ~Buffer();

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    VkBuffer GetHandle() const { return m_Buffer; }
    VkDeviceSize GetSize() const { return m_Size; }

    // Copies `data` to the start of the buffer. Only for CpuToGpu buffers.
    void Write(std::span<const std::byte> data);

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    VkBuffer m_Buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_Memory = VK_NULL_HANDLE;
    VkDeviceSize m_Size = 0;
    void* m_Mapped = nullptr; // where the CPU sees a CpuToGpu buffer's memory; null for Gpu ones
};

} // namespace Viva
