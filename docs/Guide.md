# Using VivaEngine

How to build, run and make things with the engine, as of M18. The milestone docs in
[`milestones/`](milestones/) explain *why* things work the way they do; this page is the
practical summary.

## What it is

VivaEngine is a small C++20 / Vulkan 1.3 game engine that works the way Unity does. You build a
world from **GameObjects with components**, write behavior as components with
`OnStart`/`OnUpdate`, and the engine handles the window, input, timing and drawing. It runs on
Windows.

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

- **Options:** `--demo` (the demo scene), `--load <file>` (a scene file), `--save <file>` (save the
  scene once it's built), `--no-vsync`, `--display 1` (second monitor), `--quit-after 5`.
- **A release folder you can share:** `scripts\package.cmd MyBuild` puts it in `dist\MyBuild\`.
- **Debug builds** run the Vulkan validation layer; its errors show up in the log.
- **F1** shows the Stats window and the Scene window (hierarchy and inspector).

## The editor (VivaEditor)

`build\windows-debug\bin\VivaEditor.exe` (or the **VivaEditor** run configuration in CLion)
opens the **Project Manager**, like Unity Hub: create a project, open it, remove it from the list
or delete it from disk. A project is a folder with a `.vivaproject` file, an `Assets` folder (asset
names start there) and `Scenes/Main.scene`. Opening one opens the editor: Scene (right mouse
button to look, with W/A/S/D while held; click to select; F frames the selection; a ground grid;
Move/Rotate/Scale gizmos on W/E/R, X for local/global, Ctrl to snap),
Hierarchy (right-click to create, drag to reparent, F2 to rename), Inspector (edit any field, pick
meshes and textures, Add/Remove Component), Project (double-click a scene to open it, a model to
place it) and Console, docked, with File, Edit, GameObject and Window menus and a Save prompt for
unsaved changes, and Undo/Redo (Ctrl+Z/Ctrl+Y) for every edit. **Play** (Ctrl+P) runs the scene in
the Game view and **Stop** puts it back ([M18](milestones/M18.md)); **File > Build...** makes a
folder that runs the project without the editor, through **VivaPlayer**
(`VivaPlayer --project <folder>` plays a project folder directly). See [M16](milestones/M16.md) and
[M17](milestones/M17.md) for building a scene by hand. Options: `--open <folder>`,
`--build <folder>` (with `--open`: build, then quit),
`--settings <folder>` (where the project list lives, normally `%APPDATA%\Viva\VivaEditor`), and
the common `--display`, `--no-vsync` and `--quit-after`. See [M14](milestones/M14.md).

## Making something with it

Every game has the same shape as `sandbox/src/Main.cpp`: a class derived from `Application`.
Its `OnStart` builds the scene, and its `OnUpdate` handles game-wide things. The easiest way to
experiment is to add your own scene next to the existing two (`DemoScene.cpp`,
`LaneRunnerScene.cpp`).

**1. Write a scene function**, for example `sandbox/src/MyScene.cpp`:

```cpp
#include "Viva/Assets.h"
#include "Viva/Camera.h"
#include "Viva/MeshRenderer.h"
#include "Viva/Model.h"
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

void LoadMyScene(Scene& scene, Assets& assets)
{
    // Assets by name: each is loaded once and shared between the objects that use it.
    auto cube = assets.GetMesh("Primitives::Cube");
    auto crateMaterial = assets.GetMaterial({ .Texture = assets.GetTexture("textures/crate.png") });

    GameObject& crate = scene.CreateGameObject("Crate");
    crate.AddComponent<MeshRenderer>(cube, crateMaterial);
    crate.AddComponent<Spin>();

    // A glTF model: loaded once, then Instantiate as often as you like (a prefab).
    std::shared_ptr<Model> truck = assets.GetModel("models/CesiumMilkTruck.glb");
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

**3. Call it:** in `SandboxApp::OnStart` (`Main.cpp`), call `LoadMyScene(GetScene(), GetAssets());`
instead of `LoadLaneRunnerScene`, or behind a new command-line flag like `--demo`.

**Or write a scene file** instead of code: see `assets/scenes/Shapes.scene`, and run
`Sandbox --load` with it. `Sandbox --save my.scene` saves whatever scene was built, so a scene made
in code can be turned into a file. To be saved, a component type must be registered
(`ComponentRegistry::Register<Spin>("Spin")` in `OnStart`) and list its fields in `VisitFields`;
meshes and textures must come from `Assets`. See [M13](milestones/M13.md).

**Assets:** put files under `assets/` and list them in `viva_copy_assets(...)` in
`sandbox/CMakeLists.txt`. Load them through `GetAssets()` by their path inside `assets/`, such as
`"textures/crate.png"` or `"models/x.glb"`. PNG/JPEG textures and `.gltf`/`.glb` models work. The
built-in meshes are `"Primitives::Cube"`, `"Primitives::Plane"` (10 × 10), `"Primitives::Sphere"`
and `"Primitives::Cylinder"`. Credits for third-party assets go in a `CREDITS.md` next to them.

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
| `Instantiate(prefab)` | `assets.GetModel("models/x.glb")->Instantiate(scene, parent)` |
| `Instantiate(gameObject)` / `Destroy(component)` | `scene.Instantiate(gameObject, assets, parent)` / `scene.Destroy(component)` |
| `transform.position = p` / `transform.rotation = r` / `GetInstanceID()` | `SetPosition(p)` / `SetRotation(r)` / `GetId()` |
| `transform.SetParent(p, worldPositionStays)` / `mesh.bounds` | `SetParent(&p, keepWorldPose)` / `Renderer::GetBounds(mesh)` |
| `Resources.Load<Texture2D>` / `new Texture2D` + `SetPixels32` | `assets.GetTexture(...)` / `renderer.CreateTexture(w, h, pixels)` |
| `GameObject.CreatePrimitive(PrimitiveType.Cube)` | `assets.GetMesh("Primitives::Cube")` in a `MeshRenderer` |
| `new Material` / Tiling, Offset / `MeshFilter` + `MeshRenderer` | `assets.GetMaterial({ .Texture, .Color, .Tiling, .Offset })` / `MeshRenderer(mesh, material)` |
| `[SerializeField]` fields / `EditorSceneManager.SaveScene` / `LoadScene(…, Additive)` | `VisitFields` + `ComponentRegistry::Register<T>` / `scene.Save(path)` / `scene.Load(path, assets)` |
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
- Projects are data only: their scenes use the engine's components (MeshRenderer, Camera, Spinner,
  FlyCamera). A project's own C++ code is the first "Later" item in
  [`EditorRoadmap.md`](EditorRoadmap.md).
- One built-in shader (Unlit); games can't add their own yet.
- Mirrored (negative scale) objects draw inside out.

The rest are the "Beyond M18" options in [`CLAUDE.md`](../CLAUDE.md).

## Where to read more

- **Each milestone explained**, with Unity comparisons: [`milestones/M0.md`](milestones/M0.md) to
  [`M18.md`](milestones/M18.md). M8 (renderer), M10 (scene), M11 (models), M13 (assets, scene
  files), M14 (projects), M15 (the editor), M16 (editing a scene), M17 (gizmos, undo) and M18 (Play
  mode, builds) matter most for using the engine.
- **Complete examples:** `sandbox/src/DemoScene.cpp` and `sandbox/src/LaneRunnerScene.cpp` in code,
  `assets/scenes/Shapes.scene` and `Demo.scene` as files.
- **Every public header** in `engine/include/Viva/` is commented for exactly this use.
