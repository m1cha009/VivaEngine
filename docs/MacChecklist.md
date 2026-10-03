# macOS checklist

Things to verify the first time VivaEngine is built and run on the Mac (M1 Pro, Apple Silicon).
Every milestone that adds platform-sensitive code appends its items here. Tick items off as
they're verified, and record anything that needed fixing in the decision log in `CLAUDE.md`.

## M0: Project skeleton

- [ ] The Vulkan SDK is installed with **System Global Installation**, or `setup-env.sh` is
      sourced from `~/.zshrc` (see the README). `cmake --preset macos-debug` then prints
      `Found Vulkan: ... found components: glslc`.
- [ ] SDL3 builds as a static library with Apple Clang. Its Cocoa backend is Objective-C, so it
      needs Xcode or the Command Line Tools.
- [ ] `std::format` compiles and links. `Log` formats a floating-point timestamp, which Apple's
      libc++ only allows with a deployment target of macOS 13.3 or later (set in the macOS presets).
      If it fails, try raising `CMAKE_OSX_DEPLOYMENT_TARGET`. Falling back to `fmt` via
      FetchContent is the last resort (record it in the decision log).
- [ ] Engine and sandbox code builds with zero warnings under `-Wall -Wextra`.
- [ ] `VIVA_ASSERT` with and without a message compiles. It uses C++20's `__VA_OPT__`.
- [ ] `engine/src/Platform/Debugger.cpp` compiles. Its macOS branch, which detects a debugger with
      `sysctl` and `P_TRACED`, has never been compiled.
- [ ] `./build/macos-debug/bin/Sandbox` prints the engine, SDL, Vulkan header, Vulkan loader and
      GLM versions. The loader line proves `libvulkan.1.dylib` is found at runtime.
- [ ] `./build/macos-debug/bin/Sandbox --test-assert` logs the failed assertion and exits with
      code 1. Run the same from CLion's debugger: it should stop inside
      `BreakIntoDebuggerOrExit()`.
- [ ] CLion opens the project through the `macos-debug` / `macos-release` presets with the
      default Apple Clang toolchain.
