#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Viva {

// An image decoded into memory: 4 bytes per pixel (red, green, blue, alpha), row after row from
// the top-left corner.
struct ImageData {
    uint32_t Width = 0;
    uint32_t Height = 0;
    std::vector<uint8_t> Pixels;
};

// Decodes a PNG or JPEG that's already in memory: a file's contents, or an image stored inside a
// model file (Model.cpp). `name` only appears in error messages. Returns std::nullopt (after
// logging why) if the data can't be decoded.
std::optional<ImageData> DecodeImage(std::span<const uint8_t> encoded, const std::string& name);

// Loads and decodes a PNG or JPEG file. The path is UTF-8. Returns std::nullopt (after logging
// why) if the file can't be read or decoded.
std::optional<ImageData> LoadImageFile(const std::string& path);

} // namespace Viva
