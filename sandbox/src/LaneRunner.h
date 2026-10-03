#pragma once

#include "Viva/Component.h"
#include "Viva/Model.h"
#include "Viva/Renderer.h"

#include <memory>
#include <random>
#include <vector>

struct ImFont;
class TruckController;

namespace Viva {
class GameObject;
}

// The game's rules, like a "GameManager" MonoBehaviour in Unity. The truck drives down a road with
// three lanes. Rows of crates block one or two lanes, and Cesium logo boxes in the open lanes are
// worth points. This component:
//   - starts a run when Space is pressed, in OnUpdate;
//   - in OnLateUpdate, once the truck has moved: places rows ahead of it and removes those behind
//     it, checks whether it hits a crate (the run ends) or a logo box (collected), and draws the
//     score with Dear ImGui.
class LaneRunner : public Viva::Component {
public:
    // The crate's mesh and material and the logo box model are kept, to place as many as needed.
    // hudFont: the large font for the score (see LaneRunnerScene.cpp).
    LaneRunner(TruckController& truck, std::shared_ptr<Viva::Mesh> crateMesh,
               std::shared_ptr<Viva::Material> crateMaterial, std::unique_ptr<Viva::Model> pickupModel,
               ImFont* hudFont);

protected:
    void OnStart() override;
    void OnUpdate(float dt) override;
    void OnLateUpdate(float dt) override;

private:
    enum class State { Ready, Driving, Crashed };

    // Something on the road: a crate to avoid, or a logo box to collect. The scene owns the
    // GameObject; this only points at it (M10).
    struct RoadItem {
        Viva::GameObject* Object;
        bool IsPickup;
    };

    void ResetRun();
    void Crash();
    void SpawnRowsAhead();
    void SpawnRow(float z);
    void UpdateItems();
    int Score() const;
    void DrawHud() const;

    TruckController& m_Truck;
    std::shared_ptr<Viva::Mesh> m_CrateMesh;
    std::shared_ptr<Viva::Material> m_CrateMaterial;
    std::unique_ptr<Viva::Model> m_PickupModel; // null if the model didn't load: no pickups then
    ImFont* m_HudFont;

    std::vector<RoadItem> m_Items;
    // A random number generator (Unity's Random), seeded from the OS, so every game is different.
    std::mt19937 m_Random { std::random_device {}() };
    State m_State = State::Ready;
    float m_NextRowZ = 0.0f; // where the next row goes
    int m_Pickups = 0;       // collected in this run
    int m_BestScore = 0;     // since the game started
};
