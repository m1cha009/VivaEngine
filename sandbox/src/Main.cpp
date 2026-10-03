// The sandbox: a small test program that uses the engine the way a game would.
// It includes only engine headers (Viva/...) and GLM, never SDL or Vulkan.

#include "DebugWindows.h"
#include "DemoScene.h"
#include "LaneRunnerScene.h"

#include "Viva/Application.h"
#include "Viva/Assert.h"
#include "Viva/Input.h"
#include "Viva/Log.h"
#include "Viva/Time.h"

#include <cstdint>
#include <cstdlib>
#include <string_view>
#include <utility>

namespace {

// Fine in a .cpp file. In a header it would leak into every file that includes it.
using namespace Viva;

constexpr const char* kTitle = "VivaEngine Lane Runner";
constexpr const char* kDemoTitle = "VivaEngine Sandbox";

// The game itself. It derives from Application and overrides the parts it needs, the way a
// MonoBehaviour overrides Start and Update. The scene's objects bring their own behavior as
// components (TruckController, FollowCamera, LaneRunner...); this class handles what concerns
// the whole program.
class SandboxApp : public Application {
public:
    // quitAfter: seconds until the app quits by itself, or 0 to run until Esc or the window closes.
    // demo: show the engine's demo scene (M10, M11) instead of the Lane Runner game.
    SandboxApp(ApplicationSettings settings, float quitAfter, bool demo)
        : Application(std::move(settings))
        , m_QuitAfter(quitAfter)
        , m_Demo(demo)
    {
    }

protected:
    void OnStart() override
    {
        if (m_Demo) {
            LoadDemoScene(GetScene(), GetRenderer());
            Log::Info("Fly with W/A/S/D, Q/E for down/up, Shift for speed; hold the right mouse button to look "
                      "around. F1 shows or hides the debug windows. Esc quits.");
            return;
        }

        LoadLaneRunnerScene(GetScene(), GetRenderer());
        // The game has its own HUD; the debug windows wait for F1.
        m_DebugWindows.SetVisible(false);
        Log::Info("Lane Runner: Space drives, A/D or the arrow keys change lanes. F1 shows or hides the debug "
                  "windows. Esc quits.");
    }

    void OnUpdate(float dt) override
    {
        m_DebugWindows.Draw(dt, GetRenderer(), GetScene());

        const bool timeIsUp = m_QuitAfter > 0.0f && Time::SinceStart() >= m_QuitAfter;
        if (Input::GetKeyDown(Key::Escape) || timeIsUp)
            Quit();
    }

    void OnFixedUpdate(float /*fixedDt*/) override
    {
        m_DebugWindows.CountFixedUpdate();
    }

    void OnShutdown() override
    {
        Log::Info("Shutting down after {} frames ({:.1f} s)", Time::FrameCount(), Time::SinceStart());
    }

private:
    DebugWindows m_DebugWindows;
    float m_QuitAfter = 0.0f;
    bool m_Demo = false;
};

} // namespace

int main(int argc, char* argv[])
{
    // Command-line options, mostly for testing:
    //   --test-assert        fail an assert on purpose (Debug builds), to see what that looks like
    //   --quit-after <sec>   quit by itself after that many seconds (used by CI and scripted tests)
    //   --no-vsync           draw as fast as possible instead of waiting for the display
    //   --display <n>        open the window on monitor n (0 is usually the main one)
    //   --demo               show the engine's demo scene instead of the game
    ApplicationSettings settings { .Title = kTitle };
    bool testAssert = false;
    bool demo = false;
    float quitAfter = 0.0f;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--test-assert")
            testAssert = true;
        else if (arg == "--quit-after" && i + 1 < argc)
            quitAfter = std::strtof(argv[++i], nullptr);
        else if (arg == "--no-vsync")
            settings.VSync = false;
        else if (arg == "--display" && i + 1 < argc)
            settings.Display = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
        else if (arg == "--demo")
            demo = true;
    }
    if (demo)
        settings.Title = kDemoTitle;
    VIVA_ASSERT(!testAssert, "failing on purpose because of --test-assert");

    // std::move hands the settings over instead of copying them: "settings" isn't used again.
    SandboxApp app(std::move(settings), quitAfter, demo);
    return app.Run();
}
