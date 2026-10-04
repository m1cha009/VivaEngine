// ShowFolderDialog (declared in Viva/FileSystem.h) and DeliverFolderDialogResults.

#include "Platform/FolderDialog.h"

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

// One open dialog: what to call when it closes, and the settings SDL was given, kept until then.
struct FolderRequest {
    std::function<void(const std::string&)> OnChosen;
    SDL_PropertiesID Properties = 0;
};

// A dialog that has closed, with the chosen folder (empty if it was cancelled).
struct FolderResult {
    std::unique_ptr<FolderRequest> Request;
    std::string Folder;
};

// SDL may report a dialog's result on a thread of its own, while the main thread runs the game.
// Both threads touch this list, so a std::mutex guards it: whoever holds the lock (std::lock_guard
// takes it, and lets go when it goes out of scope) is the only one using the list. That's C#'s
// lock statement.
std::mutex s_ResultsMutex;
std::vector<FolderResult> s_Results;

// Called by SDL, maybe on another thread, when the dialog closes. `filelist` is null on an error,
// an empty list when the user cancelled, and otherwise holds the chosen folder.
void SDLCALL OnDialogClosed(void* userdata, const char* const* filelist, int /*filter*/)
{
    std::unique_ptr<FolderRequest> request(static_cast<FolderRequest*>(userdata)); // ours again
    std::string folder;
    if (!filelist)
        Log::Error("The folder dialog failed: {}", SDL_GetError());
    else if (filelist[0])
        folder = NormalizePath(filelist[0]);

    const std::lock_guard lock(s_ResultsMutex);
    s_Results.push_back({ std::move(request), std::move(folder) });
}

} // namespace

void ShowFolderDialog(const std::string& title, const std::string& startFolder,
                      std::function<void(const std::string& folder)> onChosen)
{
    // The request travels through SDL as a plain pointer ("userdata"): released here, taken back
    // into a unique_ptr in OnDialogClosed.
    auto request = std::make_unique<FolderRequest>();
    request->OnChosen = std::move(onChosen);
    request->Properties = SDL_CreateProperties();
    SDL_SetStringProperty(request->Properties, SDL_PROP_FILE_DIALOG_TITLE_STRING, title.c_str());
    if (!startFolder.empty())
        SDL_SetStringProperty(request->Properties, SDL_PROP_FILE_DIALOG_LOCATION_STRING, startFolder.c_str());

    // The dialog belongs to the engine's window (the only one), so it stays in front of it.
    int windowCount = 0;
    SDL_Window** windows = SDL_GetWindows(&windowCount);
    if (windows && windowCount > 0)
        SDL_SetPointerProperty(request->Properties, SDL_PROP_FILE_DIALOG_WINDOW_POINTER, windows[0]);
    SDL_free(windows);

    const SDL_PropertiesID properties = request->Properties;
    SDL_ShowFileDialogWithProperties(SDL_FILEDIALOG_OPENFOLDER, &OnDialogClosed, request.release(), properties);
}

void DeliverFolderDialogResults()
{
    // Take the results out under the lock, then call back without it: a callback may open another
    // dialog, whose result would need the lock.
    std::vector<FolderResult> results;
    {
        const std::lock_guard lock(s_ResultsMutex);
        results.swap(s_Results);
    }
    for (FolderResult& result : results) {
        SDL_DestroyProperties(result.Request->Properties);
        if (!result.Folder.empty())
            result.Request->OnChosen(result.Folder);
    }
}

} // namespace Viva
