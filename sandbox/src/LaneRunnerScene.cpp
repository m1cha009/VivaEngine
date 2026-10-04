#include "LaneRunnerScene.h"

#include "FollowCamera.h"
#include "LaneRunner.h"
#include "Road.h"
#include "Treadmill.h"
#include "TruckController.h"

#include "Viva/Assets.h"
#include "Viva/Camera.h"
#include "Viva/Log.h"
#include "Viva/MeshRenderer.h"
#include "Viva/Model.h"
#include "Viva/Primitives.h"
#include "Viva/Renderer.h"
#include "Viva/Scene.h"

#include <imgui.h>

#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

using namespace Viva;

namespace {

// The road is kRoadLength long, starting kRoadBehind behind the truck. It's flat and simple, and
// the treadmill makes it endless. Its width and lanes are in Road.h.
constexpr float kRoadLength = 320.0f;
constexpr float kRoadBehind = 30.0f;
// The lane markings repeat every kDashPeriod meters: a dash, then a gap as long. The road texture
// is one repeat, and the road's treadmill steps by the same length.
constexpr float kDashPeriod = 8.0f;
constexpr float kLineHalfWidth = 0.08f;

// The grass around it, kGroundSize meters square, its texture repeating every kGrassPeriod.
constexpr float kGroundSize = 600.0f;
constexpr float kGrassPeriod = 4.0f;

// A light blue sky. Like every color the engine draws with, it's linear: the sRGB swapchain shows
// it brighter, about (150, 195, 240) in the 0-255 sRGB values a paint program shows.
constexpr glm::vec3 kSkyColor { 0.30f, 0.55f, 0.87f };

// Images made in code, for Renderer::CreateTexture: width x height pixels, row by row from the
// top, 4 bytes each. The colors are sRGB (0-255), like an image file's.
struct Pixels {
    Pixels(uint32_t width, uint32_t height)
        : Width(width)
        , Height(height)
        , Bytes(size_t { width } * height * 4)
    {
    }

    void Set(uint32_t x, uint32_t y, int red, int green, int blue)
    {
        uint8_t* pixel = &Bytes[(size_t { y } * Width + x) * 4];
        pixel[0] = static_cast<uint8_t>(red);
        pixel[1] = static_cast<uint8_t>(green);
        pixel[2] = static_cast<uint8_t>(blue);
        pixel[3] = 255;
    }

    uint32_t Width;
    uint32_t Height;
    std::vector<uint8_t> Bytes;
};

// The road's texture: asphalt with white lines. Across, it spans the road's width; along, one
// kDashPeriod: the top half has the dashes between the lanes, the bottom half the gaps. The solid
// lines along the edges run through both. A little random noise keeps the asphalt from looking
// like plastic.
Pixels RoadTexture(std::mt19937& random)
{
    Pixels road(256, 64);
    std::uniform_int_distribution noise(-6, 6);
    const auto laneCount = static_cast<float>(Road::kLaneCount);
    for (uint32_t y = 0; y < road.Height; ++y) {
        const bool dash = y < road.Height / 2;
        for (uint32_t x = 0; x < road.Width; ++x) {
            // Where this pixel is across the road, counted in lanes from one edge line. The lines
            // run along the whole numbers: solid at the edges (0 and the lane count), dashed in
            // between. std::round finds the nearest line.
            const float meters = (static_cast<float>(x) + 0.5f) / static_cast<float>(road.Width) * Road::kWidth;
            const float inLanes = (meters - Road::kShoulderWidth) / Road::kLaneWidth;
            const float line = std::round(inLanes);
            const bool onLine = line >= 0.0f && line <= laneCount && std::abs(inLanes - line) * Road::kLaneWidth < kLineHalfWidth;
            const bool edgeLine = line == 0.0f || line == laneCount;
            if (onLine && (edgeLine || dash)) {
                road.Set(x, y, 235, 232, 220);
            } else {
                const int gray = 70 + noise(random);
                road.Set(x, y, gray, gray, gray + 3);
            }
        }
    }
    return road;
}

// Grass: green, with random lighter and darker pixels.
Pixels GrassTexture(std::mt19937& random)
{
    Pixels grass(64, 64);
    std::uniform_int_distribution noise(-14, 14);
    for (uint32_t y = 0; y < grass.Height; ++y) {
        for (uint32_t x = 0; x < grass.Width; ++x) {
            const int shade = noise(random);
            grass.Set(x, y, 62 + shade, 122 + shade, 46 + shade / 2);
        }
    }
    return grass;
}

// tiling: how often the texture repeats across the plane it's on (see MaterialSettings).
std::shared_ptr<Material> TexturedMaterial(Renderer& renderer, const Pixels& pixels, const glm::vec2& tiling)
{
    return renderer.CreateMaterial({
        .Texture = renderer.CreateTexture(pixels.Width, pixels.Height, pixels.Bytes),
        .Tiling = tiling,
    });
}

// The built-in plane, scaled to `width` x `length` meters.
glm::vec3 PlaneScale(float width, float length)
{
    return { width / Primitives::kPlaneSize, 1.0f, length / Primitives::kPlaneSize };
}

} // namespace

