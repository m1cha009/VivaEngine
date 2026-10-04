# macOS checklist (archived)

> **Archived on 2026-10-04.** Michail dropped macOS: Windows is the only platform, and the macOS CI
> workflow was deleted. This list is kept as it stood after M12, in case Mac support ever comes
> back. Nothing new is added to it.

Things to verify when VivaEngine is first built and run on the Mac (M1 Pro, Apple Silicon). That
happens after the last milestone. Every milestone that adds platform-sensitive code appends its
items here. Tick them off as they're verified, and record anything that needed fixing in the
decision log in `CLAUDE.md`.

Items marked **(CI)** are also checked on every push to `main` by the macOS CI workflow
(`.github/workflows/macos.yml`), which runs on a GitHub-hosted Apple Silicon Mac. If CI is green,
those should just work on the real Mac too, but confirm them anyway. The unmarked items need the
real Mac: CLion, your own SDK install, a real GPU and window.

## M0: Project skeleton

- [ ] The Vulkan SDK is installed with **System Global Installation**, or `setup-env.sh` is
      sourced from `~/.zshrc` (see the README). `cmake --preset macos-debug` then prints
      `Found Vulkan: ... found components: glslc`. CI uses the same global installation.
- [ ] **(CI)** SDL3 builds as a static library with Apple Clang. Its Cocoa backend is
      Objective-C, so it needs Xcode or the Command Line Tools.
- [ ] **(CI)** `std::format` compiles and links. `Log` formats a floating-point timestamp, which
      Apple's libc++ only allows with a deployment target of macOS 13.3 or later (set in the macOS
      presets). If it fails, try raising `CMAKE_OSX_DEPLOYMENT_TARGET`. Falling back to `fmt` via
      FetchContent is the last resort (record it in the decision log).
- [ ] **(CI)** Engine and sandbox code builds with zero warnings under `-Wall -Wextra`, and the
      link of the Sandbox has no linker warnings (for example, about the deployment target).
- [ ] **(CI)** `VIVA_ASSERT` with and without a message compiles. It uses C++20's `__VA_OPT__`.
- [ ] **(CI)** `engine/src/Platform/Debugger.cpp` compiles. Its macOS branch detects a debugger
      with `sysctl` and `P_TRACED`.
- [ ] **(CI)** `./build/macos-debug/bin/Sandbox` prints the engine, SDL, Vulkan header, Vulkan
      loader and GLM versions. The loader line proves `libvulkan.1.dylib` is found at runtime.
      macOS no longer searches `/usr/local/lib` by default; the build-tree rpath that CMake adds
      should cover it.
- [ ] **(CI)** `./build/macos-debug/bin/Sandbox --test-assert` logs the failed assertion and exits
      with code 1. Run the same from CLion's debugger: it should stop inside
      `BreakIntoDebuggerOrExit()`. CI can't check that part.
- [ ] CLion opens the project through the `macos-debug` / `macos-release` presets with the
      default Apple Clang toolchain.

## M1: Window, input, game loop

- [ ] **(CI)** The window opens. SDL's Cocoa backend has to find the Vulkan loader for
      `SDL_WINDOW_VULKAN`. It first looks for `vkGetInstanceProcAddr` already loaded in the
      process, which our executable links.
- [ ] Retina: the startup line reports twice as many pixels as points, for example
      `1280x720 points, 2560x1440 pixels (display scale 2.00)`.
- [ ] Keys are physical positions. The Command keys report as `Left Meta`/`Right Meta` (check
      the names SDL gives them). Trackpad scrolling gives fractional wheel values.
- [ ] Minimize (Cmd+M): CPU drops to about 0%, and restoring continues normally. Cmd+Q and the red
      close button both shut down cleanly (exit code 0).
- [ ] Dragging the window edge logs `Window resized to …` in pixels.

## M2: Vulkan instance, validation, device

- [ ] The GPU list shows the Mac's GPU (for example `Apple M1 Pro`) through MoltenVK or
      KosmicKrisp, and the `Using …` line names the driver. The portability path makes this
      possible: the instance enables `VK_KHR_portability_enumeration` with
      `VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR`.
