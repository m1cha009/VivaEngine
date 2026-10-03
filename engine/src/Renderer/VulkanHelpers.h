#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <span>

namespace Viva {

// Small building blocks that several renderer classes share. Each one wraps a single Vulkan call
// and its "create info" boilerplate. The caller owns (and destroys) whatever they create.

// A command pool for the given queue family. TRANSIENT: its command buffers are short-lived
// (re-recorded every frame, or used once).
VkCommandPool CreateCommandPool(VkDevice device, uint32_t queueFamily);
// One primary command buffer from `pool`. It's freed together with the pool.
VkCommandBuffer AllocateCommandBuffer(VkDevice device, VkCommandPool pool);
// signaled: create it already signaled, so the first wait on it returns immediately.
VkFence CreateFence(VkDevice device, bool signaled);
VkSemaphore CreateBinarySemaphore(VkDevice device);
// A view of a whole 2D image: all its mip levels, through `aspect` (color or depth).
VkImageView CreateImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspect);

// Descriptor set layouts and sets (see FrameUniforms.h for what they are; DescriptorAllocator
// hands out the sets).
VkDescriptorSetLayout CreateDescriptorSetLayout(VkDevice device, std::span<const VkDescriptorSetLayoutBinding> bindings);
// Points one binding of `set` at a resource: a uniform buffer (its first `range` bytes), or an
// image view with its sampler, which the image must be in `layout` for whenever it's sampled.
void WriteUniformBufferDescriptor(VkDevice device, VkDescriptorSet set, uint32_t binding, VkBuffer buffer,
                                  VkDeviceSize range);
void WriteImageDescriptor(VkDevice device, VkDescriptorSet set, uint32_t binding, VkImageView view, VkSampler sampler,
                          VkImageLayout layout);

// An image layout transition plus the synchronization around it. Fill it with designated
// initializers, naming each field:
//   TransitionImage(cmd, { .Image = image, .OldLayout = ..., .NewLayout = ..., .SrcStage = ... });
struct ImageTransition {
    VkImage Image = VK_NULL_HANDLE;
    // Which part of the image: its color, or its depth for a depth buffer.
    VkImageAspectFlags Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    // Which mip levels: by default all of them. Generating mipmaps moves one level at a time.
    uint32_t BaseMipLevel = 0;
    uint32_t MipLevelCount = VK_REMAINING_MIP_LEVELS;
    VkImageLayout OldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout NewLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // Work in SrcStage (and its memory writes, SrcAccess) must finish, and be visible, before work
    // in DstStage (doing DstAccess) starts.
    VkPipelineStageFlags2 SrcStage = VK_PIPELINE_STAGE_2_NONE;
    VkAccessFlags2 SrcAccess = VK_ACCESS_2_NONE;
    VkPipelineStageFlags2 DstStage = VK_PIPELINE_STAGE_2_NONE;
    VkAccessFlags2 DstAccess = VK_ACCESS_2_NONE;
};

// Records an image memory barrier, synchronization2 style. It does two jobs:
//  - Ordering: the GPU runs commands in parallel and out of order unless barriers like this one
//    say otherwise.
//  - Layout: GPUs store images differently for different uses (being drawn into, being shown on
//    screen, being sampled as a texture), and Vulkan makes us say when an image changes role.
void TransitionImage(VkCommandBuffer cmd, const ImageTransition& transition);

} // namespace Viva
