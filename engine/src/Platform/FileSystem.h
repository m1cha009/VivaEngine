#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Viva {

// The folder the executable lives in, as UTF-8 text ending with a path separator. The engine finds
// its files (compiled shaders, and later assets) relative to it, so it works no matter which
// folder the game is started from.
std::string GetExecutableDirectory();

// Reads a whole file into memory. The path is UTF-8. Returns std::nullopt (after logging why) if
// it can't.
std::optional<std::vector<uint8_t>> ReadBinaryFile(const std::string& path);

} // namespace Viva
