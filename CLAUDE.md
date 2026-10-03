# Project brief

## Who you're working with

- **Michail**: an experienced Unity/C# game developer with a game shipped on Steam. He has **not written C++ in years**.
- **Goal: learn how a game engine works by building one.** Understanding matters more than speed or features.
- **Michail writes no code. You write all of it.** He reviews after each milestone, builds and runs it in CLion, tests on Windows and Mac, and asks questions.
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
| Compilers | MSVC on Windows, Apple Clang on macOS |
| IDE (Michail's) | CLion on both platforms; the project must open cleanly through the presets |
| Platform layer | **SDL3**, pinned to tag `release-3.4.18`, built as a **static** library (`SDL_STATIC ON`, `SDL_SHARED OFF`) |
| Graphics | **Vulkan, plain C API** (`vulkan.h`), loader and headers from the LunarG Vulkan SDK via `find_package(Vulkan REQUIRED COMPONENTS glslc)` |
| Vulkan version | Target **Vulkan 1.3 core**: dynamic rendering and synchronization2. **No `VkRenderPass` or `VkFramebuffer`.** |
| Shaders | GLSL in `shaders/`, compiled to SPIR-V at build time with `glslc` (CMake custom command). `.spv` files are copied next to the executable. |
| Math | GLM (FetchContent) with `GLM_FORCE_DEPTH_ZERO_TO_ONE` and `GLM_FORCE_RADIANS` |

**Approved for later milestones** (add each only when its milestone arrives):
- Vulkan Memory Allocator (VMA)
- stb_image
- Dear ImGui (SDL3 + Vulkan backends)
- cgltf

**Anything else needs Michail's approval first.**

**Do not use** vk-bootstrap, vulkan.hpp/RAII wrappers, or any engine framework. Writing the instance, device and swapchain setup by hand *is* the learning.

**References:**
- Primary: vkguide.dev (Vulkan 1.3, dynamic rendering, SDL)
- Secondary: the docs.vulkan.org tutorial and the Vulkan spec

## Platforms

- **Windows x64** with an NVIDIA RTX 3080 Ti. This is the main dev machine and the **only platform tested for now**. The toolchain is MSVC from Visual Studio Build Tools (no VS IDE), with CMake, Ninja, Git, the Vulkan SDK, CLion and RenderDoc installed and verified.
- **macOS on an M1 Pro** (Apple Silicon). This is a **future target**. Michail won't test on the Mac until **all milestones are completed**, so:
  - All code must stay Mac-ready, following the rules below, and the macOS presets must exist.
  - Never block a milestone on Mac verification.
  - Keep a running list of things to check on the Mac in `docs/MacChecklist.md`, adding to it whenever a milestone adds platform-sensitive code (surface, swapchain, portability, Retina sizing, file paths).
  - The GitHub Actions workflow `.github/workflows/macos.yml` builds Debug and Release with Apple Clang and runs the Sandbox on every push to `main`. It's the early warning for Mac problems, so keep it green: a milestone that breaks it isn't done. It must stay free: only standard runners (`macos-latest`), never `-large`/`-xlarge` labels, and no artifacts or caches.

### Vulkan on macOS requirements

- Vulkan runs through a translation driver from the LunarG SDK. Code must work with both:
  - **MoltenVK**
  - **KosmicKrisp** (Vulkan 1.4 conformant, requires macOS 26+)
- **Instance:** if `VK_KHR_portability_enumeration` is available, enable it and set `VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR`.
- **Device:** if `VK_KHR_portability_subset` appears in the device's extension list, it **must** be enabled.
- **Retina:** create the window with `SDL_WINDOW_HIGH_PIXEL_DENSITY`, and size the swapchain from `SDL_GetWindowSizeInPixels`, never from the window size in points.
- **Validation:** GPU-assisted validation and debug printf only work with KosmicKrisp, not MoltenVK. Don't rely on them.
- **Finding the SDK:** if CMake can't find Vulkan on the Mac, the SDK probably wasn't installed with "System Global Installation" or `setup-env.sh` wasn't sourced. Explain the fix and document it in the README.

**Isolation rules:**
- Platform `#ifdef`s live only in `engine/src/Platform/`.
- SDL headers are included only in `Platform/`.
- Vulkan headers are included only in `Renderer/`.

## Repository layout

```
VivaEngine/
├── .github/workflows/      # macos.yml: free macOS CI build on every push to main
├── CLAUDE.md
├── README.md               # how to build/run on Windows and macOS
├── CMakeLists.txt
├── CMakePresets.json
├── cmake/                  # helper scripts (dependencies, shader compilation)
├── engine/                 # static library target: VivaEngine
│   ├── include/Viva/       # public headers (what the game sees)
│   └── src/
│       ├── Core/           # Application, Log, Assert, Time
│       ├── Platform/       # Window, Input (SDL3 lives only here)
│       └── Renderer/       # all Vulkan code
├── sandbox/                # executable target: test app / game using the engine
├── shaders/                # GLSL sources
├── assets/
├── scripts/                # build.cmd: Windows command-line build through vcvars64.bat
└── docs/
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
  - Verify Apple Clang supports it; set `CMAKE_OSX_DEPLOYMENT_TARGET` as needed.
  - If `std::format` doesn't work there, fall back to `fmt` via FetchContent and record that in the decision log.

**Build settings:**
- Warnings: `/W4` on MSVC, `-Wall -Wextra` on Clang.
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
     - which platform it was verified on (Windows for now), with any new Mac checks also appended to `docs/MacChecklist.md`
   - The **Status** section of `CLAUDE.md` is updated.
   - There is **one git commit**, e.g. `M3: Swapchain and clear screen`. Push it to `origin/main` right away (Michail approved pushing each milestone on 2026-10-03), and check that its macOS CI run is green before committing the next milestone. Never force-push, and push nothing else unless Michail asks.
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

## Toolchain check (first session, on whichever machine you're on)

Run these and report the results:
- `cmake --version` (needs ≥ 3.25)
- `ninja --version`
- the compiler version (`cl` / `clang --version`)
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

**Beyond M12** (decide together): lighting and PBR, shadows, audio via SDL3, physics (Jolt), scene serialization, asset pipeline, shader hot reload, multithreaded rendering.

## Status

- **Current milestone:** M2 (Vulkan instance, validation, device) is next. Done: M0 and M1 (2026-10-03).
- **Verified on Windows:** M0 and M1, built and run from the command line (`scripts/build.cmd`) in Debug and Release with zero warnings. M1's window was driven by an automated script (keys, mouse, minimize, resize, close). Not yet verified in CLion.
- **Verified on macOS:** deferred until all milestones are completed (see `docs/MacChecklist.md`). The macOS CI workflow was added after M0; its first run happens on the next push to `main`.
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
