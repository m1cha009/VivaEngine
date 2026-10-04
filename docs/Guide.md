# Using VivaEngine

How to build, run and make things with the engine, as of M12. The milestone docs in
[`milestones/`](milestones/) explain *why* things work the way they do; this page is the
practical summary.

## What it is

VivaEngine is a small C++20 / Vulkan 1.3 game engine that works the way Unity does. You build a
world from **GameObjects with components**, write behavior as components with
`OnStart`/`OnUpdate`, and the engine handles the window, input, timing and drawing. It runs on
Windows (tested) and is kept Mac-ready (CI builds and runs it on macOS).

A game only ever includes `Viva/...` headers, plus GLM for math and `imgui.h` for debug UI. SDL3
and Vulkan stay hidden inside the engine.

| Layer | What's in it |
|---|---|
| **Core** | `Application` (main loop), `Log`, `VIVA_ASSERT`, `Time` (delta time, 50 Hz fixed step) |
| **Platform** | Window, `Input` (keyboard, mouse), file paths (SDL3 lives only here) |
| **Renderer** | Vulkan: meshes, textures with mipmaps, materials, camera, deferred GPU deletion, Dear ImGui |
| **Scene** | `Scene`, `GameObject`, `Component`, `Transform` hierarchy, `Camera`, `MeshRenderer`, glTF `Model` |
| **Sandbox** | The Lane Runner game, plus the demo scene (`--demo`), using the engine like any game would |

## Build and run

**CLion:** open the `VivaEngine` folder, enable the `windows-debug` profile, pick **Sandbox**,
then Run. Add `--demo` under the run configuration's Program arguments for the demo scene.

**Command line (Windows):**

```bat
scripts\build.cmd windows-debug
build\windows-debug\bin\Sandbox.exe
```

macOS: see the [README](../README.md).

- **Options:** `--demo` (the demo scene), `--no-vsync`, `--display 1` (second monitor),
  `--quit-after 5`.
- **A release folder you can share:** `scripts\package.cmd MyBuild` puts it in `dist\MyBuild\`.
- **Debug builds** run the Vulkan validation layer; its errors show up in the log.
- **F1** shows the Stats window and the Scene window (hierarchy and inspector).

## Making something with it

Every game has the same shape as `sandbox/src/Main.cpp`: a class derived from `Application`.
Its `OnStart` builds the scene, and its `OnUpdate` handles game-wide things. The easiest way to
experiment is to add your own scene next to the existing two (`DemoScene.cpp`,
`LaneRunnerScene.cpp`).

**1. Write a scene function**, for example `sandbox/src/MyScene.cpp`:

```cpp
#include "Viva/Camera.h"
#include "Viva/MeshRenderer.h"
#include "Viva/Model.h"
#include "Viva/Primitives.h"
#include "Viva/Renderer.h"
#include "Viva/Scene.h"

#include <glm/gtc/quaternion.hpp>

using namespace Viva;

// A component is a MonoBehaviour: override what you need.
class Spin : public Component {
protected:
    void OnUpdate(float dt) override
    {
        Transform& transform = GetTransform();
        transform.LocalRotation = glm::normalize(glm::angleAxis(dt, glm::vec3(0, 1, 0)) * transform.LocalRotation);
    }
};

