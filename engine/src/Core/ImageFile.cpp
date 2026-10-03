#include "Core/ImageFile.h"

#include "Platform/FileSystem.h"
#include "Viva/Assert.h"
#include "Viva/Log.h"

// stb_image is a single header, like VMA: this file defines STB_IMAGE_IMPLEMENTATION so its code
// is compiled here, once. The other settings:
//   STBI_ONLY_PNG/JPEG  leave out the decoders for formats we don't use (GIF, BMP, PSD...).
//   STBI_NO_STDIO       no functions that open files themselves: we read the file with
//                       ReadBinaryFile (UTF-8 paths on every OS) and decode from memory.
//   STBI_ASSERT         report stb's internal checks like our own asserts.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_ASSERT(x) VIVA_ASSERT(x)
#include <stb_image.h>

namespace Viva {

std::optional<ImageData> DecodeImage(std::span<const uint8_t> encoded, const std::string& name)
{
    // STBI_rgb_alpha: always decode to 4 channels, whatever the file holds. GPUs work with RGBA,
    // and 3-channel formats are rarely supported for textures.
    int width = 0;
    int height = 0;
    int channelsInFile = 0;
    stbi_uc* pixels = stbi_load_from_memory(encoded.data(), static_cast<int>(encoded.size()), &width, &height,
                                            &channelsInFile, STBI_rgb_alpha);
    if (!pixels) {
        Log::Error("Couldn't decode {}: {}", name, stbi_failure_reason());
        return std::nullopt;
    }

    // Copy into memory we own, then hand stb's buffer back.
    const size_t byteCount = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    ImageData image {
        .Width = static_cast<uint32_t>(width),
        .Height = static_cast<uint32_t>(height),
        .Pixels = std::vector<uint8_t>(pixels, pixels + byteCount),
    };
    stbi_image_free(pixels);
    return image;
}

std::optional<ImageData> LoadImageFile(const std::string& path)
{
    const std::optional<std::vector<uint8_t>> file = ReadBinaryFile(path);
    if (!file)
        return std::nullopt;
    return DecodeImage(*file, path);
}

} // namespace Viva
