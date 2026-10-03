#include "Renderer/Texture.h"

#include "Renderer/Buffer.h"
#include "Renderer/VulkanCheck.h"
#include "Renderer/VulkanContext.h"
#include "Renderer/VulkanHelpers.h"

#include <algorithm>
#include <bit>

namespace Viva {

namespace {

// Textures hold colors as the image file stores them: sRGB, where the 256 steps are spread to
// suit the eye rather than evenly. An _SRGB format makes the GPU convert each texel to linear
// light when a shader samples it, so lighting math (and blending between mip levels) happens on
// linear values. Data that isn't a color, like a normal map, would use _UNORM instead.
// Generating mipmaps blits this format with linear filtering. Vulkan requires every GPU to
// support that for R8G8B8A8 (_SRGB and _UNORM), so there's nothing to check. Compressed formats
// (BC7 and friends) can't be blitted at all; their mipmaps are made ahead of time.
constexpr VkFormat kTextureFormat = VK_FORMAT_R8G8B8A8_SRGB;

// Records the upload of mip level 0 from `staging`, then fills every smaller level by shrinking
// the one above it to half its size with vkCmdBlitImage (a filtered copy, done by the GPU).
// Afterwards all levels are ready for shaders to sample.
//
// Each level's layout moves along as it's used:
//   TRANSFER_DST (written) -> TRANSFER_SRC (read to make the next level) -> SHADER_READ_ONLY
// and the barriers in between make each blit wait for the write of the level it reads.
void RecordUploadAndMipmaps(VkCommandBuffer cmd, VkBuffer staging, VkImage image, uint32_t width, uint32_t height,
                            uint32_t mipLevels)
{
    // Every level becomes a copy destination first. Its old contents don't matter (UNDEFINED).
    TransitionImage(cmd, {
        .Image = image,
        .OldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .NewLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .SrcStage = VK_PIPELINE_STAGE_2_NONE,
        .SrcAccess = VK_ACCESS_2_NONE,
        .DstStage = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
        .DstAccess = VK_ACCESS_2_TRANSFER_WRITE_BIT,
    });

    // Level 0: the pixels, copied in from the staging buffer (tightly packed rows).
    const VkBufferImageCopy region {
        .imageSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .layerCount = 1 },
        .imageExtent = { width, height, 1 },
    };
    vkCmdCopyBufferToImage(cmd, staging, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Blit offsets are signed.
    auto levelWidth = static_cast<int32_t>(width);
    auto levelHeight = static_cast<int32_t>(height);
    for (uint32_t level = 0; level < mipLevels; ++level) {
        // This level is complete (copied in, or blitted from the one above): make it a copy source,
        // so the next level can be made from it.
        TransitionImage(cmd, {
            .Image = image,
            .BaseMipLevel = level,
            .MipLevelCount = 1,
            .OldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .NewLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .SrcStage = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
            .SrcAccess = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .DstStage = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
            .DstAccess = VK_ACCESS_2_TRANSFER_READ_BIT,
        });
        if (level + 1 == mipLevels)
            break;

        // Shrink it into the next level: half the size (but at least 1 pixel), with linear
        // filtering, so each new pixel averages the 2x2 pixels it covers.
        const int32_t halfWidth = std::max(levelWidth / 2, 1);
        const int32_t halfHeight = std::max(levelHeight / 2, 1);
        const VkImageBlit blit {
            .srcSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = level, .layerCount = 1 },
            .srcOffsets = { { 0, 0, 0 }, { levelWidth, levelHeight, 1 } },
            .dstSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = level + 1, .layerCount = 1 },
            .dstOffsets = { { 0, 0, 0 }, { halfWidth, halfHeight, 1 } },
        };
        vkCmdBlitImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &blit, VK_FILTER_LINEAR);
        levelWidth = halfWidth;
        levelHeight = halfHeight;
    }

    // Every level is a copy source now. One barrier makes all of them ready for shaders.
    TransitionImage(cmd, {
        .Image = image,
        .OldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .NewLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .SrcStage = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
        .SrcAccess = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .DstStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        .DstAccess = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
    });
}

} // namespace

std::unique_ptr<Texture> Texture::Create(const VulkanContext& context, uint32_t width, uint32_t height,
                                         std::span<const uint8_t> pixels)
{
    auto texture = std::make_unique<Texture>(context.GetDevice());

    // Mip levels: the image, then copies of half the size, down to 1x1. A 256x256 texture has 9
    // (256, 128, ..., 2, 1). std::bit_width(256) is 9: the number of bits needed to write 256.
    const auto mipLevels = static_cast<uint32_t>(std::bit_width(std::max(width, height)));

    // TRANSFER_DST: filled by copies and blits. TRANSFER_SRC: each level is the source for the
    // next. SAMPLED: read by shaders.
    texture->m_Image = Image::Create(context, {
        .Extent = { width, height },
        .Format = kTextureFormat,
        .Usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .MipLevels = mipLevels,
    });

    // The pixels travel like a buffer's data in M5: into a staging buffer, then copied by the GPU.
    std::unique_ptr<Buffer> staging = Buffer::Create(context, pixels.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                     MemoryLocation::CpuToGpu);
    staging->Write(std::as_bytes(pixels));
    context.ImmediateSubmit([&](VkCommandBuffer cmd) {
        RecordUploadAndMipmaps(cmd, staging->GetHandle(), texture->m_Image->GetHandle(), width, height, mipLevels);
    });

    // The sampler: how shaders read the texture.
    //   LINEAR filtering blends the 4 nearest texels (smooth up close, instead of blocky).
    //   LINEAR mipmap mode also blends between the two nearest mip levels, so there's no visible
    //   line where one level hands over to the next ("trilinear filtering").
    //   REPEAT: texture coordinates past 1 wrap around, so a texture can tile across a surface.
    //   Anisotropic filtering takes extra samples along surfaces seen at a slant, like a floor,
    //   which keeps them sharp into the distance (Unity's "Aniso Level").
    //   maxLod VK_LOD_CLAMP_NONE: every mip level may be used.
    const float anisotropy = context.GetMaxSamplerAnisotropy();
    const VkSamplerCreateInfo samplerInfo {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .anisotropyEnable = anisotropy > 0.0f ? VK_TRUE : VK_FALSE,
        .maxAnisotropy = anisotropy,
        .maxLod = VK_LOD_CLAMP_NONE,
    };
    VK_CHECK(vkCreateSampler(texture->m_Device, &samplerInfo, nullptr, &texture->m_Sampler));
    return texture;
}

Texture::Texture(VkDevice device)
    : m_Device(device)
{
}

Texture::~Texture()
{
    vkDestroySampler(m_Device, m_Sampler, nullptr);
}

} // namespace Viva
