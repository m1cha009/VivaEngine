#include "Viva/FileSystem.h"

#include "Viva/Log.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_stdinc.h>

#include <algorithm>
#include <cctype>

namespace Viva {

std::string GetExecutableDirectory()
{
    // SDL knows how to ask each OS where the running program is (on macOS, inside an app bundle
    // it's the bundle's Resources folder). It answers in UTF-8, with a separator at the end.
    const char* basePath = SDL_GetBasePath();
    return basePath ? basePath : "";
}

std::string GetUserDataFolder(const std::string& organization, const std::string& application)
{
    // SDL_GetPrefPath creates the folder if needed. The string is ours to free.
    char* path = SDL_GetPrefPath(organization.c_str(), application.c_str());
    if (!path) {
        Log::Error("There's no folder for the user's settings: {}", SDL_GetError());
        return {};
    }
    std::string result = path;
    SDL_free(path);
    return result;
}

std::string GetDocumentsFolder()
{
    // This string belongs to SDL: no SDL_free.
    const char* path = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
    return path ? path : "";
}

std::string NormalizePath(std::string_view path)
{
    std::string result(path);
    std::replace(result.begin(), result.end(), '\\', '/');
    // A drive's root keeps its slash: "C:/", not "C:".
    while (result.size() > 1 && result.back() == '/' && !(result.size() == 3 && result[1] == ':'))
        result.pop_back();
    return result;
}

std::string GetFileStem(std::string_view path)
{
    const std::string normalized = NormalizePath(path);
    const size_t slash = normalized.rfind('/');
    const std::string name = slash == std::string::npos ? normalized : normalized.substr(slash + 1);
    return name.substr(0, name.rfind('.'));
}

std::string GetFileExtension(std::string_view path)
{
    const std::string normalized = NormalizePath(path);
    const size_t dot = normalized.rfind('.');
    if (dot == std::string::npos || normalized.find('/', dot) != std::string::npos)
        return {}; // no dot, or only in a folder's name
    std::string extension = normalized.substr(dot + 1);
    for (char& c : extension)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return extension;
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

std::optional<std::string> ReadTextFile(const std::string& path)
{
    const std::optional<std::vector<uint8_t>> bytes = ReadBinaryFile(path);
    if (!bytes)
        return std::nullopt;
    // The bytes are UTF-8 text, which is what std::string holds too.
    return std::string(bytes->begin(), bytes->end());
}

bool WriteTextFile(const std::string& path, std::string_view text)
{
    // SDL_SaveFile is SDL_LoadFile's opposite: the whole file in one go, from a UTF-8 path.
    if (!SDL_SaveFile(path.c_str(), text.data(), text.size())) {
        Log::Error("Couldn't write {}: {}", path, SDL_GetError());
        return false;
    }
    return true;
}

bool PathExists(const std::string& path)
{
    return SDL_GetPathInfo(path.c_str(), nullptr);
}

bool IsFolder(const std::string& path)
{
    SDL_PathInfo info;
    return SDL_GetPathInfo(path.c_str(), &info) && info.type == SDL_PATHTYPE_DIRECTORY;
}

bool CreateFolder(const std::string& path)
{
    // Creates the missing folders above it too, and succeeds if it exists already.
    if (!SDL_CreateDirectory(path.c_str())) {
        Log::Error("Couldn't create the folder {}: {}", path, SDL_GetError());
        return false;
    }
    return true;
}

std::vector<std::string> ListFolder(const std::string& path)
{
    // SDL calls the function below once per entry. It's a C callback: a plain function (a lambda
    // that captures nothing converts to one), with the list passed through the void* "userdata"
    // pointer, the way C APIs hand state to callbacks.
    std::vector<std::string> names;
    const auto addName = [](void* userdata, const char* /*folder*/, const char* name) {
        static_cast<std::vector<std::string>*>(userdata)->push_back(name);
        return SDL_ENUM_CONTINUE;
    };
    if (!SDL_EnumerateDirectory(path.c_str(), addName, &names))
        return {};
    return names;
}

bool DeleteFolder(const std::string& path)
{
    // SDL only removes empty folders, so the contents go first: files directly, folders by this
    // same function (recursion, as in Model::Instantiate). Symbolic links are followed by
    // SDL_GetPathInfo, so a link to a folder would empty the folder it points to; projects don't
    // have any.
    bool deleted = true;
    for (const std::string& name : ListFolder(path)) {
        const std::string entry = path + "/" + name;
        if (IsFolder(entry)) {
            deleted = DeleteFolder(entry) && deleted;
        } else if (!SDL_RemovePath(entry.c_str())) {
            Log::Error("Couldn't delete {}: {}", entry, SDL_GetError());
            deleted = false;
        }
    }
    if (!SDL_RemovePath(path.c_str())) {
        Log::Error("Couldn't delete the folder {}: {}", path, SDL_GetError());
        return false;
    }
    return deleted;
}

} // namespace Viva
