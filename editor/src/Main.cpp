// VivaEditor: create, open and manage projects made with the engine (M14), and later edit their
// scenes (M15 to M18). Like the Sandbox, it's a program built on the engine library; it includes
// only engine headers (Viva/...), GLM and ImGui, never SDL or Vulkan.

#include "EditorApp.h"

#include <string_view>
#include <utility>

int main(int argc, char* argv[])
{
    // Command-line options, mostly for testing. ReadCommandLine takes the ones every program has
    // (--display <n>, --no-vsync, --quit-after <sec>; see Viva/Application.h). The editor's own:
    //   --open <folder>      open that project right away
    //   --settings <folder>  keep the project list there instead of in the user's settings folder
    Viva::ApplicationSettings settings { .Title = "VivaEditor", .Width = 1280, .Height = 760 };
    settings.ReadCommandLine(argc, argv);
    EditorOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--open" && i + 1 < argc)
            options.OpenFolder = argv[++i];
        else if (arg == "--settings" && i + 1 < argc)
            options.SettingsFolder = argv[++i];
    }

    EditorApp app(std::move(settings), std::move(options));
    return app.Run();
}
