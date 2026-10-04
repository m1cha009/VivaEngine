#include "LaneRunner.h"

#include "Road.h"
#include "TruckController.h"

#include "Viva/Assert.h"
#include "Viva/GameObject.h"
#include "Viva/Input.h"
#include "Viva/MeshRenderer.h"
#include "Viva/Scene.h"
#include "Viva/Spinner.h"
#include "Viva/Transform.h"

#include <glm/vec2.hpp>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <format>
#include <numeric>
#include <string_view>
#include <utility>

using namespace Viva;

namespace {

// The road ahead: a row every kRowSpacing meters from kFirstRow on, placed once the truck is
// within kSpawnDistance of it. That far away a crate is a few pixels tall, so its appearing isn't
// noticed. Items more than kRemoveDistance behind the truck are out of the camera's view, and go.
constexpr float kFirstRow = 60.0f;
constexpr float kRowSpacing = 28.0f;
constexpr float kSpawnDistance = 160.0f;
constexpr float kRemoveDistance = 15.0f;

constexpr float kCrateSize = 1.6f;
constexpr float kPickupSize = 0.9f;
constexpr float kPickupHeight = 1.2f;  // floating above the road
constexpr double kPickupChance = 0.35; // for each open lane in a row
constexpr int kPickupPoints = 50;

// Collision boxes, as half sizes across (x) and along (z) the road. The truck's and the crates'
// are a little smaller than the models (the truck is 2.8 m wide and 4.9 m long), so a near miss
// stays a miss. The logo boxes' is a little larger, so a near grab still counts.
constexpr glm::vec2 kTruckHalfSize { 1.25f, 2.3f };
constexpr glm::vec2 kCrateHalfSize = glm::vec2(kCrateSize / 2.0f - 0.05f);
constexpr glm::vec2 kPickupHalfSize = glm::vec2(kPickupSize / 2.0f + 0.15f);

// Whether two boxes on the road overlap, seen from above: only if they overlap both across and
// along the road. Boxes lined up with the axes like these ("axis-aligned bounding boxes") make the
// simplest collision test there is, and it's enough for a truck that barely turns.
bool Overlaps(const glm::vec3& a, const glm::vec2& halfA, const glm::vec3& b, const glm::vec2& halfB)
{
    return std::abs(a.x - b.x) < halfA.x + halfB.x && std::abs(a.z - b.z) < halfA.y + halfB.y;
}

// HUD text: white, with a dark shadow so it reads on the road and on the sky. `align` says which
// part of the text goes at `position`: 0 its start, 0.5 its middle, 1 its end. A string_view only
// points at characters stored elsewhere, so passing a literal copies nothing.
void HudText(ImDrawList& drawList, ImFont* font, float size, ImVec2 position, std::string_view text,
             float align = 0.0f)
{
    const char* begin = text.data();
    const char* end = begin + text.size();
    const ImVec2 textSize = font->CalcTextSizeA(size, FLT_MAX, 0.0f, begin, end);
    position.x -= textSize.x * align;
    const float shadow = size / 20.0f;
    drawList.AddText(font, size, ImVec2(position.x + shadow, position.y + shadow), IM_COL32(0, 0, 0, 160), begin, end);
    drawList.AddText(font, size, position, IM_COL32(255, 255, 255, 255), begin, end);
}

} // namespace

LaneRunner::LaneRunner(TruckController& truck, std::shared_ptr<Mesh> crateMesh, std::shared_ptr<Material> crateMaterial,
                       std::shared_ptr<Model> pickupModel, ImFont* hudFont)
    : m_Truck(truck)
    , m_CrateMesh(std::move(crateMesh))
    , m_CrateMaterial(std::move(crateMaterial))
    , m_PickupModel(std::move(pickupModel))
    , m_HudFont(hudFont)
{
    VIVA_ASSERT(m_HudFont, "LaneRunner needs a font for its HUD");
}

void LaneRunner::OnStart()
{
    // The road ahead is laid out while the game waits for the first start.
    ResetRun();
}

void LaneRunner::OnUpdate(float /*dt*/)
{
    // The game is a small "state machine": Ready, Driving or Crashed. Space sets off from either
    // of the other two, after clearing the road if a run just ended.
    if (m_State != State::Driving && Input::GetKeyDown(Key::Space)) {
        if (m_State == State::Crashed)
            ResetRun();
        m_Truck.Drive();
        m_State = State::Driving;
    }
}

void LaneRunner::OnLateUpdate(float /*dt*/)
{
    // In OnLateUpdate, the truck has moved (or been put back at the start) for this frame.
    if (m_State == State::Driving) {
        SpawnRowsAhead();
        UpdateItems();
    }
    DrawHud();
}

void LaneRunner::ResetRun()
{
    // Clear the road. Destroy only marks the objects; they're removed after the frame's updates
    // (M10). One may already be gone, destroyed with the debug windows: then it must not be touched.
    Scene& scene = GetGameObject().GetScene();
    for (const RoadItem& item : m_Items) {
        if (scene.Contains(item.Object))
            scene.Destroy(*item.Object);
    }
    m_Items.clear();
    m_Pickups = 0;

    m_Truck.Reset();
    m_NextRowZ = kFirstRow;
    SpawnRowsAhead();
}

void LaneRunner::Crash()
{
    m_Truck.Stop();
    m_State = State::Crashed;
    m_BestScore = std::max(m_BestScore, Score());
}

