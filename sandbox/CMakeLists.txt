# The sandbox: an executable that uses the engine the way a game would. It starts the Lane Runner
# game (M12); with --demo it shows the engine's demo scene instead.
add_executable(Sandbox
    src/DebugWindows.cpp
    src/DemoScene.cpp
    src/FollowCamera.cpp
    src/LaneRunner.cpp
    src/LaneRunnerScene.cpp
    src/Main.cpp
    src/Treadmill.cpp
    src/TruckController.cpp
    src/TruckWheels.cpp)

# Linking VivaEngine also brings in everything the engine declared PUBLIC: its include/
# folder, C++20, GLM, Dear ImGui's imgui.h and the VIVA_DEBUG define. SDL3 and Vulkan get linked
# into the .exe, but their headers stay hidden because the engine links them PRIVATE.
target_link_libraries(Sandbox PRIVATE VivaEngine)

viva_set_warnings(Sandbox)

# The game's own GLSL shaders, compiled to SPIR-V when the sandbox builds (see
# cmake/Shaders.cmake). None yet: every material uses the engine's Unlit shader (listed in
# engine/CMakeLists.txt), and the renderer doesn't load game shaders yet.
viva_compile_shaders(Sandbox)

# The game's assets, copied next to the executable (see cmake/Assets.cmake). The scenes load
# them by name through Viva::Assets. A .gltf model's .bin and image files are listed too: the
# .gltf only refers to them. CREDITS.md travels with the models, because their license asks for
# credit wherever they go. The scene files (M13) are examples for --load: Demo.scene is the demo
# scene as --demo --save writes it, and Shapes.scene is written by hand.
viva_copy_assets(Sandbox
    models/BoxTextured/BoxTextured.gltf
    models/BoxTextured/BoxTextured0.bin
    models/BoxTextured/CesiumLogoFlat.png
    models/CesiumMilkTruck.glb
    models/CREDITS.md
    scenes/Demo.scene
    scenes/Shapes.scene
    textures/checker.png
    textures/crate.png)

# install() rules list what a player needs to run the game, like the output folder of a Unity
# build. "cmake --install" copies them into one folder; scripts/package.cmd uses that to make
# the per-milestone executables in dist/.
install(TARGETS Sandbox RUNTIME DESTINATION .)
