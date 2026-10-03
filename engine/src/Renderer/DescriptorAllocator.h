#pragma once

#include <vulkan/vulkan.h>

#include <vector>

namespace Viva {

// A descriptor set, and the pool it came from (needed to free it).
struct DescriptorAllocation {
    VkDescriptorSet Set = VK_NULL_HANDLE;
    VkDescriptorPool Pool = VK_NULL_HANDLE;
};

// Hands out descriptor sets for the whole renderer: the per-frame camera sets and one set per
// material. A descriptor pool has a fixed capacity, so this keeps a list of pools and opens a
// new one whenever the existing ones are full. Sets can be given back one at a time (a material
// being destroyed), and destroying the allocator frees all of them at once.
class DescriptorAllocator {
public:
    explicit DescriptorAllocator(VkDevice device);
    ~DescriptorAllocator();

    DescriptorAllocator(const DescriptorAllocator&) = delete;
    DescriptorAllocator& operator=(const DescriptorAllocator&) = delete;

    DescriptorAllocation Allocate(VkDescriptorSetLayout layout);
    void Free(const DescriptorAllocation& allocation);

private:
    VkDescriptorPool CreatePool();

    VkDevice m_Device = VK_NULL_HANDLE;
    std::vector<VkDescriptorPool> m_Pools;
};

} // namespace Viva
