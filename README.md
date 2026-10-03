# VivaEngine

A small game engine in C++20 and Vulkan 1.3, built one milestone at a time to learn how game
engines work.

- [`CLAUDE.md`](CLAUDE.md): the project brief, roadmap, status and decision log
- [`docs/milestones/`](docs/milestones/): one explainer per milestone (start with [`M0.md`](docs/milestones/M0.md))
- [`docs/MacChecklist.md`](docs/MacChecklist.md): things to verify when building on macOS

## Prerequisites

| | Windows | macOS |
|---|---|---|
| Compiler | Visual Studio Build Tools 2026 with the **Desktop development with C++** workload (MSVC) | Xcode or the Xcode Command Line Tools (Apple Clang) |
| CMake | 3.25 or newer | 3.25 or newer (`brew install cmake`, or the copy bundled with CLion) |
| Ninja | `winget install Ninja-build.Ninja` (CLion also bundles one) | `brew install ninja` |
| Vulkan SDK | [LunarG Vulkan SDK](https://vulkan.lunarg.com/sdk/home); the installer sets `VULKAN_SDK` | LunarG Vulkan SDK with **System Global Installation** checked ([see below](#vulkan-sdk)) |
| Git | any recent version | any recent version |

You don't install SDL3 or GLM by hand. CMake downloads pinned versions of both the first time
you configure a preset, so that first configure needs internet access and takes a minute or so.

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

## Troubleshooting

- **`The CMAKE_CXX_COMPILER: cl is not a full path and was not found in the PATH`** (Windows).
  CMake ran outside the MSVC developer environment. Use `scripts\build.cmd` or the Native Tools
  prompt; in CLion, check the toolchain (step 1 above). Delete the half-configured
  `build\<preset>` folder before trying again.
- **`Could NOT find Vulkan`** (Windows). `VULKAN_SDK` isn't set in the process running CMake.
  Programs that were already open when the SDK was installed (terminals, CLion) keep their old
  environment. Restart them.
- **`Could NOT find Vulkan`** (macOS). See [Vulkan SDK](#vulkan-sdk) above.
- **The first configure is slow.** CMake is cloning SDL3 and GLM into `build/<preset>/_deps/` and
  then building SDL3. Every preset has its own copy, so this happens once per preset.

## Layout

| Folder | Contents |
|---|---|
| `engine/` | The `VivaEngine` static library. `include/Viva/` is the public API. `src/` is private: `Core/`, `Platform/` (the only place for SDL3 and OS-specific code) and `Renderer/` (the only place for Vulkan). |
| `sandbox/` | The `Sandbox` executable, a test app that uses the engine the way a game would |
| `cmake/` | CMake helpers: dependencies, compiler warnings, shader compilation |
| `shaders/` | GLSL sources, compiled to SPIR-V into `build/<preset>/bin/shaders/` |
| `assets/` | Game assets (empty for now) |
| `scripts/` | `build.cmd`, the Windows command-line build helper |
| `docs/` | Milestone explainers and the macOS checklist |
