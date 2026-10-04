# VivaEditor (M14): the editor, a second program built on the engine library, next to the
# Sandbox. It starts in the Project Manager, which creates and opens projects (M14), and edits
# their scenes in docked windows (M15 onwards).
add_executable(VivaEditor
    src/Builder.cpp
    src/ConsoleWindow.cpp
    src/EditorApp.cpp
    src/EditorCamera.cpp
    src/EditorMenus.cpp
    src/GameView.cpp
    src/Gizmo.cpp
    src/HierarchyWindow.cpp
    src/InspectorWindow.cpp
    src/Main.cpp
    src/Picking.cpp
    src/ProjectList.cpp
    src/ProjectManager.cpp
    src/ProjectWindow.cpp
    src/SceneEditor.cpp
    src/SceneView.cpp
    src/SceneViewport.cpp)

# The same as the Sandbox: the engine brings its public headers, GLM and ImGui along.
target_link_libraries(VivaEditor PRIVATE VivaEngine)

viva_set_warnings(VivaEditor)

# The editor has no assets of its own: a project's come from its folder. The engine's compiled
# shaders are installed by the engine (see engine/CMakeLists.txt).
install(TARGETS VivaEditor RUNTIME DESTINATION .)
