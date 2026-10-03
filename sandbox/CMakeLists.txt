# The sandbox: an executable that uses the engine the way a game would.
add_executable(Sandbox
    src/FlyCamera.cpp
    src/Main.cpp)

# Linking VivaEngine also brings in everything the engine declared PUBLIC: its include/
# folder, C++20, GLM and the VIVA_DEBUG define. SDL3 and Vulkan get linked into the .exe, but
# their headers stay hidden because the engine links them PRIVATE.
target_link_libraries(Sandbox PRIVATE VivaEngine)

viva_set_warnings(Sandbox)

# The game's own GLSL shaders, compiled to SPIR-V when the sandbox builds (see
# cmake/Shaders.cmake). None yet: the renderer's shaders are listed in engine/CMakeLists.txt,
# and the game gets its own once it decides what to draw (M8).
viva_compile_shaders(Sandbox)

# The game's assets, copied next to the executable (see cmake/Assets.cmake). Until M8 the
# renderer's demo scene is what loads these textures; from M8 on the game will.
viva_copy_assets(Sandbox
    textures/checker.png
    textures/crate.png)

# install() rules list what a player needs to run the game, like the output folder of a Unity
# build. "cmake --install" copies them into one folder; scripts/package.cmd uses that to make
# the per-milestone executables in dist/.
install(TARGETS Sandbox RUNTIME DESTINATION .)
