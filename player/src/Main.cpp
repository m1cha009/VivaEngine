// VivaPlayer: runs a project made with the editor, the way a built game runs (M18). It includes only
// engine headers (Viva/...), never SDL or Vulkan.

#include "PlayerApp.h"

#include <string>
#include <string_view>
#include <utility>

int main(int argc, char* argv[])
{
    // Command-line options. ReadCommandLine takes the ones every program has (--display <n>,
    // --no-vsync, --quit-after <sec>; see Viva/Application.h). The player's own:
    //   --project <folder>  play that project, rather than the one next to the executable
    //                       (handy for running a project straight from its folder while making it)
    Viva::ApplicationSettings settings { .Title = "VivaPlayer", .Width = 1280, .Height = 720 };
    settings.ReadCommandLine(argc, argv);
    std::string projectFolder;
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--project" && i + 1 < argc)
            projectFolder = argv[++i];
    }

    PlayerApp app(std::move(settings), std::move(projectFolder));
    return app.Run();
}
