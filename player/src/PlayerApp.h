#pragma once

#include "Viva/Application.h"

#include <string>

// A built game: opens a project and plays its startup scene in the window, through the scene's
// main camera, with every component running. There's no game code here at all; everything the
// game does comes from the scene file and the engine's components (game code in projects comes
// after M18).
class PlayerApp : public Viva::Application {
public:
    // projectFolder: where the project is, or "" for the folder the player is in (a built game).
    PlayerApp(Viva::ApplicationSettings settings, std::string projectFolder);

protected:
    void OnStart() override;

private:
    std::string m_ProjectFolder;
};
