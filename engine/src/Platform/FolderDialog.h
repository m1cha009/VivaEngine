#pragma once

namespace Viva {

// The engine's side of ShowFolderDialog (Viva/FileSystem.h): calls the callbacks of the folder
// dialogs that have closed since the last call. The OS may report a choice on another thread;
// Window::PollEvents calls this on the main thread, so games never see that thread.
void DeliverFolderDialogResults();

} // namespace Viva
