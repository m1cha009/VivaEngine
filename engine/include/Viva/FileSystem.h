#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Files and folders, for the engine and for tools like the editor: Unity's System.IO and
// EditorUtility.OpenFolderPanel. Every path is UTF-8 text, on every OS, and "/" works as a separator
// everywhere (Windows accepts it too). The functions that can fail log why and return false,
// std::nullopt or an empty result: no exceptions.
//
// They're built on SDL's file functions rather than std::filesystem, whose functions throw
// exceptions or need an error_code at every call, and whose path::string() can throw on Windows
// for names that aren't plain ASCII.
namespace Viva {

// The folder the executable lives in, ending with a separator. The engine finds its compiled
// shaders and the game's assets relative to it, so it works no matter which folder the game is
// started from.
std::string GetExecutableDirectory();

// A folder where a program can keep its settings for the current user, created if it doesn't exist
// yet: %APPDATA%\<organization>\<application>\ on Windows. Ends with a separator. Empty if the OS
// doesn't provide one.
std::string GetUserDataFolder(const std::string& organization, const std::string& application);

// The user's Documents folder, ending with a separator, or empty if the OS doesn't have one.
std::string GetDocumentsFolder();

// The same path with "/" as its only separator and no separator at the end, so two ways of writing
// one folder ("C:\Games\X\" and "C:/Games/X") compare equal as text.
std::string NormalizePath(std::string_view path);

// Reads a whole file into memory. Returns std::nullopt (after logging why) if it can't.
std::optional<std::vector<uint8_t>> ReadBinaryFile(const std::string& path);
// The same, as text.
std::optional<std::string> ReadTextFile(const std::string& path);

// Writes `text` into a file, replacing it if it exists. Returns false (after logging why) if it
// can't.
bool WriteTextFile(const std::string& path, std::string_view text);

// Whether something exists at `path`, and whether it's a folder. Neither logs anything.
bool PathExists(const std::string& path);
bool IsFolder(const std::string& path);

// Creates a folder, and any missing folders above it. Succeeds if it exists already.
bool CreateFolder(const std::string& path);

// The names of what's directly inside a folder (files and folders, without their paths), or an
// empty list if it can't be read.
std::vector<std::string> ListFolder(const std::string& path);

// Deletes a folder and everything in it, for good: it doesn't go to the Recycle Bin. Returns false
// (after logging why) if something couldn't be deleted.
bool DeleteFolder(const std::string& path);

// Opens the OS's "choose a folder" window over the application's window, starting in `startFolder`
// (may be empty), with `title` in its title bar. It doesn't wait: the game keeps running while
// it's open. When the user picks a folder, `onChosen` is called with it, on the main thread, while
// Application::Run handles a later frame's events. Nothing is called if they cancel. Needs a
// running Application (its window, and its event handling).
void ShowFolderDialog(const std::string& title, const std::string& startFolder,
                      std::function<void(const std::string& folder)> onChosen);

} // namespace Viva
