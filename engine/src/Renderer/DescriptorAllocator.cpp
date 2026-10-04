#include "Renderer/DescriptorAllocator.h"

#include "Renderer/VulkanCheck.h"

#include <iterator>
#include <ranges>

namespace Viva {

namespace {

// Each pool's capacity: this many sets, holding up to this many descriptors of each type the
// renderer uses.
constexpr uint32_t kSetsPerPool = 64;

} // namespace

DescriptorAllocator::DescriptorAllocator(VkDevice device)
    : m_Device(device)
{
}

DescriptorAllocator::~DescriptorAllocator()
{
    // Destroying a pool frees every set allocated from it.
    for (VkDescriptorPool pool : m_Pools)
        vkDestroyDescriptorPool(m_Device, pool, nullptr);
}

DescriptorAllocation DescriptorAllocator::Allocate(VkDescriptorSetLayout layout)
{
    DescriptorAllocation allocation;
    // Tries one pool. A full pool answers OUT_OF_POOL_MEMORY (or FRAGMENTED_POOL, when its free
    // space is in pieces too small to use); any other failure is a real error.
    auto tryPool = [&](VkDescriptorPool pool) {
        const VkDescriptorSetAllocateInfo info {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &layout,
        };
        const VkResult result = vkAllocateDescriptorSets(m_Device, &info, &allocation.Set);
        if (result != VK_SUCCESS && result != VK_ERROR_OUT_OF_POOL_MEMORY && result != VK_ERROR_FRAGMENTED_POOL)
            VK_CHECK(result);
        allocation.Pool = pool;
        return result == VK_SUCCESS;
    };

    // Try the pools from the newest (likely to have room); sets given back may have made room in
    // older ones too. If every pool is full (or there are none yet), open a new one.
    for (VkDescriptorPool pool : m_Pools | std::views::reverse) {
        if (tryPool(pool))
            return allocation;
    }
    if (!tryPool(CreatePool()))
        VK_CHECK(VK_ERROR_OUT_OF_POOL_MEMORY); // a brand-new pool can't be full
    return allocation;
}

void DescriptorAllocator::Free(const DescriptorAllocation& allocation)
{
    VK_CHECK(vkFreeDescriptorSets(m_Device, allocation.Pool, 1, &allocation.Set));
}

VkDescriptorPool DescriptorAllocator::CreatePool()
{
    // FREE_DESCRIPTOR_SET lets sets be given back one at a time with vkFreeDescriptorSets.
    // Without it, a pool's sets can only be freed all together, by resetting or destroying it.
    const VkDescriptorPoolSize sizes[] = {
        { .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .descriptorCount = kSetsPerPool },
        { .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = kSetsPerPool },
    };
    const VkDescriptorPoolCreateInfo info {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = kSetsPerPool,
        .poolSizeCount = static_cast<uint32_t>(std::size(sizes)),
        .pPoolSizes = sizes,
    };
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorPool(m_Device, &info, nullptr, &pool));
    m_Pools.push_back(pool);
    return pool;
}

} // namespace Viva
