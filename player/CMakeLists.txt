# VivaPlayer (M18): the program a built game runs as. It opens the project next to it and plays its
# startup scene, like the player Unity copies into every build. The editor's File > Build copies
# it into the build folder, renamed after the project.
add_executable(VivaPlayer
    src/Main.cpp
    src/PlayerApp.cpp)

# The same as the Sandbox and the editor: the engine brings its public headers, GLM and ImGui along.
target_link_libraries(VivaPlayer PRIVATE VivaEngine)

viva_set_warnings(VivaPlayer)

# Installed next to the editor, which needs it to build projects.
install(TARGETS VivaPlayer RUNTIME DESTINATION .)
