# The sandbox: an executable that uses the engine the way a game would.
add_executable(Sandbox src/Main.cpp)

# Linking VivaEngine also brings in everything the engine declared PUBLIC: its include/
# folder, C++20, GLM and the VIVA_DEBUG define. SDL3 and Vulkan get linked into the .exe, but
# their headers stay hidden because the engine links them PRIVATE.
target_link_libraries(Sandbox PRIVATE VivaEngine)

viva_set_warnings(Sandbox)

# GLSL files compiled to SPIR-V when the sandbox builds (see cmake/Shaders.cmake).
# None yet. The first ones arrive in M4.
viva_compile_shaders(Sandbox SOURCES)
