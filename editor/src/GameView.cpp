#include "GameView.h"

#include "EditorUi.h"

#include "Viva/Camera.h"
#include "Viva/Renderer.h"
#include "Viva/Scene.h"

#include <imgui.h>

using namespace Viva;

void GameView::Draw(Renderer& renderer, const Scene& scene, bool* open)
{
    // No padding: the picture fills the window to its edges.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const bool visible = ImGui::Begin("Game", open);
    ImGui::PopStyleVar();
    bool hovered = false;
    if (visible) {
        if (const Camera* camera = scene.GetMainCamera()) {
            // The camera's own lens, at this window's shape, and its background.
            const ImVec2 size = ImGui::GetContentRegionAvail();
            const RenderView view {
                .View = camera->ViewMatrix(),
                .Projection = camera->ProjectionMatrix(size.x / size.y),
                .ClearColor = camera->BackgroundColor,
            };
            hovered = DrawSceneImage(renderer, m_Target, size, view) && ImGui::IsItemHovered();
        } else {
            // (SetCursorPos counts from the window's corner, title bar included.)
            ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + 12.0f, ImGui::GetCursorPosY() + 12.0f));
            ImGui::TextDisabled("No camera: the game has nothing to show. Add a Camera (GameObject > Camera).");
        }
    }
    const bool focused = visible && ImGui::IsWindowFocused();
    ImGui::End();

    // Over the game's picture, or while its window has the focus, the mouse and keyboard belong to
    // the game: ImGui lets them go to Input (as the Scene view does for its camera).
    if (hovered || focused) {
        ImGui::SetNextFrameWantCaptureMouse(false);
        ImGui::SetNextFrameWantCaptureKeyboard(false);
    }
}
