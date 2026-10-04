#pragma once

namespace Viva {

// The engine's side of the file dialogs (ShowFolderDialog and friends, in Viva/FileSystem.h):
// calls the callbacks of the dialogs that have closed since the last call. The OS may report a
// choice on another thread; Window::PollEvents calls this on the main thread, so games never see
// that thread.
void DeliverFileDialogResults();

} // namespace Viva
