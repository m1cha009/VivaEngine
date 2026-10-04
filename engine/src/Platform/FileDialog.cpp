// The file dialogs declared in Viva/FileSystem.h (ShowFolderDialog, ShowOpenFileDialog,
// ShowSaveFileDialog), and DeliverFileDialogResults.

#include "Platform/FileDialog.h"

#include "Viva/FileSystem.h"
#include "Viva/Log.h"

#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>

#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace Viva {

namespace {

// One open dialog: what to call when it closes, and what SDL was given, kept until then (SDL
// reads the filter while the dialog is open).
struct DialogRequest {
    std::function<void(const std::string&)> OnChosen;
    SDL_PropertiesID Properties = 0;
    std::string FilterName;
    std::string FilterPattern;
    SDL_DialogFileFilter Filter {};
};

// A dialog that has closed, with the chosen path (empty if it was cancelled).
struct DialogResult {
    std::unique_ptr<DialogRequest> Request;
    std::string Path;
};

// SDL may report a dialog's result on a thread of its own, while the main thread runs the game.
// Both threads touch this list, so a std::mutex guards it: whoever holds the lock (std::lock_guard
// takes it, and lets go when it goes out of scope) is the only one using the list. That's C#'s
// lock statement.
std::mutex s_ResultsMutex;
std::vector<DialogResult> s_Results;

// Called by SDL, maybe on another thread, when the dialog closes. `filelist` is null on an error,
// an empty list when the user cancelled, and otherwise holds the chosen path.
void SDLCALL OnDialogClosed(void* userdata, const char* const* filelist, int /*filter*/)
{
    std::unique_ptr<DialogRequest> request(static_cast<DialogRequest*>(userdata)); // ours again
    std::string path;
    if (!filelist)
        Log::Error("The file dialog failed: {}", SDL_GetError());
    else if (filelist[0])
        path = NormalizePath(filelist[0]);

    const std::lock_guard lock(s_ResultsMutex);
    s_Results.push_back({ std::move(request), std::move(path) });
}

// Shows one kind of dialog. `extension` (without the dot) limits the files shown, unless empty.
void ShowDialog(SDL_FileDialogType type, const std::string& title, const std::string& location,
                const std::string& filterName, const std::string& extension,
                std::function<void(const std::string&)> onChosen)
{
    // The request travels through SDL as a plain pointer ("userdata"): released here, taken back
    // into a unique_ptr in OnDialogClosed.
    auto request = std::make_unique<DialogRequest>();
    request->OnChosen = std::move(onChosen);
    request->Properties = SDL_CreateProperties();
    SDL_SetStringProperty(request->Properties, SDL_PROP_FILE_DIALOG_TITLE_STRING, title.c_str());
    if (!location.empty())
        SDL_SetStringProperty(request->Properties, SDL_PROP_FILE_DIALOG_LOCATION_STRING, location.c_str());
    if (!extension.empty()) {
        // SDL's filter is a pair of C strings, which point into the request's own strings.
        request->FilterName = filterName;
        request->FilterPattern = extension;
        request->Filter = { request->FilterName.c_str(), request->FilterPattern.c_str() };
        SDL_SetPointerProperty(request->Properties, SDL_PROP_FILE_DIALOG_FILTERS_POINTER, &request->Filter);
        SDL_SetNumberProperty(request->Properties, SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER, 1);
    }

    // The dialog belongs to the engine's window (the only one), so it stays in front of it.
    int windowCount = 0;
    SDL_Window** windows = SDL_GetWindows(&windowCount);
    if (windows && windowCount > 0)
        SDL_SetPointerProperty(request->Properties, SDL_PROP_FILE_DIALOG_WINDOW_POINTER, windows[0]);
    SDL_free(windows);

    const SDL_PropertiesID properties = request->Properties;
    SDL_ShowFileDialogWithProperties(type, &OnDialogClosed, request.release(), properties);
}

} // namespace

void ShowFolderDialog(const std::string& title, const std::string& startFolder,
                      std::function<void(const std::string& folder)> onChosen)
{
    ShowDialog(SDL_FILEDIALOG_OPENFOLDER, title, startFolder, {}, {}, std::move(onChosen));
}

void ShowOpenFileDialog(const std::string& title, const std::string& startFolder, const std::string& filterName,
                        const std::string& extension, std::function<void(const std::string& path)> onChosen)
{
    ShowDialog(SDL_FILEDIALOG_OPENFILE, title, startFolder, filterName, extension, std::move(onChosen));
}

void ShowSaveFileDialog(const std::string& title, const std::string& startPath, const std::string& filterName,
                        const std::string& extension, std::function<void(const std::string& path)> onChosen)
{
    ShowDialog(SDL_FILEDIALOG_SAVEFILE, title, startPath, filterName, extension, std::move(onChosen));
}

void DeliverFileDialogResults()
{
    // Take the results out under the lock, then call back without it: a callback may open another
    // dialog, whose result would need the lock.
    std::vector<DialogResult> results;
    {
        const std::lock_guard lock(s_ResultsMutex);
        results.swap(s_Results);
    }
    for (DialogResult& result : results) {
        SDL_DestroyProperties(result.Request->Properties);
        if (!result.Path.empty())
            result.Request->OnChosen(result.Path);
    }
}

} // namespace Viva
