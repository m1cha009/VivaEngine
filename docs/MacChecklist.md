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
