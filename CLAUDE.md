# Project brief

## Who you're working with

- **Michail**: an experienced Unity/C# game developer with a game shipped on Steam. He has **not written C++ in years**.
- **Goal: learn how a game engine works by building one.** Understanding matters more than speed or features.
- **Michail writes no code. You write all of it.** He reviews after each milestone, builds and runs it in CLion on Windows, and asks questions.
- So the code and explanations must teach:
  - When a C++ idiom or Vulkan concept appears for the first time, explain it briefly in comments and in the milestone doc.
  - Compare to C#/Unity where it helps. For example:
    - a destructor works like `IDisposable` or `OnDestroy`
    - `std::unique_ptr` expresses single ownership, where C# has the GC
    - the header/source split has no C# equivalent
    - `Application::OnUpdate` plays the role of `MonoBehaviour.Update`

## Tech stack (decided; ask before changing anything here)

| Area | Choice |
|---|---|
| Language | C++20 |
| Build | CMake ≥ 3.25 with `CMakePresets.json`; dependencies via `FetchContent` |
| Compiler | MSVC on Windows |
| IDE (Michail's) | CLion; the project must open cleanly through the presets |
| Platform layer | **SDL3**, pinned to tag `release-3.4.18`, built as a **static** library (`SDL_STATIC ON`, `SDL_SHARED OFF`) |
| Graphics | **Vulkan, plain C API** (`vulkan.h`), loader and headers from the LunarG Vulkan SDK via `find_package(Vulkan REQUIRED COMPONENTS glslc)` |
| Vulkan version | Target **Vulkan 1.3 core**: dynamic rendering and synchronization2. **No `VkRenderPass` or `VkFramebuffer`.** |
| Shaders | GLSL in `shaders/`, compiled to SPIR-V at build time with `glslc` (CMake custom command). `.spv` files are copied next to the executable. |
| Math | GLM (FetchContent) with `GLM_FORCE_DEPTH_ZERO_TO_ONE` and `GLM_FORCE_RADIANS` |
| GPU memory | Vulkan Memory Allocator (VMA) 3.4.0 (FetchContent, header-only, PRIVATE to the engine), since M5 |
| Image decoding | stb_image (FetchContent, pinned commit archive, PNG/JPEG only, PRIVATE to the engine), since M7 |

**Approved for later milestones** (add each only when its milestone arrives):
- Dear ImGui (SDL3 + Vulkan backends): added in M9
- cgltf: added in M11
- Dear ImGui's docking build (`v1.92.9b-docking`, replacing the plain release): approved 2026-10-04, added in M15

**Anything else needs Michail's approval first.**

**Do not use** vk-bootstrap, vulkan.hpp/RAII wrappers, or any engine framework. Writing the instance, device and swapchain setup by hand *is* the learning.

**References:**
- Primary: vkguide.dev (Vulkan 1.3, dynamic rendering, SDL)
- Secondary: the docs.vulkan.org tutorial and the Vulkan spec

## Platforms

- **Windows x64** with an NVIDIA RTX 3080 Ti. This is the dev machine and the **only platform**. The toolchain is MSVC from Visual Studio Build Tools (no VS IDE), with CMake, Ninja, Git, the Vulkan SDK, CLion and RenderDoc installed and verified.
- **macOS is dropped** (Michail's decision, 2026-10-04). Don't spend time on Mac builds:
  - New code doesn't have to be Mac-ready, and nothing is added to `docs/MacChecklist.md`, which is archived.
  - There is no macOS CI any more (the workflow was deleted).
  - The Mac code that already exists (portability extensions, `SDL_WINDOW_HIGH_PIXEL_DENSITY` and pixel-size swapchains, the `macos-*` presets, the Clang warning flags) stays as long as it costs nothing. Remove it if it ever gets in the way, rather than maintaining it.

**Isolation rules** (kept for clean layering, not for porting):
- Platform `#ifdef`s live only in `engine/src/Platform/`.
- SDL headers are included only in `Platform/`.
- Vulkan headers are included only in `Renderer/`.

## Repository layout

```
VivaEngine/
├── CLAUDE.md
├── README.md               # how to build and run on Windows
├── CMakeLists.txt
├── CMakePresets.json
├── cmake/                  # helper scripts (dependencies, shader compilation)
├── engine/                 # static library target: VivaEngine
│   ├── include/Viva/       # public headers (what the game sees)
│   └── src/
│       ├── Core/           # Application, Log, Assert, Time
│       ├── Platform/       # Window, Input (SDL3 lives only here)
│       ├── Renderer/       # all Vulkan code
│       └── Scene/          # Scene, GameObject, Component, Transform, Camera (since M10), Model (M11), Assets and scene files (M13)
├── sandbox/                # executable target: test app / game using the engine
├── editor/                 # executable target: VivaEditor, Project Manager + scene editor (since M14)
├── shaders/                # GLSL sources
├── assets/                 # textures, models, scenes (scene files since M13)
├── scripts/                # build.cmd: Windows command-line build through vcvars64.bat
└── docs/
    ├── Guide.md            # how to use the engine: build, run, make a scene, cheat sheet (keep it current)
    ├── EditorRoadmap.md    # M13–M18 checklists: projects and the editor (tick items as they land)
    └── milestones/         # one explainer per milestone: M0.md, M1.md, ...
```

## Code conventions

**Naming and files:**
- Everything lives in `namespace Viva`.
- Types and functions use `PascalCase`; locals and parameters use `camelCase`.
- Member variables use `m_Name`; constants use `kName`; variables with static storage (file-local state in an anonymous namespace, static members) use `s_Name`.
- One class per file pair, with matching names. Use `#pragma once`.

**Ownership:**
- RAII everywhere. Each Vulkan handle has a clear owner that destroys it, in reverse creation order.
- No raw `new`/`delete`. Use `std::unique_ptr` for ownership.
- Resource-owning classes are non-copyable, and movable only where needed.

**Errors and logging:**
- A `VK_CHECK(expr)` macro logs file, line and the `VkResult` name, then aborts in Debug.
- No exceptions in engine code.
- Logging goes through a small `Viva::Log` (levels: Trace/Info/Warn/Error) using `std::format`.
  - If `std::format` doesn't work there, fall back to `fmt` via FetchContent and record that in the decision log.

**Build settings:**
- Warnings: `/W4` on MSVC.
- Engine code builds warning-free; third-party code is excluded from these warning flags.
- Debug builds enable validation layers and a debug messenger routed into `Viva::Log`. Release builds disable both.

**Style:**
- Comments explain *why*, not *what*.
- Prefer clear code to clever code. Don't add abstractions until the current milestone needs them.

## Workflow rules (important)

1. **One milestone at a time.** Never start the next milestone until Michail says so. (Since 2026-10-03, Michail has asked for the remaining milestones to be done back to back without waiting for his review. Still stop for decisions that are his to make.)
2. **At the start of a milestone**, post a short plan (files to add or change, new concepts), then implement. Wait for approval only if something deviates from this brief.
3. **A milestone is done only when all of these hold:**
   - It configures and builds in Debug with zero warnings in engine code.
   - It runs, and the Vulkan validation layers report no errors or warnings during startup, running, resizing, minimizing and shutdown (from M2 onward).
   - The `/simplify` skill has been run on the milestone's changes, and its fixes are applied and re-verified before the commit.
   - `docs/milestones/Mx.md` is written, covering:
     - what was built
     - the concepts in plain language, with Unity comparisons
     - a **reading guide**: the files in the order Michail should read them, with what to notice in each
     - **how to test**: what he should see, and things to try (resize, minimize, keys)
     - known limitations
     - that it was verified on Windows
   - The **Status** section of `CLAUDE.md` is updated.
   - There is **one git commit**, e.g. `M3: Swapchain and clear screen`. Push it to `origin/main` right away (Michail approved pushing each milestone on 2026-10-03). Never force-push, and push nothing else unless Michail asks.
   - A Release executable is packaged with `scripts\package.cmd Mx` into `dist/Mx/` (git-ignored). Add a short `README.txt` there: what to look at, and the controls.
4. **Then** give Michail a short summary: what changed, what to read first, and how to run it (including the packaged executable).
5. **Ask before:**
   - adding a dependency that isn't in the approved list
   - changing the stack or architecture
   - reordering the roadmap
   - doing anything destructive in git
6. **When Michail asks a question, answer it fully** before continuing. Explaining is part of the job.
7. **On Windows, make command-line builds work too.**
   - If Ninja can't find `cl.exe` from a plain shell, either provide a Visual Studio generator preset for command-line builds or run through `vcvars64.bat`.
   - Pick one approach, make sure CLion still works, and document it in the README and the decision log.

## Toolchain check (first session on a new machine)

Run these and report the results:
- `cmake --version` (needs ≥ 3.25)
- `ninja --version`
- the compiler version (`cl`)
- the `VULKAN_SDK` environment variable
- `glslc --version`
- `vulkaninfo --summary` (GPU, driver, supported Vulkan API version)
- `git --version`

## Roadmap

**M0: Project skeleton**
- Root CMake; `engine` static library and `sandbox` executable.
- Presets: `windows-debug`, `windows-release`, `macos-debug`, `macos-release`.
- FetchContent for SDL3 (static, `release-3.4.18`) and GLM; Vulkan found from the SDK.
- Shader-compilation CMake function, wired up but with no shaders yet.
- `Viva::Log`, `VIVA_ASSERT`, `.gitignore` (build dirs, `.idea/`, `.vs/`, `cmake-build-*`), README with build steps for both platforms.
- *You should see:* the sandbox prints the engine version, the SDL version, and the Vulkan header version, then exits.

**M1: Window, input, game loop**
- `Viva::Application` base class owning the main loop, with virtual `OnStart`, `OnUpdate(float dt)`, `OnFixedUpdate(float fixedDt)` and `OnShutdown`. The sandbox subclasses it.
- `Window` wrapping SDL3, created with `SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY`; handles quit, resize and minimize events.
- `Input` with keyboard and mouse state: pressed this frame, held, released (like `GetKeyDown`, `GetKey`, `GetKeyUp`).
- `Time`: delta time and a fixed-timestep accumulator (50 Hz default, like Unity).
- *You should see:* an empty window with FPS in the title, key presses logged, and ESC to quit.

**M2: Vulkan instance, validation, device**
- `VkInstance` (API 1.3) with extensions from `SDL_Vulkan_GetInstanceExtensions`; validation layer plus debug messenger in Debug; portability handling on macOS.
- Surface via `SDL_Vulkan_CreateSurface`.
- Physical device selection: prefer a discrete GPU; require graphics and present queues, the swapchain extension, and the `dynamicRendering` and `synchronization2` features.
- Logical device and queues; log the GPU name and API version.
- Concepts: what an instance, physical device, logical device and queue each are.
- *You should see:* the GPU info logged, and a clean shutdown with no validation output.

**M3: Swapchain and clear screen (first pixels)**
- Swapchain: explain the surface format and present mode choice (FIFO by default, MAILBOX optional); image views.
- Command pool and buffers; 2 frames in flight with fences and semaphores.
- Image layout transitions with synchronization2.
- Dynamic rendering that clears to a color cycling over time.
- Recreate the swapchain on resize; skip rendering while minimized.
- *You should see:* a window that smoothly changes color and survives resizing and minimizing.

**M4: First triangle**
- Shaders compiled by the build; pipeline layout; graphics pipeline using `VkPipelineRenderingCreateInfo`; dynamic viewport and scissor.
- Vertices hardcoded in the vertex shader first.
- *You should see:* an RGB triangle.

**M5: Buffers and GPU memory**
- Vertex and index buffers.
- Do it manually once: find the memory type, `vkAllocateMemory`, upload through a staging buffer with an immediate-submit helper. Explain why GPU memory types exist.
- Then introduce VMA and refactor to it.
- *You should see:* a colored quad and other meshes drawn from buffers.

**M6: 3D: transforms, depth, camera**
- GLM; push constants for the model matrix; per-frame uniform buffer for view and projection, using descriptor set layout, pool and sets.
- Depth buffer. Explain Vulkan's clip space: Y points down and depth runs 0 to 1.
- *You should see:* a spinning cube and a free-fly camera (WASD plus right-mouse look, like Unity's Scene view).

**M7: Textures**
- stb_image; `VkImage`, image view and sampler; staging upload and layout transitions; mipmap generation.
- *You should see:* a textured cube.

**M8: Renderer abstraction**
- `Mesh`, `Texture`, `Shader`/`Material`, and a `Renderer` API (`BeginFrame` / `Submit` / `EndFrame`).
- A deferred-deletion queue for GPU resources.
- After this, the sandbox no longer touches Vulkan at all.
- *You should see:* the same results as before, with much simpler sandbox code.

**M9: Debug UI**
- Dear ImGui using the SDL3 and Vulkan backends.
- Stats window (FPS, frame time, draw calls) and camera controls.

**M10: Scene and entities/components**
- First, discuss the options with Michail: a simple hand-written ECS vs EnTT, and Unity's GameObject model vs ECS. (Decided 2026-10-03: **Unity-style GameObjects**. A `GameObject` owns `Component`s with virtual `OnStart`/`OnUpdate`, like `MonoBehaviour`. The M10 doc explains briefly how ECS differs.)
- `Transform` with a parent/child hierarchy, `MeshRenderer`, and `Camera` components; the scene drives rendering.

**M11: Model loading**
- glTF via cgltf: meshes, base-color materials, and the node hierarchy loaded into the Scene.
- Test models (approved 2026-10-03): `BoxTextured` and `CesiumMilkTruck` from KhronosGroup/glTF-Sample-Assets (CC-BY 4.0), committed to `assets/models/` with a credits file.

**M12: First small game**
- Propose 2–3 small game ideas; Michail picks one. (Picked 2026-10-03: **lane runner**. Drive a truck down a 3-lane road, dodge obstacles and collect pickups while the speed rises; follow camera, score in ImGui.)
- Build it in the sandbox, adding only the engine features it needs.

**M13–M18: Projects and the editor** (planned 2026-10-04, at Michail's request). The full checklists are in `docs/EditorRoadmap.md`; tick items there as they land.
- **M13: Scene serialization.** Hand-written JSON, a component registry, scene save/load, Sphere and Cylinder primitives.
- **M14: Projects and the Project Manager.** Project folders (`.vivaproject`, `Assets/`, `Scenes/`), a recent-projects list, and the new `VivaEditor` executable that starts in a Project Manager: create, add existing, open, remove from list, delete from disk.
- **M15: Editor shell.** The scene rendered to a texture inside a docking ImGui layout: Scene view, Hierarchy, Inspector, Project, Console; an editor camera; menu bar; unsaved-change prompts.
- **M16: Placing and editing objects.** A GameObject menu for primitives, a full Inspector, Hierarchy editing, click-to-select (ray vs bounding box), grid, save/reopen.
- **M17: Gizmos and undo.** Hand-written move/rotate/scale gizmos (W/E/R), snapping, undo/redo.
- **M18: Play mode and builds.** Play/Pause/Stop with a scene snapshot, a `VivaPlayer` executable, File > Build.

**Beyond M18** (decide together): a project's own C++ code (DLL + hot reload), asset GUIDs and `.meta` files, prefabs, lighting and PBR, shadows, audio via SDL3, physics (Jolt), asset pipeline, shader hot reload, multithreaded rendering.

## Status

- **Current milestone:** none. M0–M17 are done (M0–M8 on 2026-10-03, M9–M17 on 2026-10-04). Next: M18 (Play mode and builds, see `docs/EditorRoadmap.md`), once Michail says to start.
- **Verified on Windows:** M0–M17, built and run from the command line (`scripts/build.cmd`) in Debug and Release with zero warnings. Windows were driven by an automated script on the second monitor (keys, mouse clicks, minimize, resize, maximize, close), with screenshots via PrintWindow. Validation, including synchronization validation since M3, was silent. Michail checked mouse look by hand on the M8 build (2026-10-04): it works as intended. Not yet verified in CLion.
- **macOS:** dropped on 2026-10-04 and never tested on a real Mac. The macOS CI was green through M11 before it was deleted.
- **Dev machine notes:** VS Build Tools 2026 (MSVC 14.51) is the compiler. VS Community 2026 (no C++ workload) and Build Tools 2019 (MSVC 14.29) are also installed, so CLion's toolchain must point at Build Tools 2026. Implicit Vulkan layers are installed (RTSS, Overwolf, Steam overlay). Keep an eye on them when validation output appears in M2.

## Decision log

Append one line per decision: date, decision, reason.

- 2026-10-03: Stack fixed as C++20 / CMake / SDL3 / Vulkan 1.3 (C API) / GLSL→SPIR-V / GLM. Targets are Windows and macOS (M1 Pro). Goal is learning; Michail reviews, Claude writes all code.
- 2026-10-03: Windows-only testing for now (MSVC via VS Build Tools). Mac testing deferred until the engine is functional; code stays Mac-ready, and Mac checks are tracked in `docs/MacChecklist.md`.
- 2026-10-03: Windows command-line builds run through `vcvars64.bat` via `scripts/build.cmd` (finds the newest VS install with C++ tools using vswhere, then configures and builds a preset). Rejected a Visual Studio generator preset, because it would mean a second, multi-config build tree, and this machine has a VS 2026 instance without C++ tools that the generator could select. CLion uses the same Ninja presets through its Visual Studio toolchain.
- 2026-10-03: GLM pinned to 1.0.3 (latest release), built header-only (`GLM_BUILD_LIBRARY OFF`). `GLM_FORCE_RADIANS` is a no-op in GLM 1.x but is kept as the brief specifies.
- 2026-10-03: `std::format` is used for logging. The macOS presets set `CMAKE_OSX_DEPLOYMENT_TARGET=13.3`, the minimum for libc++'s floating-point formatting. Unverified on Apple Clang until the first Mac or CI build.
- 2026-10-03: MSVC builds use `/Zc:preprocessor`, PUBLIC on the engine, because `VIVA_ASSERT` uses C++20 `__VA_OPT__`. They also use `/utf-8`.
- 2026-10-03: Plain-data structs (all public, no invariants, e.g. `Version`) use PascalCase fields without the `m_` prefix. `m_` is for members of classes.
- 2026-10-03: The engine version is defined once in `project(VERSION)` and passed to code via compile definitions. It starts at 0.1.0.
- 2026-10-03: A failed `VIVA_ASSERT` logs, then calls `BreakIntoDebuggerOrExit()` (`Platform/Debugger.cpp`): it breaks if a debugger is attached, otherwise exits with `std::_Exit(1)`. Not `std::abort()`, whose MSVC Debug dialog would block CLion runs and scripted runs. `VK_CHECK` (M2) will reuse it.
- 2026-10-03: "No exceptions" means engine code never throws or catches. Compiler exception support stays at the default (`/EHsc`), because the standard library itself may throw.
- 2026-10-03: M0 also prints the Vulkan loader version (`vkEnumerateInstanceVersion`), which proves the loader links and loads at runtime, not just that the headers are found.
- 2026-10-03: Michail's decision: Mac testing is deferred until all milestones are completed (previously: until the engine is functional).
- 2026-10-03: Added the macOS CI workflow (`.github/workflows/macos.yml`), approved by Michail on the condition that it costs nothing. The repo is public, and GitHub's billing docs say Actions minutes are free for public repositories on standard GitHub-hosted runners. `macos-latest` (macOS 26, arm64, M1) is a standard runner; larger runners are always billed. The workflow builds Debug and Release in one job (one 396 MB SDK download per run). It installs Vulkan SDK 1.4.363.0 unattended with LunarG's installer, including `com.lunarg.vulkan.usr` (System Global Installation). It fails on warnings in `engine/`/`sandbox/` and on linker warnings, then runs the Sandbox and checks `--test-assert` exit codes. Open question for M2: whether the runner's virtual GPU can create a Vulkan device. If not, GPU steps stay build-only in CI.
- 2026-10-03: Michail's process change: work through all remaining milestones back to back. After each one, run `/simplify` before the commit, push to `origin/main` so CI checks it, and package a Release executable into `dist/Mx/`.
- 2026-10-03: M12 game: the lane runner (Michail's pick out of lane runner, 3D Asteroids and 3D Breakout).
- 2026-10-03: M11 test models approved: `BoxTextured` (8 KB) and `CesiumMilkTruck` (365 KB) from KhronosGroup/glTF-Sample-Assets, both CC-BY 4.0. The Duck was skipped because its SCEA license terms are unclear.
- 2026-10-03: M10 entity model: GameObjects vs mini ECS vs EnTT explained to Michail, who hasn't decided yet. Default is the hand-written mini ECS unless he says otherwise before M10.
- 2026-10-03: M10 decided: Unity-style GameObject/Component model (Michail's choice, because it matches his Unity experience; he hasn't used ECS). No new dependency.
- 2026-10-03: M0 `/simplify` pass:
  - Dependencies are downloaded as official release archives pinned by SHA-256. The shallow git clones they replace fetched every branch and tag: about 87 MB versus 22 MB.
  - Policy `CMP0168` is set to NEW, so FetchContent skips its sub-builds. A no-op `build.cmd` run went from 4.8 s to 2.1 s. The minimum stays at 3.25, because a `3.25...3.28` range would also turn on C++20 module scanning for every file.
  - SDL's renderer, GPU API and camera are turned off.
  - GLM's defines sit on the `glm` target.
  - Our targets treat warnings as errors (`COMPILE_WARNING_AS_ERROR`), so CI only greps for linker warnings.
  - `Log` formats through `std::format_args` in `Log.cpp`, which cut the compile cost per logging file from about 0.5 s to under 0.1 s in Release. It adds a public `Log::Message(Level, ...)`, and `kMinLevel` is `constexpr`.
  - Asserts use `std::source_location`.
  - Deferred: a precompiled header (revisit when there are more files), and moving the SDL/Vulkan version functions out of the public API (M1).
- 2026-10-03: Naming convention added for state with static storage: `s_Name` (file-local variables in anonymous namespaces, static members). The brief only covered `m_` and `k`.
- 2026-10-03: M1 design:
  - `Key` values are USB HID usage codes, so SDL scancodes convert with a cast and names come from `SDL_GetScancodeName` (`Platform/KeyNames.cpp`).
  - `Input` and `Time` are Unity-style static classes, fed through `friend` access.
  - `Window` is created by a factory function that returns nullptr on failure, and owns `SDL_Init`/`SDL_Quit`.
  - `dt` is capped at 0.25 s, and the loop waits for events while minimized.
  - The version banner moved into `Application::Run()`; the SDL and Vulkan version queries are private headers.
  - The Sandbox's `--quit-after <seconds>` lets CI and scripted tests run it unattended.
  - CI reports the Sandbox's first output lines as `::notice` annotations, which are readable through the public API (job logs need admin auth).
- 2026-10-03: M1 `/simplify` pass:
  - `Input` moved to `Platform/Input.cpp`, as the brief's layout says (`KeyNames.cpp` folded in). Its feed functions are free functions in the private `Platform/InputEvents.h` (`BeginInputFrame`, `ProcessInputEvent`), which replaces the `friend`s in the public header.
  - `static_assert`s check that the `Key` values equal SDL's scancodes.
  - `Window` asks SDL for the minimized flag. `WaitForEvent()` waits without dequeuing, so key presses that arrive with a restore aren't lost.
  - The FPS title moved from the engine into the sandbox via `Application::SetWindowTitle`.
  - `m_QuitRequested` replaces `m_Running`.
  - `Time::DeltaTime()` returns the fixed step inside `OnFixedUpdate` (as in Unity), and `SetFixedDeltaTime` asserts a positive step.
  - CI's run steps get a 2-minute timeout.
  - Skipped: a 1 ms sleep until M3's vsync (documented as a limitation), and `Extent` → `glm::uvec2`.
- 2026-10-03: M2 design:
  - `Renderer` (no `vulkan.h` in its header) is what `Application` owns. `VulkanContext` holds the instance, messenger, surface, device and queues, built by a factory and torn down in reverse order by its destructor.
  - `Window.h` declares `VkInstance_T`/`VkSurfaceKHR_T`, so surface creation needs no `vulkan.h` in `Platform/` and no SDL in `Renderer/`.
  - GPUs are picked by type (discrete > integrated > virtual) among those with Vulkan 1.3, swapchain, dynamicRendering + synchronization2, and graphics + present queues; each rejection is logged.
  - The debug messenger subscribes to WARNING|ERROR. Loader messages ("Loader Message") at Warning are logged as Info with a `[Vulkan loader]` prefix: on this PC, Overwolf's implicit layers (Vulkan 1.2) warn on every run.
- 2026-10-03: Vulkan structs use C++20 designated initializers. Clang's `-Wmissing-designated-field-initializers` (part of `-Wextra`) fires on every omitted field, which was verified with CLion's clang-tidy (LLVM 23), so non-MSVC builds add `-Wno-missing-field-initializers`.
- 2026-10-03: Test harness: test windows are started with `SDL_WINDOW_ACTIVATE_WHEN_SHOWN=0`, so they don't take keyboard focus from Michail while he works. Input is injected with PostMessage, which doesn't need focus.
- 2026-10-03: M2 `/simplify` pass:
  - `VK_CHECK` is fatal in every build (`[[noreturn]]`). The brief only requires that in Debug, but carrying on in Release just crashes later or repeats the error every frame.
  - `VkResultName()` keeps `vk_enum_string_helper.h` in one file.
  - `VulkanContext::kApiVersion` is the single Vulkan version, which VMA and ImGui must also get.
  - GPU selection is split into `CheckDevice()` and `PickPhysicalDevice()`.
  - Validation is selected by a `constexpr bool` instead of `#if`; with `#if`, Clang's Release build failed on an unused function.
  - The portability subset uses the SDK's `VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME` (from `vulkan_beta.h`).
  - CI treats "No suitable GPU found" on the runner as a warning, and checks asserts by log text.
- 2026-10-03: M3 design:
  - `Swapchain` (sRGB format; FIFO, or MAILBOX/IMMEDIATE when `VSync` is off; refuses 0×0) and `FrameResources` (2 frames in flight; "render finished" semaphores per swapchain image, only ever added) are separate classes owned by `Renderer`, declared after `m_Context`.
  - `DrawFrame` = wait, acquire, record (sync2 barriers + dynamic rendering), submit2, present.
  - The swapchain is rebuilt with `vkDeviceWaitIdle` when the window's pixel size differs from the last build's, or on OUT_OF_DATE/SUBOPTIMAL.
  - Synchronization validation is on in Debug via `VK_EXT_layer_settings` (`validate_sync`).
  - `ApplicationSettings::VSync`, and the sandbox's `--no-vsync`.
- 2026-10-03: M3 `/simplify` pass:
  - The loop sleeps when the window has no area (`Extent::IsEmpty`), not only when minimized; a zero-height window spun a core.
  - The swapchain is logged once per build, at Info.
  - The pNext chain for the layer settings hangs off the messenger settings.
  - Members are declared in creation order (context, frames, swapchain).
  - Rebuilds assert the swapchain format is unchanged, because pipelines are built for it.
  - `Extent` has a defaulted `==`, and `std::numbers` replaces magic constants.
  - Deferred to M6: a shared image-barrier helper taking a subresource range (depth aspect, mips).
  - Deferred to M8: `BeginFrame` (fence wait + acquire) should run before input is polled. Today the vsync wait comes after `OnUpdate`, which adds up to one refresh of input latency.
- 2026-10-03: M4 design:
  - Engine shaders live in `shaders/` and compile via `viva_compile_shaders(VivaEngine …)`.
  - The `Pipeline` class covers shader modules, an empty layout, `VkPipelineRenderingCreateInfo`, dynamic viewport and scissor, and no culling.
  - Shaders are loaded from `GetExecutableDirectory()/shaders`; `Platform/FileSystem` uses `SDL_GetBasePath`.
- 2026-10-03: Michail plays games on his main monitor while Claude tests, so `ApplicationSettings::Display` and the sandbox's `--display <n>` were added. Scripted test runs always open on monitor 1 (the second screen, "ROG PG248Q") and never take focus.
- 2026-10-03: M4 `/simplify` pass:
  - `Pipeline::Create(device, PipelineSettings)` takes a settings struct; later milestones add fields with defaults. Callers give shader names, and only `CompiledShaderPath` knows the `shaders/*.spv` layout.
  - `DrawFrame` sets viewport and scissor once, and `RecordDraws()` holds the draw calls (M8 replaces it).
  - `ReadBinaryFile` uses `SDL_LoadFile` with UTF-8 `std::string` paths. `std::filesystem::path::string()` can throw on MSVC for non-ASCII paths, which the no-exceptions engine must avoid.
  - Array counts use `std::size`.
  - Noted for M8: build pipelines from SPIR-V data (spans) rather than paths, and add a `VkPipelineCache` once there are many pipelines.
- 2026-10-03: M5 design:
  - Two commits. `e3f2681` allocates by hand: `FindMemoryType`, one `vkAllocateMemory` per buffer, persistent `vkMapMemory`. The second moves `Buffer` to VMA, so the diff shows exactly what VMA replaces.
  - `Buffer::Create(context, size, usage, MemoryLocation)` takes `Gpu` or `CpuToGpu`. `CpuToGpu` buffers stay mapped, and `Write` is a memcpy plus `vmaFlushAllocation`. `CreateWithData` uploads through a staging buffer: `vkCmdCopyBuffer`, then a buffer barrier to ALL_COMMANDS/MEMORY_READ. That's broad, but it only runs at load time.
  - `VulkanContext::ImmediateSubmit(std::function)` creates a transient pool and a fence per call, submits and waits. Load time only.
  - `Mesh` holds a vertex and an index buffer (uint32 indices) and draws with `vkCmdDrawIndexed`.
  - The `Vertex` layout is `inline constexpr` `kVertexBindings`/`kVertexAttributes` in `Mesh.h`, passed to `PipelineSettings` as spans.
  - The `VertexColor` shaders replace M4's `Triangle` ones.
  - VMA 3.4.0 comes from the GitHub tag archive, pinned by SHA-256, as `SYSTEM`, linked PRIVATE. It uses VMA's default function import, which statically links only Vulkan 1.0–1.3 functions; every 1.3 loader exports those.
  - `VmaAllocator` lives in `VulkanContext`: created after the device, destroyed before it.
  - `VmaImplementation.cpp` routes `VMA_ASSERT` to `VIVA_ASSERT`. VMA's leak check at shutdown then logs and exits like our asserts, which was verified with a deliberate leak.
  - Debug builds log the memory heaps and types (`VkMemoryPropertyNames`), and VMA's statistics after loading.
- 2026-10-03: M5 `/simplify` pass:
  - The hand-written memory-flag name table became `VkMemoryPropertyNames()` in `VulkanCheck.cpp`. The SDK helper is complete and lives in one file.
  - Kept: the explicit memcpy + flush in `Buffer::Write`, which teaches flushing (M6 writes through the mapped pointer every frame), and the fence in `ImmediateSubmit`.
  - Deferred to M6: shared helpers for command pool/buffer, fence and submit. `FrameResources`, `ImmediateSubmit` and `DrawFrame` repeat that code. Put them in the same file as the image-barrier helper.
  - Deferred to M8/M11:
    - Batch uploads into one `ImmediateSubmit`. Today it's one blocking round trip per buffer, two per mesh.
    - Never call `ImmediateSubmit` during gameplay: its fence wait also waits for the frames in flight.
    - Move `Vertex`/`MeshData` to a header without Vulkan once the sandbox builds meshes.
  - Noted for M9: a per-frame memory display should use `vmaGetHeapBudgets`. `vmaCalculateStatistics` is slow and meant for debugging.
- 2026-10-03: M6 design:
  - Conventions: right-handed, Y up, cameras look down −Z, counter-clockwise front faces. That's glTF's convention, for M11; Unity is left-handed with clockwise front faces.
  - `Viva::Camera` is a public plain struct: Position, Yaw, Pitch, FieldOfView, Near and Far.
    - `WorldMatrix()` is translate · rotateY(yaw) · rotateX(pitch). `ViewMatrix()` is its inverse. Forward and Right are columns of the world matrix. M10 will feed the same formula from a Transform.
    - `ProjectionMatrix()` is GLM's OpenGL-style perspective with depth 0..1. The renderer flips y when it writes the uniforms (Unity's `GL.GetGPUProjectionMatrix`), so Camera stays free of Vulkan.
  - `Application::GetCamera()` gives the game the camera, and `Renderer::DrawFrame(const Camera&)` reads it. M8 replaces this.
  - Shader data:
    - Push constants carry the model matrix (64 bytes, vertex stage).
    - `FrameUniforms` holds the view and projection: one uniform buffer and one descriptor set per frame in flight, with its own layout and an exactly-sized pool. It's written right after the frame's fence wait.
  - Depth:
    - `Image` (VMA image + view) holds the depth buffer.
    - `VulkanContext::kDepthFormat = D32_SFLOAT` is required in `CheckDevice`; it's portable, and Apple has no D24S8.
    - The depth image is rebuilt in `RecreateSwapchain` and shared by both frames in flight. The barrier waits on EARLY|LATE_FRAGMENT_TESTS writes.
    - Clear to 1.0, store DONT_CARE, compare LESS_OR_EQUAL.
  - `VulkanHelpers` holds the command pool, command buffer, fence, semaphore and image view creation helpers, plus `TransitionImage(cmd, ImageTransition{...})` with named fields and an aspect.
  - `Input::SetCursorLocked` (Unity's `Cursor.lockState`) uses SDL relative mouse mode. Window registers its `SDL_Window*` through `Platform/InputEvents.h`. SDL only applies it while the window has keyboard focus, so scripted test windows can't grab the cursor.
  - The sandbox's `FlyCamera`: WASD/QE always active, Shift ×3, right mouse button to look, pitch clamped to ±89°. M1's key logging was removed from the sandbox.
- 2026-10-03: Test harness limit found in M6: SDL checks the physical mouse button's state, so posted right-button drags are released at once. Mouse look can't be scripted without moving the real cursor, which is off-limits. It's verified by its math, and Michail tries it.
- 2026-10-03: M6 `/simplify` pass:
  - `CreateImageView` is shared by Swapchain and Image.
  - The Vulkan y flip moved from Camera into the renderer.
  - Camera builds a world matrix instead of `lookAt`, which has no singularity at ±90° pitch.
  - Cursor lock is `Input::SetCursorLocked` rather than an Application method, and FlyCamera owns its whole look behaviour.
  - `AddQuad` holds the counter-clockwise index order for Cube and Floor.
  - The vertex shader multiplies right to left (matrix × vector).
  - Depth compare is LESS_OR_EQUAL.
  - The pillar ring is turned by half a step so the cube is visible from the start position.
  - Deferred to M7:
    - mip ranges in `ImageTransition` and `CreateImageView` (the mips half of the M3 item);
    - descriptor helpers (layout and pool) shared by FrameUniforms and textures.
  - Deferred to M8:
    - Split `Mesh` bind from draw, and sort draws by pipeline, material and mesh.
    - Every pipeline must share the set-0 layout and the identical push constant range, so set 0 stays bound across pipeline switches.
    - Fold the per-frame uniform buffer and set into `FrameData`.
    - Make the deferred-deletion queue a per-frame vector of `unique_ptr`s, not a `std::function` deque.
    - Upload batches with a persistent pool and fence.
- 2026-10-03: M7 design:
  - stb_image is pinned to nothings/stb commit `2c980bb` (v2.30) by SHA-256, as an INTERFACE target `stb::image`.
    - `Core/ImageFile` decodes from memory after `ReadBinaryFile` (`STBI_NO_STDIO`, PNG/JPEG only, `STBI_ASSERT` → `VIVA_ASSERT`) into RGBA8.
  - Assets:
    - `cmake/Assets.cmake`'s `viva_copy_assets(target files...)` copies them with `copy_if_different` to `bin/assets/` and installs them. The Sandbox owns them.
    - `GetAssetPath(relative)` sits next to `GetExecutableDirectory`.
    - The textures are procedural, from `scripts/make-textures.ps1` (Windows/System.Drawing), with the PNGs committed.
  - `Texture`:
    - an `Image` with a full mip chain (`std::bit_width`) in `R8G8B8A8_SRGB`;
    - uploaded through staging, with mips blitted on the GPU: each level goes DST→SRC, then one final barrier moves the whole image to SHADER_READ_ONLY;
    - its own sampler: trilinear, REPEAT, anisotropy at the device maximum when `samplerAnisotropy` is available. The feature is optional, and `VulkanContext::GetMaxSamplerAnisotropy` returns 0 when it's absent.
  - Descriptor set 1 is one combined image sampler. `TextureDescriptors` owns its layout and a fixed pool of 16, and `Allocate(texture)` writes it.
  - `VulkanHelpers` gained descriptor helpers (layout, pool, allocate, write buffer/image), which FrameUniforms uses too.
  - `Vertex` has UV at location 2. The `Unlit` shaders (texture × vertex color) replace the VertexColor ones, and a 1×1 white texture stands for "no texture".
  - The demo content lives in a `DemoScene` struct defined in Renderer.cpp (forward-declared in Renderer.h, which stays free of vulkan.h). M8 removes it.
  - README: each milestone doc describes its own commit.
- 2026-10-03: M7 `/simplify` pass:
  - Descriptor writes go through `WriteUniformBufferDescriptor` and `WriteImageDescriptor`.
  - Mip generation has a single path to SHADER_READ_ONLY.
  - `Image` no longer stores extent and mip count.
  - stb gets a namespaced alias.
  - A comment notes that the spec guarantees linear blits for R8G8B8A8.
  - Deferred to M8:
    - one descriptor allocator that grows and can free, shared by FrameUniforms, textures and materials;
    - the renderer owns samplers, cached by settings once glTF brings per-texture samplers;
    - loading APIs resolve asset names to paths in one place.
  - Deferred to M11:
    - `DecodeImage(span)` split from `LoadImageFile`, for images embedded in .glb files;
    - a color-space parameter (sRGB vs UNORM) when the first data texture arrives.
- 2026-10-03: M8 design (after its `/simplify` pass):
  - Public API:
    - `Viva/Renderer.h`, a pimpl: `Renderer::Impl` in Renderer.cpp holds all the state and logic, and the public functions forward to it.
    - `CreateMesh`, `LoadTexture` (with a weak_ptr cache by name) and `CreateMaterial({ .Texture, .Color })` return `shared_ptr` to the opaque `Mesh`/`Texture`/`Material`.
    - `Submit(mesh, material, transform)` is only valid while a frame is open; it asserts and is skipped otherwise.
    - `BeginFrame`/`EndFrame`/`Create` are private, with `friend class Application`.
    - Also public: `Viva/MeshData.h` and `Viva/Primitives.h` (`Cube`, `Plane`).
  - Deferred deletion:
    - `Impl::Track` gives each `shared_ptr` a deleter that parks the resource (`unique_ptr<GpuResource>`, virtual dtor) in the release list of the most recently begun frame (`m_ReleaseSlot`).
    - `BeginFrame` destroys that list after the slot's fence (`std::exchange` handles cascades).
    - At shutdown, after `vkDeviceWaitIdle`, releases destroy immediately (`m_DestroyNow`), and live resources are logged as an error in every build.
    - The renderer and window are destroyed in `~Application`, after the game's members.
  - Rendering:
    - One `VkPipelineLayout` is owned by the renderer (set 0 camera, set 1 material texture, push constants model + color, 80 B, vertex stage). `PipelineSettings::Layout` is borrowed.
    - Set 0 is bound once per frame. Draws are sorted by shader, material and mesh and bound on change (`Mesh::Bind`/`Draw`).
    - `Material` owns its set 1 from `DescriptorAllocator` (pools of 64 with FREE_DESCRIPTOR_SET, newest first, a new pool when full) and frees it in its destructor.
    - `Shader` is internal (only the default Unlit); public `LoadShader`/`MaterialSettings::Shader` were dropped until a second shader is needed.
  - Main loop:
    - `BeginFrame` (fence + acquire), then `PollEvents`, the updates, `EndFrame`, and finally `BeginInputFrame()`, so input flags survive passes that can't draw.
    - Timed: with vsync the CPU waits 16–23 ms in `vkWaitForFences` versus 0.15 ms in present, so polling input after `BeginFrame` gains a refresh of latency.
    - `Window::IsDrawable()` holds the "minimized or zero area" rule.
  - The sandbox's `DemoScene` owns the scene content.
  - The cubes lost M7's renderer-side gradient (`Primitives::Cube` is white).
- 2026-10-03: M8 deferred items resolved:
  - Done:
    - BeginFrame before input (M3);
    - Mesh bind/draw split and draw sorting, plus the shared layout (M6);
    - per-frame `unique_ptr` deletion lists (M6);
    - the growable, freeable descriptor allocator (M7);
    - asset names resolved inside `LoadTexture` (M7).
  - Kept as is: `FrameUniforms` stays a class instead of folding into `FrameData`, since that would make FrameResources depend on VMA and the allocator.
  - Dropped: SPIR-V spans + `VkPipelineCache` (M4), unneeded with one pipeline.
  - Re-deferred:
    - Upload batching / in-frame uploads → when a game creates resources at runtime. A stress test showed per-frame creation halves the frame rate, because each ImmediateSubmit waits for the frame in flight.
    - A sampler cache → M11 (glTF samplers).
  - `VulkanContext::LogMemoryUsage` was removed (no caller). M9's stats can show `vmaGetHeapBudgets`.
- 2026-10-04: M9 design:
  - **Dependency:** Dear ImGui 1.92.9b (latest release, 2026-07-31), from the GitHub tag archive pinned by SHA-256. It's built as two OBJECT libraries in `cmake/Dependencies.cmake`:
    - `imgui` (core + demo), PUBLIC to the engine, so games include `imgui.h`;
    - `imgui_backends` (`imgui_impl_sdl3` + `imgui_impl_vulkan`), PRIVATE.
  - **Build details:**
    - OBJECT libraries compile with ImGui's own settings, and their objects are archived into `VivaEngine.lib`. That avoids a cycle between two static libraries (`AssertFailed` lives in the engine).
    - They're compiled as C++20; MSVC would default to C++14.
    - `engine/include` is added to `imgui` in a separate, non-SYSTEM call: `SYSTEM` applies to a whole `target_include_directories` call, and it would have hidden warnings in our own headers.
  - **Compile-time config:** `IMGUI_USER_CONFIG="Viva/ImGuiConfig.h"` routes `IM_ASSERT` to `Viva::Detail::AssertFailed` (the function `VIVA_ASSERT` calls), with `std::source_location::current()` for ImGui's file and line. The function is declared again there, so `<format>` stays out of ImGui's files. It's gated by `NDEBUG`, like `assert()`. `IMGUI_DISABLE_OBSOLETE_FUNCTIONS` is on.
  - **Ownership:** `Window` owns the ImGui context and the SDL3 backend. They're created in `Window::Create` after the SDL window and destroyed in `~Window` before `SDL_Quit`. `IniFilename` is null. The style is scaled by `SDL_GetDisplayContentScale`; Retina is handled by ImGui's framebuffer scale.
  - **Input policy:**
    - Every event goes to ImGui first.
    - Presses are held back from `Input` when ImGui wants them: mouse down and wheel on `WantCaptureMouse`, key down on `WantCaptureKeyboard`.
    - Releases always pass; `Input` ignores the release of a key it never saw pressed.
    - `ImGuiConfigFlags_NoMouse` is set while relative mouse mode (cursor lock) is on.
  - **`ImGuiRenderer`** (`Renderer/`) is an RAII wrapper around the Vulkan backend:
    - dynamic rendering with the swapchain format + D32 depth, drawn in the scene's rendering after the scene;
    - a backend-owned descriptor pool (`DescriptorPoolSize`);
    - `MinImageCount = ImageCount = kFramesInFlight`, which the backend uses for its vertex-buffer ring and its texture destroy delay;
    - `CheckVkResultFn` → `VulkanCallFailed` for negative results;
    - `ApiVersion = kApiVersion`, so the backend loads the core `vkCmdBeginRendering`.
  - **sRGB:**
    - A custom fragment shader (`shaders/ImGui.frag`, via `CustomShaderFragCreateInfo`) decodes the vertex colors from sRGB to linear. It's always used: like the rest of the renderer, it assumes an sRGB swapchain, and `ChooseSurfaceFormat` now warns when it has to fall back to another format.
    - `WindowBg` and `PopupBg` are made opaque, because blending into an sRGB target happens in linear space.
    - Rejected: UNORM views of the swapchain images (`VK_KHR_swapchain_mutable_format`) plus a second rendering pass.
  - **Frame:** `Renderer::BeginFrame` calls `ImGui_ImplVulkan_NewFrame`. `Application` calls `Window::NewImGuiFrame` + `ImGui::NewFrame` after a successful `BeginFrame`, and `ImGui::Render` before `EndFrame`.
  - **Public API:**
    - `RenderStats`/`GetStats`: draw calls, triangles, UI draw calls, and VMA statistics summed over the heaps (`vmaGetHeapBudgets` every frame).
    - `SetVSync`/`IsVSync`: a change marks the swapchain outdated.
    - `Pipeline.h` exposes `ReadCompiledShader`.
  - **Sandbox:** `DebugWindows` (Stats + Camera windows, F1). The title-bar FPS was removed; `Application::SetWindowTitle` stays as public API for games.
  - **Test harness:** posted mouse clicks work when `WM_MOUSEMOVE`, `WM_LBUTTONDOWN` and `WM_LBUTTONUP` are posted back to back. The first posted click after startup is lost, so tests start with a warm-up click.
- 2026-10-04: M9 `/simplify` pass:
  - `IM_ASSERT` calls the existing `AssertFailed` (no ImGui-specific failure function), so `Assert.cpp` is unchanged from M8.
  - The ImGui fragment shader is always used, and the non-sRGB branch is gone (see the M9 design entry).
  - Removed two members that only recorded that a step had run, when that step can't fail: `Window::m_ImGuiContext` and `ImGuiRenderer::m_Started`.
  - `FlyCamera::kMaxPitch` is shared with the Camera window's pitch slider. Yaw is wrapped to ±180° in `FlyCamera`, not as a side effect of displaying it.
  - Efficiency review: nothing to change.
    - ImGui's font upload waits for the queue only when glyphs change.
    - `vmaGetHeapBudgets` costs under 1 µs without the memory-budget extension.
    - ImGui adds about 4.5 s of CPU to a clean Debug build.
  - Deferred:
    - **Linear-space blending of translucent UI** (patched for window and popup backgrounds only). Fix when a translucent HUD needs it (M12). Either draw the UI through a UNORM view of the swapchain image (`VK_KHR_swapchain_mutable_format`) in a second rendering pass, or render the scene into an offscreen image that's copied into a UNORM swapchain (the shape post-processing needs).
    - **Re-applying the UI scale** on `SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED`: keep an unscaled style copy, and use `SDL_GetWindowDisplayScale / SDL_GetWindowPixelDensity`.
    - **ImGui's font uploads** use the backend's own staging buffer and `vkQueueWaitIdle`. If a game shows lots of changing text, route `ImDrawData::Textures` through the engine's upload path.
- 2026-10-04: M10 design:
  - **Public API:** `Viva/Scene.h`, `GameObject.h`, `Component.h`, `Transform.h`, `MeshRenderer.h`. `Viva/Camera.h` became a Component. The code lives in a new `engine/src/Scene/` folder (`Camera.cpp` moved there from `Core/`).
  - **Ownership:** `Application` owns a `std::unique_ptr<Scene>`, declared after the renderer so it's destroyed first. The scene holds `std::vector<std::unique_ptr<GameObject>>`. A GameObject holds its `Transform` as a member and `std::vector<std::unique_ptr<Component>>`. Parent/child and component→GameObject links are raw, non-owning pointers.
  - **Components:**
    - `Component` is a base class with protected virtual `OnStart`/`OnUpdate`/`OnFixedUpdate`, called by `GameObject` (a friend), which `Scene` drives (a friend of `GameObject`).
    - `AddComponent<T>(args...)` uses `static_assert` + `make_unique` + perfect forwarding; constructor arguments are allowed, unlike Unity.
    - `GetComponent<T>` is a linear `dynamic_cast` search.
    - `OnStart` runs before a component's first (fixed) update, once its object is active.
  - **Lifecycle:** the update loops are index-based, because callbacks may add objects or components. `SetActive` / `IsActiveInHierarchy` walk up the parents. `Destroy` marks the subtree; `Scene::Render` removes the marked objects (detaching them from surviving parents, then `std::erase_if`) before drawing.
  - **Transform:**
    - Public `LocalPosition`/`LocalRotation` (`glm::quat`)/`LocalScale` fields, and `LocalMatrix` = T·R·S.
    - `WorldMatrix` = parent world × local, recomputed recursively on every call (no caching yet).
    - **Michail's decision (asked during M10's review):** `Forward` = +Z for models and cameras (glTF's asset convention and Unity's meaning), `Up` = +Y, `Right` = −X (right-handed). This revises M6's "cameras look down −Z", which now holds only in view space.
    - `SetParent` keeps the local values (Unity's `SetParent(p, false)`) and asserts against cycles.
  - **Frame order:** scene `FixedUpdate`, game `OnFixedUpdate`; scene `Update`, game `OnUpdate` (LateUpdate-like); scene `Render` (remove destroyed objects, `Renderer::SetCamera` from the first active Camera, `Submit` every active MeshRenderer); `ImGui::Render`; `Renderer::EndFrame()`. With no camera, nothing in the scene is drawn.
  - **Renderer:** `SetCamera(view, projection)` stores matrices; `GetAspectRatio()` comes from the swapchain. The renderer stays unaware of scenes and components.
  - **Camera:** the view is `lookAt(position, position + forward, up)` from the camera's normalized world axes, so a parent's scale doesn't leak in.
  - **Sandbox:**
    - `FlyCamera` is a Component. It derives yaw/pitch from `Forward()` while looking and writes `LocalRotation` = `angleAxis(yaw, Y) · angleAxis(−pitch, X)`, so the Transform is the only state.
    - `Spinner` pre-multiplies a per-frame `angleAxis` step and normalizes.
    - `LoadDemoScene` builds the hierarchy.
    - The DebugWindows Scene window has a hierarchy tree (object addresses as ImGui IDs; the selection is validated against the scene each frame) and an inspector with Active, Destroy, the Transform with Euler-angle rotation, and the known components.
- 2026-10-04: M10 `/simplify` pass:
  - **Destruction:**
    - `~Transform` unlinks both ways, so any destruction order is safe.
    - `RemoveDestroyed` re-marks the subtrees of destroyed objects; this fixes children added under a destroyed object in the same frame, which were left with a dangling parent. It then moves the destroyed objects into a local vector before they're destroyed, so a component destructor (our `OnDestroy`) can create or destroy objects safely. No "something was destroyed" flag.
    - `Run` destroys the scene after `OnShutdown`, while the game and the renderer still exist.
  - **Smaller cleanups:**
    - Update and FixedUpdate share one loop (`ForEachStartedComponent` with a lambda).
    - `kMaxPitch` is private to `FlyCamera.cpp`, and `SetParent`'s loop check uses the assert-then-return pattern.
    - The renderer keeps one `CameraUniforms`.
    - `glm::identity<glm::quat>()` replaces a hand-written (w, x, y, z).
  - **Inspector:** the Euler angles use GLM's YXZ decomposition (Unity's order). Y gets the full ±180°; with GLM's `eulerAngles`, the demo's turns around Y flipped past 90°.
  - **Efficiency review:** nothing to change at M11/M12 scale.
    - World matrices are recomputed per object per frame. If the Stats window ever shows it, use one top-down pass.
    - M12's spawner should reuse meshes and materials kept in game members: creating them blocks (M8).
  - **Deferred to M11:**
    - A glTF mesh has several primitives with their own materials, but only one MeshRenderer per GameObject is drawn. Either MeshRenderer gets a list of mesh+material parts (like Unity's submeshes + `materials[]`), or the loader makes a child object per primitive.
    - `Transform::FindChild(name)` for named nodes such as the truck's wheels.
- 2026-10-04: M11 design:
  - **Dependency:** cgltf 1.15 (latest release, 2025-02-09), from the GitHub tag archive pinned by SHA-256, as an INTERFACE target `cgltf::cgltf` (SYSTEM include) linked PRIVATE to the engine. Its implementation is compiled in `Scene/Model.cpp`, as stb's is in `ImageFile.cpp`.
  - **Assets:** `BoxTextured` as `.gltf` + `.bin` + `.png` (readable JSON; exercises external files) and `CesiumMilkTruck` as `.glb` (exercises embedded images), both unchanged, in `assets/models/` with `CREDITS.md`: CC BY 4.0, © 2017 Cesium, plus the sample repo's notice for the Cesium logo both show (LicenseRef-LegalMark-Cesium: used by permission under Cesium's guidelines, no rights beyond them). The credits are copied with the assets, so they're in every package. `.gitattributes` marks png/jpg/bin/glb as binary.
  - **Public API:** `Viva/Model.h`. `Model::Load(Renderer&, assetName)` returns a `unique_ptr` (null after logging why). `Instantiate(Scene&, parent)` returns a top GameObject named after the file, with the node tree below it: Unity's model prefab and `Instantiate`. Instances share meshes, materials and textures through `shared_ptr`, so the `Model` can go once its instances exist. `Renderer::CreateTexture(w, h, pixels)` is public (procedural textures too).
  - **Loading:** `cgltf_parse_file` + `cgltf_load_buffers` with file callbacks through `ReadBinaryFile` (UTF-8 paths; the bytes are copied into `malloc`'d memory for cgltf's release callback), then `cgltf_validate`. A `unique_ptr<cgltf_data, CgltfDeleter>` owns the result. Files with `extensionsRequired` are refused. Animations and skins are reported (Info) and left out.
  - **Materials:** base color factor × base color texture only, since there's no lighting. One texture per image, made when a material first uses it (the truck's two textures share one 2048² JPEG). Images inside a `.glb`: `DecodeImage(span)` (split from `LoadImageFile`, the M7 deferral) + `CreateTexture`. Image files next to a `.gltf`: `Renderer::LoadTexture(folder + percent-decoded URI)`, sharing the name cache. `data:` URI images aren't supported. A primitive without a material gets a plain white one.
  - **Meshes:** one engine `Mesh` per triangle primitive: POSITION, TEXCOORD_0, COLOR_0 (white without), indices (0..n-1 without). Other primitive modes are skipped with a warning.
  - **The M10 deferrals:** a multi-primitive mesh becomes one `MeshRenderer` with `std::vector<MeshPart> Parts` (a mesh and a material each; the two-argument constructor stays). Unity's submeshes + `materials[]` would need submesh ranges in `Mesh` and in the draw list, for nothing, while each glTF primitive has its own vertices anyway. `Transform::Find(path)` is Unity's: direct children, `/` steps down a level.
  - **Nodes:** a name (else the mesh's name, else "Node N"); TRS (glTF quaternions are x, y, z, w: `glm::quat::wxyz`), or a matrix taken apart (column 3; the column lengths, with the determinant's sign; `quat_cast` of the normalized axes). The top nodes are those of the file's scene (`scene`, else the first).
  - **M7/M8 deferrals:** the color-space parameter waits for the first data texture (only base color is loaded, always sRGB). glTF sampler settings are ignored (both models use repeat + linear), and the sampler cache is dropped: each Texture owns an identical sampler, which is fine at this scale; if anything, the end state is one sampler owned by the renderer.
  - **Sandbox:** 8 BoxTextured instances on the pillars (children of the ring: the pillars' non-uniform scale would stretch them). The truck rides a spinning pivot (3 m/s on a 9.5 m circle). Its axles are found with `Transform::Find` and turned by Spinners at speed/radius around −Y in the file's axes. That direction matches the file's wheel animation, and was checked by measuring the hub's rotation between screenshots. Floor 28×28; camera at (0, 5.5, 15), tilted 18° down.
  - **CI:** the Run Sandbox step turns the log's `[Warn ]`/`[Error]` lines into a warning annotation, and fails on any `[Error]`: a model that didn't load doesn't stop the app.
- 2026-10-04: M11 `/simplify` pass:
  - **One way to a texture:** `Texture::Load` was removed. `Renderer::Impl::LoadTexture` is the name cache + `LoadImageFile` + `CreateTexture`, and the white texture uses `CreateTexture` too. So the size check exists once (it was also an assert in `Texture::Create`), and `Renderer/Texture` no longer knows about image files.
  - **Model:** a node holds its part list directly (no `m_Meshes` + optional index). `TextureFromImage` returns early, with a message for each failure. No fallback for files without a scene: by the glTF spec that's a library, so it gets a warning and empty instances. `GetName()` was dropped (unused).
  - `Transform::Find` tries the next child with the same name when the rest of a path isn't below the first.
  - The sandbox finds the axles with whole paths.
  - **Efficiency review:** nothing to change. The truck loads in ~80 ms in Debug (mostly decoding its JPEG), and `Instantiate` does no GPU work, so M12 can spawn at runtime; it should keep its `Model`s as members (Load has no cache). Skipped: two extra copies in `ReadFile` and `DecodeImage` (milliseconds, at load), creating materials lazily (both models use all theirs), and `STBI_NEON` for stb_image on Apple Silicon (startup only).
  - **Deferred:** mirrored objects (a negative scale, or a mirrored glTF node) are drawn inside out, because the front face is fixed and back faces are culled. Fix when something is mirrored: record the sign of the world matrix's determinant per draw, make the front face dynamic state (`VK_DYNAMIC_STATE_FRONT_FACE`, core in 1.3) and call `vkCmdSetFrontFace` when it changes, like Unity's automatic culling flip.
- 2026-10-04: M12 design:
  - **The game:** Lane Runner, which the Sandbox starts by default. `--demo` keeps the M10/M11 demo scene, which the Mac checklist uses, and CI runs both. Space starts and restarts; A/D or the arrow keys change lanes. Three 3.5 m lanes; the speed rises from 14 to 40 m/s at 0.4 m/s². A row every 28 m, placed 160 m ahead and removed 15 m behind: 1 or 2 crates (one lane always open), and logo boxes (BoxTextured instances) in open lanes 35% of the time. Score = meters + 50 per logo box; the best score lasts the session.
  - **Sandbox components:** `TruckController` (MoveTowards sideways at 12 m/s, the nose steering up to 8°), `TruckWheels` (the model's axle paths, turning direction and wheel radius in one place, also used by DemoScene), `FollowCamera` (offset (0, 6, −10), looking 14 m ahead, x eased), `Treadmill` (snaps to whole texture repeats: road 8 m, grass 4 m), `LaneRunner` (Ready/Driving/Crashed; input in OnUpdate; spawning, collisions as slightly forgiving XZ boxes, and the HUD in OnLateUpdate), `Road.h` (the layout, derived from `kLaneCount`), `Smoothing.h` (`SmoothTowards`). The road and grass textures are made in code with `Renderer::CreateTexture`, so they follow Road.h's numbers; `scripts/make-textures.ps1` stays for M7's PNGs.
  - **Engine additions:** `Component::OnLateUpdate` (a second pass in `Scene::Update`); `Camera::BackgroundColor` → `Renderer::SetClearColor` (the renderer starts black; the old constant became the Camera's default); `Transform::GetRotation`/`LookAt` (built from cross products; GLM's `quatLookAtLH` is the same); `Scene::Contains`; `Primitives::Plane(width, length, repeats = 1)`, which replaces the square version; `Window::Create` adds ImGui's default font explicitly, because ImGui's first font is its default and a game's HUD font would otherwise replace the debug UI's.
  - **HUD:** drawn on ImGui's background draw list (over the scene, under the debug windows) with ProggyForever (`AddFontDefaultVector`) at 18–56 points × FontScaleDpi, each text with a shadow. The debug windows start hidden in the game.
  - **Depth:** the game camera's near plane is 0.5 m, so the grass 5 cm under the road doesn't z-fight at the road's far end (290 m).
- 2026-10-04: M12 `/simplify` pass:
  - **LateUpdate** replaced relying on creation order between GameObjects: the treadmills used to follow before a restart put the truck back at the start, which left it on the grass for a frame.
  - **Shared pieces:** `TruckWheels` (its code was copied between DemoScene and TruckController), `Road.h` (three lanes were hard-coded in four places), `SmoothTowards` (two copies).
  - **`Scene::Contains`:** LaneRunner forgets items destroyed with the inspector, which it would otherwise read after they were freed; DebugWindows uses it for its selection. Destroying the truck or the camera that way still breaks the game (documented).
  - **Smaller:** one `Plane` (the two-float overload read like a rectangle but made a square); collision boxes derived from the item sizes, with pickups deliberately generous; `UpdateItems` flatter, and stopping after a crash; one `ResetRun`; HUD text as `string_view` (no allocations per frame); the window title from `ApplicationSettings`; `FollowCamera::OnStart` only places the camera; CI skips both modes when the runner has no GPU (`continue 2`).
  - **Efficiency review:** nothing measurable. Noted: ImGui uploads new glyphs through its backend's own staging buffer and `vkQueueWaitIdle` (a few one-off stalls at the start and at the first crash; the M9 deferral covers it), and `GetPosition` builds a whole world matrix (fine at this scale).
- 2026-10-04: Projects and the editor planned as M13–M18 (`docs/EditorRoadmap.md`), at Michail's request: create, list, open and delete projects, and place primitives in a scene by hand. Michail's choices:
  - **Projects are data only at first:** a folder with a `.vivaproject` file, `Assets/` and `Scenes/`, built from the engine's components. A project's own C++ code (a DLL with hot reload) comes after M18.
  - **One `VivaEditor` executable** (new `editor/` target) starts in the Project Manager; the recent-projects list lives in `SDL_GetPrefPath`. The Sandbox stays as the code-only example.
  - **JSON is hand-written** (`Core/Json`, no exceptions), not nlohmann/json or yyjson.
  - **ImGui switches to its docking build** (`v1.92.9b-docking`) in M15, for a Unity-like dockable layout.
  - **Gizmos are hand-written** (M17), not ImGuizmo.
- 2026-10-04: **macOS dropped** (Michail's decision): Windows is the only platform. New code needn't be Mac-ready, `.github/workflows/macos.yml` was deleted (with the README badge), and `docs/MacChecklist.md` is archived as it stood after M12. Existing Mac code and the `macos-*` presets stay while they cost nothing. Earlier decision-log entries and milestone docs that mention the Mac are left as history.
- 2026-10-04: M13 design:
  - **JSON:** `Core/Json` (private): a `std::variant` value with order-keeping objects (vector of pairs), a recursive-descent parser that reports the first error by line and column, and a writer that indents two spaces and keeps number arrays on one line. Numbers go through `from_chars`/`to_chars`; floats through `FloatToJsonNumber` (the shortest exact text), so files say `0.1` and round trips are exact.
  - **Fields:** `Viva/FieldVisitor.h`. Components override `VisitFields(FieldVisitor&)` (public, virtual, empty by default); `Transform` has a non-virtual one; `VisitFields(FieldVisitor&, MaterialSettings&)` is a free function. Overloads for float, vec2, vec3, vec4, quat, and shared_ptr to Mesh, Texture and Material; lists via a `List(name, vector, lambda)` template on protected Begin/End virtuals. Missing fields keep their values, unknown ones are ignored, wrong types warn. The M16 Inspector is meant to be a third visitor.
  - **Registry:** `Viva/ComponentRegistry.h`, a static class. `Register<T>(name)` stores the name, `typeid(T)` and `&Create<T>`; the engine's MeshRenderer, Camera and Spinner are registered on first use. Explicit registration, because self-registering globals in a static library can be dropped by the linker.
  - **Assets:** `Viva/Assets.h`, owned by `Application` (`GetAssets()`), created after the renderer and destroyed before it. Names: `Primitives::Cube|Plane|Sphere|Cylinder`, file paths, and `<model>#mesh<i>/<p>` / `<model>#image<i>` for what a model holds. Primitives, textures and materials are cached as `weak_ptr`s; models as `shared_ptr`s until the application ends. `GetMaterial(settings)` shares a live material with equal settings (`MaterialSettings` has a defaulted `operator==`). `Renderer::LoadTexture` moved here (`GetTexture`), and `Model::Load` became private (`Assets::GetModel`), which closes the M7 deferral "resolve asset names in one place".
  - **Names on resources:** `CreateMesh`/`CreateTexture` take an optional asset name, stored on the internal `Mesh`/`Texture`; `Renderer::GetAssetName` and `GetSettings(Material)` are static readers. `Material` keeps its `MaterialSettings` as given (texture may be null) next to the texture it binds (white for null).
  - **Scene files:** `Scene::Save(path)` / `Scene::Load(path, assets)` in `Scene/SceneFile.cpp` (Scene members in a second .cpp). Format `{"Format": "VivaEngine Scene", "Version": 1, "GameObjects": [...]}`, children nested, empty `Components`/`Children` omitted, quaternions as x, y, z, w. Load is additive and refuses newer versions. Unregistered components, and meshes and textures made in code, are skipped on save with a warning each. Rotations are normalized on load only when visibly off length 1, so save → load → save is byte-identical. `WriteTextFile` (`SDL_SaveFile`) joins `ReadBinaryFile`. Public `GameObject::GetComponents()` and `IsDestroyed()`.
  - **Rendering:** `MaterialSettings::Tiling`/`Offset`, pushed as a vec4 after the color (push constants 80 → 96 bytes), applied in `Unlit.vert`.
  - **Primitives:** `Sphere` (radius 0.5, 32 × 16, seam column, degenerate pole triangles) and `Cylinder` (radius 0.5, height 2, separate cap vertices), as grids via `AddGrid`. `Plane()` is always 10 × 10 (`kPlaneSize`), like Unity's.
  - **Sandbox:** `Spinner` moved into the engine. `--save <file>` saves right after the scene is built; `--load <file>` shows a file instead. FlyCamera and TruckWheels are registered in `OnStart` and have fields. The demo uses only assets (pillars are cubes with eight colored materials; the floor is the scaled plane, tiled 14×). `assets/scenes/Demo.scene` (written by `--demo --save`) and `Shapes.scene` (hand-written) are copied as assets.
- 2026-10-04: M13 `/simplify` pass:
  - **Material fields listed once:** the JSON writer and reader both spelled out Texture/Color/Tiling/Offset. Added `vec2` and `Texture` overloads and `VisitFields(FieldVisitor&, MaterialSettings&)`; each visitor's material overload only opens the nested object.
  - **One way to tile:** `Primitives::Plane` lost its size and repeat parameters; the Lane Runner's road and grass are the shared plane asset, scaled, with tiled materials. `kPlaneSize` replaces a magic 10.
  - **One factory:** the engine's components register through `Register<T>` (`RegisterEngineComponents`, guarded by a plain flag) instead of a second copy of the factory lambda.
  - **TruckWheels turns the axles itself** instead of adding Spinners in `OnStart`, which a scene saved afterwards would hold and a reload would double.
  - `JsonFieldWriter` lost its derivable list stack; float reading moved out of `ReadFloats`.
  - Skipped: registering model sub-assets in Assets' caches instead of `Model::FindMesh`/`FindTexture` (load-time only, and it moves code more than it removes); hiding the asset-name parameter from the public Renderer API (the alternatives need friends or internal headers); saving model instances as prefab references (a known limitation, for later).
  - Noted for M14+: models stay loaded until exit, which matters once the editor switches scenes (add something like Unity's `Resources.UnloadUnusedAssets`). The sandbox inspector still lists fields by hand until M16's visitor replaces it.
- 2026-10-04: M14 design:
  - **Projects:** `Viva/Project.h` (Core): a folder with `<Name>.vivaproject` (JSON: Format "VivaEngine Project", Version 1, Name, EngineVersion, StartupScene), `Assets/` and `Scenes/Main.scene`. `Create` writes a template scene from a raw string literal (camera, floor, spinning cube; colors only, since Assets starts empty) and removes the half-made folder on failure; `Open` warns about another engine version but opens; `CheckNewProject` (name rules incl. Windows' reserved names, location, folder exists) is shared by `Create` and the UI. `Project` is a plain value.
  - **Public file system:** `Platform/FileSystem.h` became `Viva/FileSystem.h` (the editor needs it), built on SDL (UTF-8, no exceptions): read/write, `PathExists`, `IsFolder`, `CreateFolder`, `ListFolder` (C callback + userdata), recursive `DeleteFolder` (permanent), `GetUserDataFolder` (`SDL_GetPrefPath`), `GetDocumentsFolder`, `NormalizePath`. `GetAssetPath` is gone: `Assets` takes a root folder and has `GetFilePath`.
  - **Folder dialog:** `ShowFolderDialog(title, start, onChosen)` via `SDL_ShowFileDialogWithProperties`, parented to the engine's window (`SDL_GetWindows`). SDL may call back on another thread, so `Platform/FolderDialog.cpp` queues results under a `std::mutex`, and `Window::PollEvents` delivers them on the main thread.
  - **Json public:** `Viva/Json.h`, plus `ReadJsonFile(path, format, version, error)` for every engine file (scenes, projects, the project list).
  - **Application:** `SetAssetsFolder(folder)` (a fresh `Assets`); `ApplicationSettings::QuitAfter` and `ReadCommandLine` (`--display`, `--no-vsync`, `--quit-after`), shared by the Sandbox and the editor. `Scene::Clear()` marks everything destroyed (Clear + Load = Unity's single-mode load).
  - **FlyCamera** moved into the engine and is a built-in registered component (both programs use it).
  - **VivaEditor** (`editor/`, an Application): `ProjectList` (`projects.json` in `GetUserDataFolder("Viva", "VivaEditor")`: entries with Name, Folder, LastOpened in Unix seconds, newest first, `Missing` refreshed on load and close; plus `NewProjectLocation`), `ProjectManager` (full-window ImGui: table with Open / Remove / Delete..., New Project popup with a unique suggested name and Browse..., Delete confirmation naming the folder and re-checking for the project file, Problem popup), `EditorApp` (opening: Clear, SetAssetsFolder, Load the startup scene, add a FlyCamera to the main camera; a small Project panel with Close Project). `--open <folder>` and `--settings <folder>` for tests.
  - **Test harness:** ImGui text fields ignore posted typing without keyboard focus, which test windows never take, so typing and the OS folder dialog are manual checks; the remembered location and suggested names let scripted tests create projects with clicks alone.
- 2026-10-04: M14 `/simplify` pass:
  - `ReadJsonFile` replaced three copies of read + parse and two of the Format/Version check (scenes, projects; the project list reads without a header).
  - `Application::SetAssetsFolder` replaced the editor's own per-project `Assets` member, so `GetAssets()` is right in every program (M18's player will need the same).
  - `QuitAfter` and the common flags moved into `ApplicationSettings`/`Application::Run`, out of both `main`s.
  - `Project::CheckNewProject` is shared by `Create` and the popup, which re-checks only when a field changes (it touched the disk every frame).
  - Removed: the unused `GetFileName` and `Project::GetEngineVersion` (it hid the free function); the New Project popup's open flag; duplicated error handling around `OpenProject`.
  - Skipped: making the folder dialog an Application service (documented where its callbacks run instead); building the template scene through the scene writer (needs the renderer); moving FlyCamera's registration out of the engine (M15 replaces the M14 stopgap that adds it to project scenes; it must not be saved into them in M16); idle redraw while the Project Manager sits still.
- 2026-10-04: Michail checked M14's text fields, Browse... and Add Existing... by hand: all work.
- 2026-10-04: M15 design:
  - **Dependency:** ImGui's `v1.92.9b-docking` tag archive, pinned by SHA-256. Its Vulkan backend's `ImGui_ImplVulkan_AddTexture(view, layout)` uses its own sampler.
  - **Scene target:** `Renderer::SetSceneTargetSize(w, h)` (called each frame the image is shown; 0 x 0 = draw into the window) and `GetSceneTexture()` (ImTextureID as `uint64_t`). `SceneTarget` (a `GpuResource`: color image in the swapchain format with COLOR_ATTACHMENT | SAMPLED, its own D32 depth, ImGui descriptor set) is rebuilt in `BeginFrame` after the fence wait, the old one parked in that frame's release list (no device wait). `EndFrame` draws the scene into it (UNDEFINED→COLOR_ATTACHMENT after the last frame's fragment reads; then →SHADER_READ_ONLY for fragment sampling), then a UI-only rendering into the swapchain (still with a depth attachment, because ImGui's pipeline has one), through one `RecordRendering`. A target not requested this frame isn't drawn. `GetAspectRatio` follows the target.
  - **Application:** `OnRender()` after `Scene::Render` (the editor sets its camera there); `OnQuitRequested()` (false keeps running; `Window::TakeCloseRequest` reports each close once); `SetSceneUpdating(bool)` (Edit mode skips the scene's Update/FixedUpdate, still renders). `Scene::Render` submits meshes even without a main camera.
  - **Engine:** `Fly(position, rotation, dt, FlySettings)` extracted from FlyCamera (`FlySettings::MoveOnlyWhileLooking` for the Scene view; FlyCamera holds a `FlySettings Settings`); `Transform::kForwardAxis/kUpAxis/kRightAxis`; `Log::SetListener` (one listener, called under Log's mutex with level, seconds and the message); `ShowOpenFileDialog`/`ShowSaveFileDialog` beside `ShowFolderDialog` (`Platform/FileDialog.cpp`, filters kept alive in the request); `Project::GetScenesFolder`, `Scene::kFileExtension`, `GetFileStem`.
  - **Editor windows** (`editor/src`): `SceneView` (ImGui::Image of the target; `SetNextFrameWantCaptureMouse/Keyboard(false)` while hovered or flying so `Input` sees the camera's input; `EditorCamera` with Fly, Frame, its own view/projection), `HierarchyWindow`, `InspectorWindow` (`InspectorVisitor`: Drag/Color widgets, quats as YXZ Euler degrees, asset names read-only, materials as nested settings that ask `Assets` for the material with new settings, lists as tree nodes with an open-stack), `ProjectWindow` (Assets and Scenes trees read on open/Refresh/save; double-click opens a scene), `ConsoleWindow` (listener → incoming list under a mutex → drawn list; `ImGuiListClipper`; deque capped at 2000).
  - **EditorApp:** docking (`DockSpaceOverViewport`; DockBuilder default layout on first run and Window > Reset Layout); layout saved in the user's settings folder (`layout.ini`, loaded in OnStart before the first frame; per user, a deviation from the roadmap's "per project", because ImGui applies saved layouts only before its windows exist); File menu + Ctrl+N/O/S/Shift+S via `ImGui::Shortcut`; Window menu; `AskToSaveThen(action)` with a Save / Don't Save / Cancel modal (Save As first for a never-saved scene); title `VivaEditor - Project - Scene*`; Edit mode always (`SetSceneUpdating(false)`); F frames the selection (read through `Input`). The M14 FlyCamera stopgap is gone.
  - **Test harness:** ImGui ignores posted keys without keyboard focus (shortcuts, text fields, its own key queries), so editor keys the tests need go through `Input`; docking tabs can't be clicked by posted messages (they select on release, after the posted WM_MOUSELEAVE), so tests switch tabs through the Window menu.
- 2026-10-04: M15 `/simplify` pass:
  - **Renderer:** scene target replacement through the release lists instead of `vkDeviceWaitIdle`; no scene pass while the Scene view is hidden; `CreateDepthImage` shared with the swapchain's.
  - **Log:** the message is formatted straight onto the line again; the listener gets a view of it (no extra copy).
  - **Console:** `ImGuiListClipper` over the filtered lines; a deque; colors without a sentinel.
  - **Smaller:** `CenterNextWindow` in `editor/src/EditorUi.h`; `SceneReplaced` for the per-scene reset; `BuildDefaultLayout` a free function; the Inspector returns its changed flag; `FlyCamera::Settings`; scene folder/extension/stem helpers; `Scene::Render` submits without a camera.
  - Deferred, with the milestone that needs them: render targets as objects with their own cameras (M18's Game view); one input path for editor commands with focus-scoped `ImGui::Shortcut`s and the Scene view as an ImGui item for clicks and gizmo drags (M16/M17); edits reported as begin/commit to one place for undo (M17); stable GameObject IDs for selection, undo and Play mode (M17/M18). Skipped: the Sandbox's DebugWindows duplicate the editor's hierarchy and Euler code (separate programs; the debug windows stay small).
- 2026-10-04: Michail checked M15 by hand (mouse look and flying, dragging values and tabs, the shortcuts, text fields, the file dialogs, layout persistence): all work.
- 2026-10-04: M16 design:
  - **One door for edits:** `SceneEditor` (editor) makes every structural change (create, duplicate, delete, rename, reparent, add/remove component) and owns the selection and the dirty flag; windows only ask. M17's undo records there. It reads the scene and assets from the Application each time, so `Application::GetRenderer/GetAssets/GetScene` became public (`SetAssetsFolder` replaces the Assets object).
  - **Menus as tables:** `EditorMenus.cpp` has one table of new-object kinds (name, primitive mesh) feeding the GameObject menu, the Hierarchy's right-click menu and `SceneEditor::Create`, and one table of Edit commands (label, shortcut text, key, Ctrl) feeding the Edit menu, the right-click menu and the keyboard. All Edit keys (F, F2, Del, Ctrl+D) go through `Input` (ignored while a text field has the keyboard; posted keys reach it, so tests can press them) and only while the Hierarchy or the Scene view is focused or hovered. The File menu's Ctrl shortcuts stay `ImGui::Shortcut`s. Commands run after the windows have drawn (`EditorApp::RunEditCommand`).
  - **Picking:** `Bounds` (min/max, `Corner(i)`) computed per mesh at creation (`ComputeBounds`, `Renderer::GetBounds`); a ray through the mouse from the inverse view-projection; the slab test in the object's space (inverse world matrix, unnormalized direction so distances compare). `ForEachDrawnBox` (active, not destroyed, optionally the children) is shared by picking, F's world bounds and the outline. Boxes, not triangles (Unity tests triangles).
  - **Selection outline:** the oriented bounding boxes of the selection and its children, drawn with ImGui's window draw list over the image, lines clipped at the near plane in clip space; M17's gizmos will use the same overlay.
  - **Grid:** `Renderer::DrawGrid()` (one frame, like Submit), a `Grid` shader pair: the vertex shader builds a 2000-unit square under the camera from `gl_VertexIndex`; the fragment shader draws 1- and 10-unit lines with `fwidth` (constant pixel width, fading where cells get small), red X and blue Z axes, and a distance fade. New `PipelineSettings`: `AlphaBlend`, `DepthWrite`, `DepthBias` (constant and slope); the grid uses blending, no depth write, bias -1, no culling, after the opaque draws. No `discard`: glslc emits `OpDemoteToHelperInvocation` for it, whose 1.3 feature isn't enabled (validation error).
  - **Engine:** `Scene::Instantiate(original, assets, parent)` copies through the scene-file JSON in memory (Unity's Instantiate serializes too; unregistered components and code-made meshes are left out). `Scene::Destroy(Component&)` marks; `RemoveDestroyed` removes marked components of surviving objects (`GameObject::RemoveDestroyedComponents`), and marked components neither start nor update. `Transform::SetParent(parent, keepWorldPose)` (local = inverse(parent world) × world), `CanSetParent`, `IsBelow`, and `DecomposeMatrix` (moved from Model.cpp). `FieldVisitor::Angle` (default calls `Field`; the Inspector shows degrees; Camera's FieldOfView uses it, files unchanged). `ComponentRegistry::GetNames()`, `Assets::GetPrimitiveNames()` (one `{ name, function }` table in Assets.cpp), `GetFileExtension` (lower case).
  - **Editor details:** new objects 8 units in front of the editor camera (children at their parent's origin), with the shared white material; duplicates named "Cube (1)" from the base name; the Hierarchy defers structural edits until its tree is drawn, reveals a new selection (opens its parents, scrolls), renames the selection in place (Enter/click away keeps, Escape cancels; `SceneEditor::Rename` ignores empty and unchanged names), reparents by ImGui drag and drop (targets only where `CanSetParent` allows; the empty space is an `InvisibleButton`); the Inspector has a name field, combo pickers (primitives; the project's png/jpg files), a Size field for lists, Create Material, Add Component, and Remove Component on the header's right-click menu; the Project window places .gltf/.glb models on double-click.
  - **Test harness:** Delete must be posted with the extended-key flag (bit 24), or SDL reads the numpad's period; a menu item's click is sometimes lost after an idle pause (posted input makes SDL see the mouse leave), so tests check each step and retry; hover-only submenus don't open (click them); drags can't be posted.
- 2026-10-04: M16 `/simplify` pass:
  - **Shared:** `Bounds::Corner` and `ForEachDrawnBox` (the corner and part walks were written three times, with different rules for inactive objects); `CopyToBuffer` moved to `EditorUi.h` (three copies); `GetFileExtension` in FileSystem (scene files were matched case-sensitively, images and models not); `Transform::CanSetParent` (the loop rule was written twice); the primitive and menu tables (the primitive names were spelled out in about five places).
  - **Altitude:** one Edit-command path for menus, right-click and keys, focus-scoped (Del in another window no longer deletes); the rename rule moved into `SceneEditor`; renaming is a flag on the selection (the Hierarchy no longer keeps its own GameObject pointer, so `SceneEditor::Update` is the only liveness check); `SceneEditor` no longer caches an `Assets*` that had to be re-handed after every `SetAssetsFolder`.
  - **Smaller:** the grid's square is placed by its vertex shader (no model matrix or push constants); `SetParent` computes the world matrix only when keeping the world pose; `LoadGameObject` lost its counter out-parameter (the load log counts by the list's growth); unused parameters, members and includes.
  - Skipped: sizing the grid's square from its fade distance; per-frame string/vector allocations and recursive world matrices in the outline (not measurable at this scale); `Assets::GetAssetName(path)` (the Project window's own prefix check is enough until assets move); ImGui's `imgui_stdlib` `std::string` InputText (a CMake change to the dependency setup).
  - Noted: without lighting, white primitives show no shape; asked Michail whether to bring simple lighting forward.
- 2026-10-04: Lighting stays where the roadmap has it (Michail's decision, asked after M16): after M18, with the other "Beyond M18" items. Until then primitives are unlit, and colors tell them apart.
- 2026-10-04: M17 design:
  - **Undo by snapshots** (the memento pattern), not commands: after each finished edit `SceneEditor` keeps the whole scene as JSON text (`WriteJson(Scene::Serialize(true))`), restored with `Clear` + `Deserialize`; 100 steps; Undo/Redo move states between two stacks, a new edit clears Redo; each state has a step number, and `IsDirty` compares it with the saved one (undoing back to the saved state is clean). Commands would need reversing code per edit; snapshots reuse M13's files and M18's Play mode needs the same.
  - **Step boundary:** edits call `MarkChanged()` (structural edits before they change the selection, so the step keeps the selection before it); `EndFrame(ImGui::IsAnyItemActive())` ends the step once nothing is held, so one value drag, one gizmo drag or one typed name is one step. A snapshot equal to the previous one is dropped. Undo/Redo first finish an edit in progress. Keys aren't read while a widget is held.
  - **Ids:** `GameObject::GetId()` from a scene counter (Unity's instance ID), `Scene::FindById`. `Serialize(withIds)` writes them for snapshots only (files stay without, loaded and instantiated objects get fresh ones); `Deserialize` restores them (`Scene::LoadGameObject` became a private member to set them). The selection is kept as an Id, and the Hierarchy's ImGui IDs are object Ids (resolves the M15 deferral "stable GameObject IDs").
  - **Gizmos** (`editor/src/Gizmo.*`), hand-written, drawn with the draw list over the image: per frame `MakeFrame` (center, axes: object rotation for Local and always for Scale, world for Global; size = 100 px via `SceneViewport::PixelsToWorld`), `Project` (axis tips, Move's plane squares at 0.2–0.45 of the size, Rotate's rings as 64 segments with a front/back flag at cosine -0.3), then `HitTest` and `Draw` on that one projection. Drags start from saved values: Move axis = closest point between mouse ray and axis line; Move plane = ray-plane intersection; Rotate = drag along the screen projection of A × r at the grabbed point, one ring radius = one radian; Scale = mouse travel along the axis on screen / handle length; center = uniform. Nothing changes until the mouse moves. Ctrl snaps moves to 1 unit and turns to 15 degrees (relative to the drag's start). Writes through the new `Transform::SetPosition/SetRotation` (world space).
  - **Scene view:** the image is an `InvisibleButton` (left button) with the texture drawn by the draw list: press/held/release come from ImGui (resolves the M15 deferral "Scene view as an ImGui item"). A toolbar row (Move/Rotate/Scale, Local/Global) from one tool table shared with W/E/R; X toggles; tool keys only while the Hierarchy or Scene view has focus and not while flying. `SceneViewport` (image rect, view, projection, camera position: `ToScreen`, `RayThrough`, `PixelsToWorld`, clipped `DrawLine`) is shared by the outline, picking and the gizmo.
  - **Edit menu:** Undo (Ctrl+Z) and Redo (Ctrl+Y) join the command table; they work from any window (the selection commands only from the Hierarchy/Scene view), are left out of the Hierarchy's right-click menu, and the menu greys them out when there's nothing to do. Edit menu commands run after the windows have drawn, like the keys.
  - **Engine:** `Scene::Serialize/Deserialize` (Save/Load wrap them); `SaveGameObject` skips components destroyed this frame.
- 2026-10-04: M17 `/simplify` pass:
  - **Stable Ids instead of tree positions:** the first version found the selection by its path in the hierarchy and gave Hierarchy lines path-based IDs, which moved branches' open state onto other lines after a delete. Ids fixed both and removed `PathOf`/`FindByPath`.
  - **Gizmo:** the handles are projected once per frame (`ScreenHandles`) for hit-testing and drawing (the ring segments, plane corners and axis tips were computed twice); the ring drag follows the grabbed point's tangent instead of adding up screen angles around the center (touchy for rings seen nearly edge-on); a held handle that hasn't moved changes nothing (it made no-op undo steps); the screen center is part of the frame; `kMinAxisPixels` names the hidden-axis threshold.
  - **Undo:** snapshots are text, not `Json` trees (about 20x smaller); identical snapshots aren't steps; Redo finishes a pending edit like Undo does.
  - **Smaller:** Edit menu commands deferred until after the windows; one switch in `RunEditCommand`; `IsCtrlHeld()` in `EditorUi.h`; one tool table for toolbar labels and keys.
  - Skipped: caching the view-projection matrix in `SceneViewport` (a few matrix products per frame); a deque for the history (moves of 100 strings).
