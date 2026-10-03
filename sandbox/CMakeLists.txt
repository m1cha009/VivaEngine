# The sandbox: an executable that uses the engine the way a game would.
add_executable(Sandbox
    src/DebugWindows.cpp
    src/DemoScene.cpp
    src/FlyCamera.cpp
    src/Main.cpp)

# Linking VivaEngine also brings in everything the engine declared PUBLIC: its include/
# folder, C++20, GLM, Dear ImGui's imgui.h and the VIVA_DEBUG define. SDL3 and Vulkan get linked
# into the .exe, but their headers stay hidden because the engine links them PRIVATE.
target_link_libraries(Sandbox PRIVATE VivaEngine)

viva_set_warnings(Sandbox)

# The game's own GLSL shaders, compiled to SPIR-V when the sandbox builds (see
# cmake/Shaders.cmake). None yet: every material uses the engine's Unlit shader (listed in
# engine/CMakeLists.txt), and the renderer doesn't load game shaders yet.
viva_compile_shaders(Sandbox)

# The game's assets, copied next to the executable (see cmake/Assets.cmake). DemoScene loads
# the textures with Renderer::LoadTexture.
viva_copy_assets(Sandbox
    textures/checker.png
    textures/crate.png)

# install() rules list what a player needs to run the game, like the output folder of a Unity
# build. "cmake --install" copies them into one folder; scripts/package.cmd uses that to make
# the per-milestone executables in dist/.
install(TARGETS Sandbox RUNTIME DESTINATION .)