- [ ] `VK_KHR_portability_subset` is enabled automatically when the device lists it. There
      should be no validation error about it at device creation.
- [ ] "Vulkan validation layer enabled" appears in Debug, and the layer reports nothing during
      startup, resize, minimize (Cmd+M) and shutdown.
- [ ] With both drivers installed by the SDK, note which one the loader picks. KosmicKrisp needs
      macOS 26 or later; MoltenVK works on older versions. To force one, see the LunarG
      macOS guide (`VK_DRIVER_FILES`).
- [ ] Check for `[Vulkan loader]` messages, and whether any of them should stay Info.
- Already seen in CI (M2 run on the GitHub runner): the portability path works, with
  `Apple Paravirtual device (integrated GPU), Vulkan 1.3.357, driver MoltenVK 1.4.2`, validation
  on, and no errors.

## M3: Swapchain and clear screen

- [ ] **(CI)** The frame loop runs under MoltenVK without validation errors, including
      synchronization validation.
- [ ] Retina: the `Swapchain:` line shows the pixel size, twice the window's points (for example
      2560x1440 for a 1280x720 window), and the colors look sharp, not upscaled.
- [ ] The surface format is still an sRGB one (`B8G8R8A8_SRGB` or `R8G8B8A8_SRGB`), and the
      colors look the same as on Windows.
- [ ] Resize by dragging: macOS reports `currentExtent` either as the size or as "your choice"
      (`0xFFFFFFFF`). Both paths should work, and there should be no endless rebuild loop (watch
      the `[Trace] Swapchain:` lines).
- [ ] Minimize (Cmd+M), restore, full screen (green button): no validation output.
- [ ] Moving the window between a Retina and a non-Retina display rebuilds the swapchain at the
      new pixel size.
- [ ] `--no-vsync`: MoltenVK may not offer MAILBOX. The engine then tries IMMEDIATE, then falls
      back to FIFO; check which one the log reports.

## M4: First triangle

- [ ] **(CI)** The shaders compile with the SDK's `glslc` on macOS, and the `.spv` files land in
      `build/macos-debug/bin/shaders/`.
- [ ] **File paths:** `SDL_GetBasePath()` returns the executable's folder for a plain binary
      (inside an app bundle it would be `Contents/Resources/`), and the shaders load from
      `shaders/` there, whichever folder the Sandbox is started from.
- [ ] The triangle looks the same as on Windows: red at the top, blue bottom-left, green
      bottom-right. That's MoltenVK's view of Vulkan clip space, with y pointing down.
- [ ] `--display 1` with a second monitor opens the window there.

## M5: Buffers and GPU memory

- [ ] **(CI)** VMA compiles with Apple Clang without warnings (its header is a SYSTEM include),
      and the Sandbox runs with validation silent.
- [ ] **Unified memory:** the Debug log's memory heaps should show one main heap, with memory
      types that are `DEVICE_LOCAL | HOST_VISIBLE | HOST_COHERENT` (MoltenVK may list a few more).
      The staging upload still works there; it's just not strictly needed.
- [ ] The three shapes look the same as on Windows. The hexagon's corners, starting at the right
      and going clockwise: red, yellow, green, cyan, blue, magenta.
- [ ] Quitting with Esc gives exit code 0 and no "Some allocations were not freed" assert (VMA's
      leak check).

## M6: 3D: transforms, depth, camera

- [ ] **(CI)** The scene renders under MoltenVK with validation silent, including
      synchronization validation of the shared depth buffer.
- [ ] `VK_FORMAT_D32_SFLOAT` is accepted as the depth format (no "no 32-bit float depth buffer"
      line when the GPU is picked).