void LoadLaneRunnerScene(Scene& scene, Assets& assets)
{
    // The road and the grass are made in code, so they're created through the renderer itself;
    // what comes from files comes from Assets.
    Renderer& renderer = assets.GetRenderer();
    const std::shared_ptr<Model> truckModel = assets.GetModel("models/CesiumMilkTruck.glb");
    if (!truckModel) {
        Log::Error("The Lane Runner can't start without the milk truck model");
        return;
    }
    GameObject& truck = truckModel->Instantiate(scene);
    TruckController& truckController = truck.AddComponent<TruckController>();
    const Transform& truckTransform = truck.GetTransform();

    // A fixed seed: the textures come out the same in every run.
    std::mt19937 random(2026);

    // The road, from kRoadBehind behind the truck to the rest ahead of it. Its texture repeats once
    // across it, and every kDashPeriod meters along it.
    GameObject& road = scene.CreateGameObject("Road");
    road.GetTransform().LocalPosition.z = kRoadLength / 2.0f - kRoadBehind;
    road.GetTransform().LocalScale = PlaneScale(Road::kWidth, kRoadLength);
    road.AddComponent<MeshRenderer>(assets.GetMesh("Primitives::Plane"),
                                    TexturedMaterial(renderer, RoadTexture(random), { 1.0f, kRoadLength / kDashPeriod }));
    road.AddComponent<Treadmill>(truckTransform, kDashPeriod);

    // The grass, a little below the road so that the road wins the depth test where they overlap.
    GameObject& grass = scene.CreateGameObject("Grass");
    grass.GetTransform().LocalPosition.y = -0.05f;
    grass.GetTransform().LocalScale = PlaneScale(kGroundSize, kGroundSize);
    grass.AddComponent<MeshRenderer>(assets.GetMesh("Primitives::Plane"),
                                     TexturedMaterial(renderer, GrassTexture(random), glm::vec2(kGroundSize / kGrassPeriod)));
    grass.AddComponent<Treadmill>(truckTransform, kGrassPeriod);

    // The game's rules, with what they place on the road. The HUD's font is ImGui's built-in one
    // in its scalable version, which stays sharp at large sizes (the debug UI keeps the default).
    GameObject& game = scene.CreateGameObject("Lane Runner");
    game.AddComponent<LaneRunner>(truckController, assets.GetMesh("Primitives::Cube"),
                                  assets.GetMaterial({ .Texture = assets.GetTexture("textures/crate.png") }),
                                  assets.GetModel("models/BoxTextured/BoxTextured.gltf"),
                                  ImGui::GetIO().Fonts->AddFontDefaultVector());

    // The camera, following the truck.
    GameObject& cameraObject = scene.CreateGameObject("Camera");
    Camera& camera = cameraObject.AddComponent<Camera>();
    camera.BackgroundColor = kSkyColor;
    // The nearest the camera ever gets to anything is meters away. A larger near plane gives the
    // depth buffer more precision far away, where the road and the grass just below it would
    // otherwise flicker through each other ("z-fighting").
    camera.NearPlane = 0.5f;
    cameraObject.AddComponent<FollowCamera>(truckTransform);
}
