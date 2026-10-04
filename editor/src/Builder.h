#pragma once

#include <string>

namespace Viva {
class Project;
}

// File > Build (M18), Unity's Build Settings > Build: makes a folder that runs the project without
// the editor. It holds
//   <Project>.exe          VivaPlayer, renamed after the project
//   shaders/               the engine's compiled shaders
//   <Project>.vivaproject  the project file, which tells the player the startup scene
//   Assets/, Scenes/       the project's assets and scenes, as they are on disk (save first)
// That's the same layout as a project folder, with the player and shaders added: the player finds
// the project next to itself. Files already in `outputFolder` are replaced, others are left alone.
// Returns false (the Console says why) if something couldn't be copied.
bool BuildProject(const Viva::Project& project, const std::string& outputFolder);
