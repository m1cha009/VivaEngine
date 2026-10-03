# VivaEngine

[![macOS build](https://github.com/m1cha009/VivaEngine/actions/workflows/macos.yml/badge.svg)](https://github.com/m1cha009/VivaEngine/actions/workflows/macos.yml)

A small game engine in C++20 and Vulkan 1.3, built one milestone at a time to learn how game
engines work.

- [`CLAUDE.md`](CLAUDE.md): the project brief, roadmap, status and decision log
- [`docs/milestones/`](docs/milestones/): one explainer per milestone (start with [`M0.md`](docs/milestones/M0.md)). Each describes the code as of its milestone's commit (`git log --oneline` lists them; `git checkout <commit>` to follow along exactly).
- [`docs/MacChecklist.md`](docs/MacChecklist.md): things to verify when building on macOS

## Lane Runner

The Sandbox is a small game made with the engine ([M12](docs/milestones/M12.md)): drive a milk truck
down an endless three-lane road, dodge the crates and collect the logo boxes while the speed rises.
Space starts, A/D or the arrow keys change lanes, F1 shows the debug windows, Esc quits.
`Sandbox --demo` shows the engine's demo scene (M10, M11) instead.

## Prerequisites

| | Windows | macOS |
|---|---|---|
| Compiler | Visual Studio Build Tools 2026 with the **Desktop development with C++** workload (MSVC) | Xcode or the Xcode Command Line Tools (Apple Clang) |
| CMake | 3.25 or newer | 3.25 or newer (`brew install cmake`, or the copy bundled with CLion) |
| Ninja | `winget install Ninja-build.Ninja` (CLion also bundles one) | `brew install ninja` |
| Vulkan SDK | [LunarG Vulkan SDK](https://vulkan.lunarg.com/sdk/home); the installer sets `VULKAN_SDK` | LunarG Vulkan SDK with **System Global Installation** checked ([see below](#vulkan-sdk)) |
| Git | any recent version | any recent version |

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
3. Select the **Sandbox** run configuration and the **windows-debug** profile, then Run
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

## macOS

### Vulkan SDK

macOS has no Vulkan of its own. The LunarG SDK provides both the Vulkan loader and a driver that
translates Vulkan calls to Metal: MoltenVK, or KosmicKrisp on macOS 26 and later. In the
installer, check **System Global Installation**. That copies the headers, libraries and tools
into `/usr/local`, where CMake finds them without any setup.

If you skipped the global install, configuring fails with `Could NOT find Vulkan`. Fix it by
sourcing the SDK's environment script from your shell profile. Terminals pick it up, and so does
CLion, which reads your login shell's environment when it starts:

```bash
echo 'source ~/VulkanSDK/<version>/setup-env.sh' >> ~/.zshrc
```

Then restart the terminal and CLion, and configure again.

### CLion

Open the folder and enable the `macos-debug` profile (Settings → Build, Execution, Deployment →
CMake). The default toolchain, Apple Clang, is the right one. Select **Sandbox** and Run.

### Command line

```bash
cmake --preset macos-debug
cmake --build --preset macos-debug
./build/macos-debug/bin/Sandbox
```

## Continuous integration (macOS)

Every push to `main` triggers [`.github/workflows/macos.yml`](.github/workflows/macos.yml) on a
GitHub-hosted Apple Silicon Mac. It installs the Vulkan SDK, builds `macos-debug` and
`macos-release` with Apple Clang, fails on any warning in our code, and runs the Sandbox. Results
show up in the repository's **Actions** tab, as the badge at the top of this README, and as a green
check or red cross next to each commit. GitHub emails you when a run fails. You can also start a
run by hand: Actions → macOS build → **Run workflow**.

It costs nothing, because the repository is public and the workflow uses a standard runner
(`macos-latest`). Don't change it to a `-large` or `-xlarge` runner: those are always billed.

## Troubleshooting

- **`The CMAKE_CXX_COMPILER: cl is not a full path and was not found in the PATH`** (Windows).
  CMake ran outside the MSVC developer environment. Use `scripts\build.cmd` or the Native Tools
  prompt; in CLion, check the toolchain (step 1 above). Delete the half-configured
  `build\<preset>` folder before trying again.
- **`Could NOT find Vulkan`** (Windows). `VULKAN_SDK` isn't set in the process running CMake.
  Programs that were already open when the SDK was installed (terminals, CLion) keep their old
  environment. Restart them.
- **`Could NOT find Vulkan`** (macOS). See [Vulkan SDK](#vulkan-sdk) above.
- **The first configure is slow.** CMake is downloading the libraries (about 26 MB, most of it SDL3) into
  `build/<preset>/_deps/`, and the first build compiles SDL3. Every preset has its own copy, so this happens once
  per preset.

## Layout

| Folder | Contents |
|---|---|
| `engine/` | The `VivaEngine` static library. `include/Viva/` is the public API. `src/` is private: `Core/`, `Platform/` (the only place for SDL3 and OS-specific code), `Renderer/` (the only place for Vulkan) and `Scene/` (GameObjects, components, transforms, model loading). |
| `sandbox/` | The `Sandbox` executable: the Lane Runner game, and with `--demo` the engine's demo scene. It uses the engine the way any game would |
| `cmake/` | CMake helpers: dependencies, compiler warnings, shader compilation, asset copying |
| `shaders/` | GLSL sources, compiled to SPIR-V into `build/<preset>/bin/shaders/` |
| `assets/` | Game assets: textures (from `scripts/make-textures.ps1`) and glTF models (credits in `assets/models/CREDITS.md`) |
| `scripts/` | `build.cmd` and `package.cmd` (Windows builds), `make-textures.ps1` |
| `.github/workflows/` | The macOS CI build |
| `docs/` | Milestone explainers and the macOS checklist |
