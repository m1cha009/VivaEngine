# VivaEngine

A small game engine in C++20 and Vulkan 1.3, built one milestone at a time to learn how game
engines work. It runs on Windows.

- [`CLAUDE.md`](CLAUDE.md): the project brief, roadmap, status and decision log
- [`docs/Guide.md`](docs/Guide.md): how to use the engine (build, run, make a scene, Unity cheat sheet)
- [`docs/milestones/`](docs/milestones/): one explainer per milestone (start with [`M0.md`](docs/milestones/M0.md)). Each describes the code as of its milestone's commit (`git log --oneline` lists them; `git checkout <commit>` to follow along exactly).
- [`docs/EditorRoadmap.md`](docs/EditorRoadmap.md): the plan for projects and the editor (M13–M18)

## Lane Runner

The Sandbox is a small game made with the engine ([M12](docs/milestones/M12.md)): drive a milk truck
down an endless three-lane road, dodge the crates and collect the logo boxes while the speed rises.
Space starts, A/D or the arrow keys change lanes, F1 shows the debug windows, Esc quits.
`Sandbox --demo` shows the engine's demo scene (M10, M11) instead, and `Sandbox --load <file>` a
scene saved to a file ([M13](docs/milestones/M13.md)); `assets/scenes/` has two examples.

## VivaEditor

The editor ([M14](docs/milestones/M14.md)) is a second program next to the Sandbox. It opens on
the Project Manager, where you create, open and delete projects, like Unity Hub. Editing scenes
comes in the next milestones ([`docs/EditorRoadmap.md`](docs/EditorRoadmap.md)).

## Prerequisites

| | |
|---|---|
| Compiler | Visual Studio Build Tools 2026 with the **Desktop development with C++** workload (MSVC) |
| CMake | 3.25 or newer |
| Ninja | `winget install Ninja-build.Ninja` (CLion also bundles one) |
| Vulkan SDK | [LunarG Vulkan SDK](https://vulkan.lunarg.com/sdk/home); the installer sets `VULKAN_SDK` |
| Git | any recent version |

You don't install the libraries by hand (SDL3, GLM, VMA, stb_image, Dear ImGui and cgltf). CMake
downloads pinned versions of them the first time you configure a preset, so that first configure
needs internet access and takes a minute or so.

## Windows

### CLion

1. **Pick the compiler (first time only).** Open Settings → Build, Execution, Deployment →
   Toolchains and select the Visual Studio toolchain (add one with **+** if there's none). Set
   **Toolset** to `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools` and
   **Architecture** to `amd64`, and keep it at the top of the list so it's the default. The dev
   machine also has VS Community 2026, which has no C++ workload, and Build Tools 2019, which has
   an older compiler. Don't let CLion pick either of those.
2. **Open the `VivaEngine` folder.** CLion reads `CMakePresets.json` and offers one CMake profile
   per preset. Enable `windows-debug`, plus `windows-release` if you want it, either in the
   wizard that opens or later under Settings → Build, Execution, Deployment → CMake. If CLion
   also created its own `Debug` profile, you can delete it.
3. Select the **Sandbox** (or **VivaEditor**) run configuration and the **windows-debug** profile, then Run
   (Shift+F10) or Debug (Shift+F9).

### Command line

`cl.exe`, the MSVC compiler, only works inside the developer environment that `vcvars64.bat`
sets up. `scripts\build.cmd` does that for you, so it works from any plain shell (cmd,
PowerShell or Git Bash):

```bat
scripts\build.cmd windows-debug
build\windows-debug\bin\Sandbox.exe
```

`scripts\build.cmd windows-release` builds the Release preset. To build by hand instead, open the
x64 Native Tools prompt for Build Tools 2026 from the Start menu, under **Visual Studio 2026 →
Visual Studio Tools → VC**. On the dev machine it's named "x64 Native Tools Command Prompt for
VS (2)"; don't pick the VS 2019 one. Then `cd` into the repository and run:

```bat
cmake --preset windows-debug
cmake --build --preset windows-debug
```

## Troubleshooting

- **`The CMAKE_CXX_COMPILER: cl is not a full path and was not found in the PATH`**.
  CMake ran outside the MSVC developer environment. Use `scripts\build.cmd` or the Native Tools
  prompt; in CLion, check the toolchain (step 1 above). Delete the half-configured
  `build\<preset>` folder before trying again.
- **`Could NOT find Vulkan`**. `VULKAN_SDK` isn't set in the process running CMake.
  Programs that were already open when the SDK was installed (terminals, CLion) keep their old
  environment. Restart them.
- **The first configure is slow.** CMake is downloading the libraries (about 26 MB, most of it SDL3) into
  `build/<preset>/_deps/`, and the first build compiles SDL3. Every preset has its own copy, so this happens once
  per preset.

## Layout

| Folder | Contents |
|---|---|
| `engine/` | The `VivaEngine` static library. `include/Viva/` is the public API. `src/` is private: `Core/`, `Platform/` (the only place for SDL3 and OS-specific code), `Renderer/` (the only place for Vulkan) and `Scene/` (GameObjects, components, transforms, model loading). |
| `sandbox/` | The `Sandbox` executable: the Lane Runner game, and with `--demo` the engine's demo scene. It uses the engine the way any game would |
| `editor/` | The `VivaEditor` executable: the Project Manager, and (from M15) the scene editor |
| `cmake/` | CMake helpers: dependencies, compiler warnings, shader compilation, asset copying |
| `shaders/` | GLSL sources, compiled to SPIR-V into `build/<preset>/bin/shaders/` |
| `assets/` | Game assets: textures (from `scripts/make-textures.ps1`) and glTF models (credits in `assets/models/CREDITS.md`) |
| `scripts/` | `build.cmd` and `package.cmd` (Windows builds), `make-textures.ps1` |
| `docs/` | The usage guide, milestone explainers and the editor roadmap |