void LaneRunner::SpawnRowsAhead()
{
    const float truckZ = m_Truck.GetTransform().GetPosition().z;
    while (m_NextRowZ < truckZ + kSpawnDistance) {
        SpawnRow(m_NextRowZ);
        m_NextRowZ += kRowSpacing;
    }
}

void LaneRunner::SpawnRow(float z)
{
    Scene& scene = GetGameObject().GetScene();

    // Shuffle the lanes, then block the first one, or one time in three the first two. At least
    // one lane always stays open.
    std::array<int, Road::kLaneCount> lanes;
    std::iota(lanes.begin(), lanes.end(), 0); // 0, 1, 2...
    std::ranges::shuffle(lanes, m_Random);
    const int blocked = std::min(std::uniform_int_distribution(1, 3)(m_Random) == 1 ? 2 : 1, Road::kLaneCount - 1);

    for (int i = 0; i < Road::kLaneCount; ++i) {
        const glm::vec3 lane(Road::LaneX(lanes[i]), 0.0f, z);
        if (i < blocked) {
            // Every crate shares one mesh and one material: creating them would wait for the GPU
            // (M8), while a GameObject with a MeshRenderer is quick to make.
            GameObject& crate = scene.CreateGameObject("Crate");
            crate.GetTransform().LocalPosition = lane + glm::vec3(0.0f, kCrateSize / 2.0f, 0.0f);
            crate.GetTransform().LocalScale = glm::vec3(kCrateSize);
            crate.AddComponent<MeshRenderer>(m_CrateMesh, m_CrateMaterial);
            m_Items.push_back({ &crate, false });
        } else if (m_PickupModel && std::bernoulli_distribution(kPickupChance)(m_Random)) {
            // An instance of the logo box model: no GPU work either, it shares the model's (M11).
            GameObject& pickup = m_PickupModel->Instantiate(scene);
            pickup.GetTransform().LocalPosition = lane + glm::vec3(0.0f, kPickupHeight, 0.0f);
            pickup.GetTransform().LocalScale = glm::vec3(kPickupSize);
            pickup.AddComponent<Spinner>(glm::vec3(0.0f, 1.0f, 0.0f), 120.0f);
            m_Items.push_back({ &pickup, true });
        }
    }
}

void LaneRunner::UpdateItems()
{
    Scene& scene = GetGameObject().GetScene();
    const glm::vec3 truck = m_Truck.GetTransform().GetPosition();

    // std::erase_if (C++20) is C#'s List.RemoveAll: it removes the items for which the lambda
    // returns true. [&] lets the lambda use the variables around it (by reference).
    std::erase_if(m_Items, [&](const RoadItem& item) {
        // Destroyed with the debug windows: the scene has freed it, so just forget it.
        if (!scene.Contains(item.Object))
            return true;
        // Crashed into an earlier item in this pass: leave the rest as they are.
        if (m_State != State::Driving)
            return false;

        const glm::vec3 position = item.Object->GetTransform().GetPosition();
        if (Overlaps(truck, kTruckHalfSize, position, item.IsPickup ? kPickupHalfSize : kCrateHalfSize)) {
            if (!item.IsPickup) {
                Crash();
                return false; // the crate stays where the truck hit it
            }
            ++m_Pickups;
        } else if (position.z >= truck.z - kRemoveDistance) {
            return false; // still ahead, or not far enough behind
        }
        scene.Destroy(*item.Object);
        return true;
    });
}

int LaneRunner::Score() const
{
    // A point per meter driven, plus the logo boxes collected.
    return static_cast<int>(m_Truck.GetTransform().GetPosition().z) + kPickupPoints * m_Pickups;
}

void LaneRunner::DrawHud() const
{
    // The HUD goes straight onto ImGui's background layer, without any windows: over the scene,
    // but under the debug windows. Sizes are in points, scaled for the display like the debug
    // UI's (see Window.cpp).
    ImDrawList& drawList = *ImGui::GetBackgroundDrawList();
    const ImVec2 screen = ImGui::GetIO().DisplaySize;
    const float scale = ImGui::GetStyle().FontScaleDpi;
    const float margin = 20.0f * scale;

    HudText(drawList, m_HudFont, 40.0f * scale, ImVec2(margin, margin), std::format("Score {}", Score()));
    HudText(drawList, m_HudFont, 22.0f * scale, ImVec2(margin, margin + 46.0f * scale),
            std::format("Best {}    Logo boxes {}", m_BestScore, m_Pickups));
    // Meters per second to kilometers per hour: times 3600 seconds, divided by 1000 meters.
    HudText(drawList, m_HudFont, 40.0f * scale, ImVec2(screen.x - margin, margin),
            std::format("{:.0f} km/h", m_Truck.GetSpeed() * 3.6f), 1.0f);

    const ImVec2 center(screen.x / 2.0f, screen.y * 0.3f);
    if (m_State != State::Driving) {
        const bool ready = m_State == State::Ready;
        HudText(drawList, m_HudFont, 56.0f * scale, center, ready ? "LANE RUNNER" : "Crashed!", 0.5f);
        HudText(drawList, m_HudFont, 26.0f * scale, ImVec2(center.x, center.y + 70.0f * scale),
                ready ? "Press Space to drive" : "Press Space to drive again", 0.5f);
    }
    HudText(drawList, m_HudFont, 18.0f * scale, ImVec2(center.x, screen.y - margin - 18.0f * scale),
            "A/D or arrow keys: change lanes    F1: debug windows    Esc: quit", 0.5f);
}