void LoadMyScene(Scene& scene, Renderer& renderer)
{
    // GPU resources: create once while loading, then share them between objects.
    auto cube = renderer.CreateMesh(Primitives::Cube());
    auto crateMaterial = renderer.CreateMaterial({ .Texture = renderer.LoadTexture("textures/crate.png") });

    GameObject& crate = scene.CreateGameObject("Crate");
    crate.AddComponent<MeshRenderer>(cube, crateMaterial);
    crate.AddComponent<Spin>();

    // A glTF model: load once, then Instantiate as often as you like (a prefab).
    std::unique_ptr<Model> truck = Model::Load(renderer, "models/CesiumMilkTruck.glb");
    GameObject& truckObject = truck->Instantiate(scene);
    truckObject.GetTransform().LocalPosition = { 4, 0, 0 };

    // Without a camera nothing is drawn.
    GameObject& camera = scene.CreateGameObject("Camera");
    camera.GetTransform().LocalPosition = { 0, 2, -8 };
    camera.AddComponent<Camera>().BackgroundColor = { 0.3f, 0.55f, 0.87f };
    camera.GetTransform().LookAt({ 2, 0.5f, 0 });
}
```

**2. Add it to the build:** list `src/MyScene.cpp` in `add_executable(Sandbox ...)` in
`sandbox/CMakeLists.txt`.

**3. Call it:** in `SandboxApp::OnStart` (`Main.cpp`), call `LoadMyScene(GetScene(), GetRenderer());`
instead of `LoadLaneRunnerScene`, or behind a new command-line flag like `--demo`.

**Assets:** put files under `assets/` and list them in `viva_copy_assets(...)` in
`sandbox/CMakeLists.txt`. Load them by their path inside `assets/`, such as `"textures/crate.png"`
or `"models/x.glb"`. PNG/JPEG textures and `.gltf`/`.glb` models work. Credits for third-party
assets go in a `CREDITS.md` next to them.

**Debug UI:** call ImGui anywhere in `OnUpdate`:
`ImGui::Begin("Tuning"); ImGui::SliderFloat("Speed", &m_Speed, 0, 10); ImGui::End();`

## Unity → VivaEngine cheat sheet

| Unity | VivaEngine |
|---|---|
| `MonoBehaviour` + `Start`/`Update`/`LateUpdate`/`FixedUpdate` | `Component` + `OnStart`/`OnUpdate`/`OnLateUpdate`/`OnFixedUpdate` |
| `OnDestroy()` | the component's destructor |
| `new GameObject("x")` / `Destroy(go)` | `scene.CreateGameObject("x", parent)` / `scene.Destroy(go)` |
| `AddComponent<T>()` / `GetComponent<T>()` | the same; `AddComponent` passes constructor arguments, and `GetComponent` returns a pointer (`nullptr` if absent) |
| `transform.localPosition`, `localRotation`, `localScale` | `GetTransform().LocalPosition` / `LocalRotation` (a `glm::quat`) / `LocalScale` |
| `transform.position`, `forward`, `rotation`, `LookAt`, `Find` | `GetPosition()`, `Forward()`, `GetRotation()`, `LookAt()`, `Find("a/b")` |
| `Instantiate(prefab)` | `model->Instantiate(scene, parent)` |
| `Resources.Load<Texture2D>` / `new Texture2D` + `SetPixels32` | `renderer.LoadTexture(...)` / `renderer.CreateTexture(w, h, pixels)` |
| `new Material` / `MeshFilter` + `MeshRenderer` | `renderer.CreateMaterial({ .Texture, .Color })` / `MeshRenderer(mesh, material)` |
| `Camera.main`, `backgroundColor`, `fieldOfView` | `scene.GetMainCamera()`, `Camera::BackgroundColor`, `FieldOfView` (radians) |
| `Input.GetKeyDown(KeyCode.Space)` | `Input::GetKeyDown(Key::Space)`; also `GetMouseButton`, `MouseDelta()`, `SetCursorLocked()` |
| `Time.deltaTime` / `Debug.Log` / `Debug.Assert` | `Time::DeltaTime()` / `Log::Info("x = {}", x)` / `VIVA_ASSERT(cond)` |

## Rules worth remembering

- **After `Destroy`, forget the object.** C++ has no garbage collector and no "fake null", so a
  stored pointer to a destroyed object is invalid. If something else might destroy it, check
  `scene.Contains(object)` before using it.
- **Create meshes, textures and materials while loading, not every frame.** Each one waits for
  the GPU. Creating GameObjects and calling `Instantiate` at runtime is cheap. GPU resources are
  `shared_ptr`s and are freed when their last user lets go.
- **Use the GameObject in `OnStart`, not in the constructor.** The constructor runs before the
  component is attached.
- **Move things in `OnUpdate`; follow or check them in `OnLateUpdate`.** That's how cameras and
  collision checks see this frame's positions. Don't rely on the order GameObjects were created.
- **Conventions:** right-handed, Y up, meters; **Forward is +Z, Right is −X**; angles in radians
  (`glm::radians(45.0f)`); colors are linear; front faces are counter-clockwise.

## What it doesn't do yet

- No lighting or shadows: everything is unlit, opaque texture × color.
- No physics (collisions are your own box tests), audio, animation or skinning.
- No saved scenes and no editor.
- One built-in shader (Unlit); games can't add their own yet.
- Mirrored (negative scale) objects draw inside out.

These are the "Beyond M12" options in [`CLAUDE.md`](../CLAUDE.md).

## Where to read more

- **Each milestone explained**, with Unity comparisons: [`milestones/M0.md`](milestones/M0.md) to
  [`M12.md`](milestones/M12.md). M8 (renderer), M10 (scene) and M11 (models) matter most for using
  the engine.
- **Two complete examples:** `sandbox/src/DemoScene.cpp` and `sandbox/src/LaneRunnerScene.cpp`.
- **Every public header** in `engine/include/Viva/` is commented for exactly this use.
