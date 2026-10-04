#include "Renderer/VulkanHelpers.h"

#include "Renderer/VulkanCheck.h"

namespace Viva {

VkCommandPool CreateCommandPool(VkDevice device, uint32_t queueFamily)
{
    const VkCommandPoolCreateInfo info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = queueFamily, // its command buffers are submitted to this family's queues
    };
    VkCommandPool pool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateCommandPool(device, &info, nullptr, &pool));
    return pool;
}

VkCommandBuffer AllocateCommandBuffer(VkDevice device, VkCommandPool pool)
{
    const VkCommandBufferAllocateInfo info {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, // submitted directly to a queue
        .commandBufferCount = 1,
    };
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateCommandBuffers(device, &info, &cmd));
    return cmd;
}

VkFence CreateFence(VkDevice device, bool signaled)
{
    const VkFenceCreateInfo info {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : VkFenceCreateFlags { 0 },
    };
    VkFence fence = VK_NULL_HANDLE;
    VK_CHECK(vkCreateFence(device, &info, nullptr, &fence));
    return fence;
}

VkSemaphore CreateBinarySemaphore(VkDevice device)
{
    const VkSemaphoreCreateInfo info { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkSemaphore semaphore = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSemaphore(device, &info, nullptr, &semaphore));
    return semaphore;
}

VkImageView CreateImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspect)
{
    const VkImageViewCreateInfo info {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange = {
            .aspectMask = aspect,
            .levelCount = VK_REMAINING_MIP_LEVELS, // every mip level, so sampling can pick any
            .layerCount = VK_REMAINING_ARRAY_LAYERS,
        },
    };
    VkImageView view = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImageView(device, &info, nullptr, &view));
    return view;
}

VkDescriptorSetLayout CreateDescriptorSetLayout(VkDevice device, std::span<const VkDescriptorSetLayoutBinding> bindings)
{
    const VkDescriptorSetLayoutCreateInfo info {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data(),
    };
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &layout));
    return layout;
}

void WriteDynamicUniformBufferDescriptor(VkDevice device, VkDescriptorSet set, uint32_t binding, VkBuffer buffer,
                                         VkDeviceSize range)
{
    const VkDescriptorBufferInfo bufferInfo { .buffer = buffer, .offset = 0, .range = range };
    const VkWriteDescriptorSet write {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = set,
        .dstBinding = binding,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        .pBufferInfo = &bufferInfo,
    };
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

void WriteImageDescriptor(VkDevice device, VkDescriptorSet set, uint32_t binding, VkImageView view, VkSampler sampler,
                          VkImageLayout layout)
{
    // A "combined image sampler": the view and the sampler travel in one descriptor.
    const VkDescriptorImageInfo imageInfo { .sampler = sampler, .imageView = view, .imageLayout = layout };
    const VkWriteDescriptorSet write {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = set,
        .dstBinding = binding,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &imageInfo,
    };
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

void TransitionImage(VkCommandBuffer cmd, const ImageTransition& transition)
{
    const VkImageMemoryBarrier2 barrier {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = transition.SrcStage,
        .srcAccessMask = transition.SrcAccess,
        .dstStageMask = transition.DstStage,
        .dstAccessMask = transition.DstAccess,
        .oldLayout = transition.OldLayout,
        .newLayout = transition.NewLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, // not moving between queue families
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = transition.Image,
        .subresourceRange = {
            .aspectMask = transition.Aspect,
            .baseMipLevel = transition.BaseMipLevel,
            .levelCount = transition.MipLevelCount,
            .layerCount = VK_REMAINING_ARRAY_LAYERS, // every layer, from 0
        },
    };
    const VkDependencyInfo dependency {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };
    vkCmdPipelineBarrier2(cmd, &dependency);
}

} // namespace Viva