- [ ] The scene looks the same as on Windows: correct depth, no missing or inside-out faces
      (counter-clockwise front faces with the projection's y flip).
- [ ] **Mouse look:** holding the right mouse button (or a two-finger click on a trackpad) hides
      the cursor and turns the view smoothly. Releasing it brings the cursor back. Check the
      turning speed on a Retina screen, where SDL reports deltas in points.
- [ ] Resize and full screen rebuild the depth buffer at the new pixel size, with no validation
      output.

## M7: Textures

- [ ] **(CI)** The textures load from `build/macos-*/bin/assets/textures/` (copied by the build),
      and the mip chains generate with validation silent.
- [ ] The crate and the checker floor look the same as on Windows: correct orientation, no color
      shift (sRGB), smooth distant floor.
- [ ] Anisotropic filtering is on: the Debug build accepts the GPU with `samplerAnisotropy`
      (MoltenVK reports it), and the far floor stays fairly sharp.
- [ ] `scripts/make-textures.ps1` is Windows-only. Nothing on the Mac needs it, since the PNGs are
      committed.

## M8: Renderer abstraction

- [ ] **(CI)** The Sandbox exits with code 0 after `--quit-after`: in Debug that includes the
      shutdown check that every GPU resource was released before the renderer.
- [ ] The scene looks the same as on Windows (the sandbox now builds it through the public API).
- [ ] Minimize (Cmd+M) and restore with the new frame order (`BeginFrame` before input): the
      loop sleeps while minimized and resumes cleanly, with no validation output.
- [ ] The frame rate with vsync matches the display (60 or 120 Hz on ProMotion), and mouse look
      feels at least as responsive as before.

## M9: Debug UI

- [ ] **(CI)** Dear ImGui and its SDL3 and Vulkan backends build with Apple Clang (their own
      warnings are hidden; ours stay at zero), and the Sandbox runs with the UI.
- [ ] The Stats and Camera windows look the same as on Windows: dark grey backgrounds (not light
      grey), the same colors and the same physical size.
- [ ] Text is sharp on a Retina screen: ImGui renders its fonts at the framebuffer scale (2×). The
      UI is measured in points, so a click lands exactly on the widget under the pointer.
- [ ] Trackpad scrolling works in the demo window. A two-finger click (the right button) that
      starts over a debug window doesn't turn the camera; one that starts on the scene does.
- [ ] **Cmd**+click on a slider types a value (ImGui swaps Ctrl and Cmd on macOS). Copy and paste
      in that field (Cmd+C/V) use the system clipboard through SDL.
- [ ] Unchecking VSync rebuilds the swapchain with MAILBOX or IMMEDIATE (whichever MoltenVK or
      KosmicKrisp offers), and the FPS rises above the display's refresh rate.

## M10: Scene, GameObjects and components

- [ ] **(CI)** The Sandbox runs with the scene built from GameObjects, and exits with code 0 (no
      GPU resources outlive the renderer: the scene is destroyed first).
- [ ] The scene looks and moves the same as on Windows: the pillar ring turns, the small crate
      orbits the big one.
- [ ] The Scene window works with the trackpad: open tree nodes with their arrows, select with a
      click, and drag the inspector's values.
- [ ] Destroying "Crate" and "Pillar ring" in the inspector removes them (and their children) with
      no validation output. The Stats window's allocations drop as on Windows.

## M11: Model loading

- [ ] **(CI)** cgltf builds with Apple Clang, and the Sandbox loads both models. CI now fails if
      the Sandbox logs an error, such as a model file that wasn't found.
- [ ] Both models look as on Windows: logo boxes on the pillars, and the white milk truck with dark
      green windows, driving cab first with its wheels rolling forward.
- [ ] The models load from inside the app's folder, with `BoxTextured.gltf`, its `.bin` and its
      `.png` side by side in `assets/models/BoxTextured/`. File names in a `.gltf` must match the
      files' exact case, because a macOS volume can be case-sensitive.
- [ ] Destroying "Truck pivot" in the inspector frees its 9 allocations, as on Windows, with no
      validation output.

## M12: Lane Runner

- [ ] **(CI)** The game and the demo scene (`--demo`) both run in Debug and Release, with no errors
      in the log.
- [ ] The game plays as on Windows: Space starts, A/D and the arrow keys change lanes, a crash shows
      "Crashed!", Space restarts.
- [ ] The HUD text is sharp on a Retina screen and the same size as on Windows: it's measured in
      points, and ImGui renders the scalable font at the framebuffer's 2× scale.
- [ ] At 144 km/h the frame rate holds the display's refresh (60 or 120 Hz with ProMotion), and the
      road shows no flickering against the grass far away.
- [ ] F1 shows the debug windows over the HUD; Esc and Cmd+Q quit.
