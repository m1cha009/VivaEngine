# The sandbox: an executable that uses the engine the way a game would.
add_executable(Sandbox src/Main.cpp)

# Linking VivaEngine also brings in everything the engine declared PUBLIC: its include/
# folder, C++20, GLM and the VIVA_DEBUG define. SDL3 and Vulkan get linked into the .exe, but
# their headers stay hidden because the engine links them PRIVATE.
target_link_libraries(Sandbox PRIVATE VivaEngine)

viva_set_warnings(Sandbox)

# The game's own GLSL shaders, compiled to SPIR-V when the sandbox builds (see
# cmake/Shaders.cmake). None yet: the renderer's shaders are listed in engine/CMakeLists.txt,
# and the game gets its own once it decides what to draw (M8).
viva_compile_shaders(Sandbox)

# install() rules list what a player needs to run the game, like the output folder of a Unity
# build. "cmake --install" copies them into one folder; scripts/package.cmd uses that to make
# the per-milestone executables in dist/.
install(TARGETS Sandbox RUNTIME DESTINATION .)
