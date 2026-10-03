#include "Platform/FileSystem.h"

#include "Viva/Log.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_stdinc.h>

namespace Viva {

std::string GetExecutableDirectory()
{
    // SDL knows how to ask each OS where the running program is (on macOS, inside an app bundle
    // it's the bundle's Resources folder). It answers in UTF-8, with a separator at the end.
    const char* basePath = SDL_GetBasePath();
    return basePath ? basePath : "";
}

std::string GetAssetPath(const std::string& relativePath)
{
    // "/" works as a separator on Windows too.
    return GetExecutableDirectory() + "assets/" + relativePath;
}

std::optional<std::vector<uint8_t>> ReadBinaryFile(const std::string& path)
{
    // SDL_LoadFile reads the whole file into memory that SDL allocates. It takes a UTF-8 path on
    // every OS (on Windows it converts it for the system), and on failure SDL_GetError() says why.
    size_t size = 0;
    void* data = SDL_LoadFile(path.c_str(), &size);
    if (!data) {
        Log::Error("Couldn't read {}: {}", path, SDL_GetError());
        return std::nullopt;
    }

    // Copy into a vector we own, then hand SDL's memory back to it.
    const auto* bytes = static_cast<const uint8_t*>(data);
    std::vector<uint8_t> result(bytes, bytes + size);
    SDL_free(data);
    return result;
}

} // namespace Viva
