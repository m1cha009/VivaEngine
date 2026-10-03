# macOS checklist

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
