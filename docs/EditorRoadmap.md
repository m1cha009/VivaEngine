# Editor roadmap (M13–M18)

Michail's goals, asked for on 2026-10-04:

1. **Projects:** create, view, open (load) and delete projects made with the engine, like Unity Hub.
2. **Editor:** the engine works as an editor where primitives can be placed in a scene by hand.

Both goals are too big for one milestone, so they're split into six. Each one ends in something
you can run, and follows the usual milestone rules in `CLAUDE.md`: plan, zero warnings, silent
validation, `/simplify`, `docs/milestones/Mx.md`, one commit pushed to `main`, and a package
in `dist/`.

Tick the boxes as items land.

## What a "project" is

In Unity, a project is a folder (`Assets/`, `ProjectSettings/`, ...) that the editor opens. Ours
works the same way:

```
MyGame/
├── MyGame.vivaproject      # JSON: project name, engine version, startup scene
├── Assets/                 # textures, models, ...
└── Scenes/
    └── Main.scene          # JSON: the GameObjects, their components and settings
```

- **One executable, `VivaEditor`** (new `editor/` folder, linking the engine like the Sandbox
  does). It starts in the **Project Manager** (our Unity Hub). Opening a project switches the
  same window to the editor.
- **The list of known projects** lives in the user's settings folder (`SDL_GetPrefPath`, e.g.
  `%APPDATA%\Viva\VivaEditor\`), not in the repo, the way Unity Hub remembers projects.
- **First version: projects are data only.** Scenes are built from the engine's built-in
  components (Transform, MeshRenderer, Camera, plus a few simple ones such as Spinner). A
  project's own C++ code needs compiling and loading a DLL, which comes after M18 (see "Later").
- **The Sandbox and Lane Runner stay** as the code-only example of using the engine.

## M13: Scene serialization

Saving and loading scenes underpins both goals: a project is mostly scene files.

- [x] JSON reading and writing (`Core/Json`), with no exceptions; errors give a line and a reason
- [x] A component registry: type name → factory, like Unity's `[Serializable]` + `AddComponent(Type)`
- [x] `Serialize`/`Deserialize` on Transform, MeshRenderer, Camera and Spinner (Spinner moves into the engine)
- [x] Asset references in MeshRenderer: a primitive name (`"Cube"`) or an asset path, plus the
      material's color and texture, so a saved scene can find its meshes again
- [x] GameObject names, active flags and the parent/child tree saved and restored
- [x] `Scene::Save(path)` / `Scene::Load(path)`
- [x] More primitives: Sphere and Cylinder next to Cube and Plane
- [x] *You should see:* the Sandbox's demo scene saved to a file, then loaded back to look
      identical (`--demo --save` / `--load`)

Done on 2026-10-04 (see [`milestones/M13.md`](milestones/M13.md)). Also in M13: `Viva::Assets`
(every asset by name), material Tiling/Offset, and Lane Runner's planes tiled through materials.

## M14: Projects and the Project Manager

Goal 1 is done at the end of this milestone.

- [x] `Project` class: create from a template (folder layout above + an empty `Main.scene` with a
      camera and a floor), open (check the file and the engine version), close
- [x] Recent-projects list in the settings folder; missing folders are shown greyed out
- [x] Folder pickers through SDL3's `SDL_ShowOpenFolderDialog` (no new dependency); file system
      operations in `Platform/FileSystem`
- [x] New `editor/` target `VivaEditor`, with its own `CMakeLists.txt` and presets support
- [x] Project Manager window (ImGui):
  - [x] the list: name, path, last opened; sort by last opened
  - [x] **New Project**: name + location, then it opens
  - [x] **Add Existing**: pick a folder that has a `.vivaproject`
  - [x] **Open**: loads the project's startup scene
  - [x] **Remove from list** (files stay), and **Delete from disk** behind a confirmation dialog
        that names the folder
- [x] After opening: the startup scene is rendered with a fly camera (viewing only, no editing yet)
- [x] *You should see:* create two projects, close and reopen the editor, see both, open one,
      delete the other

Done on 2026-10-04 (see [`milestones/M14.md`](milestones/M14.md)). Also in M14: the new-project
location is remembered and names are suggested ("My Project 2"); `Application::SetAssetsFolder`,
`Scene::Clear`, `ReadJsonFile`, and `--quit-after`/`--display`/`--no-vsync` read by the engine.

## M15: Editor shell

The editor's frame: the windows that Unity also has, without editing yet.

- [ ] **Render to texture:** the scene is drawn into an offscreen color + depth image, which is
      shown inside an ImGui window (`ImGui::Image`). This is also the M9 deferral's "offscreen
      image" for post-processing.
- [ ] Editor windows: **Scene view**, **Hierarchy**, **Inspector**, **Project** (files in
      `Assets/`), **Console** (the `Viva::Log` output)
- [ ] Docking window layout (ImGui's docking build), saved per project
- [ ] Editor camera that isn't part of the scene: right mouse look + WASD (the Sandbox's
      FlyCamera moves into the editor), F to focus the selection
- [ ] Menu bar: File (New Scene, Open Scene, Save, Save As, Close Project), Edit, GameObject, Window
- [ ] Unsaved changes: `*` in the title, and a Save / Don't Save / Cancel prompt on close
- [ ] *You should see:* an editor that looks like a small Unity, showing the scene and its hierarchy

## M16: Placing and editing objects

Goal 2's core: build a scene by hand.

- [ ] **GameObject menu** (and right-click in the Hierarchy): Create Empty, Cube, Plane, Sphere,
      Cylinder, Camera. New objects appear in front of the editor camera.
- [ ] **Inspector:** name, active, Transform (position, rotation in degrees, scale),
      MeshRenderer (mesh, color, texture picked from `Assets/`), Camera, Add Component, Remove
- [ ] **Hierarchy:** select, rename (F2), drag to reparent, Duplicate (Ctrl+D), Delete (Del)
- [ ] **Click to select in the Scene view:** a ray from the mouse tested against each object's
      bounding box (mesh bounds are recorded when meshes are created), with the selection outlined
- [ ] A grid on the ground plane
- [ ] Save, close and reopen: the scene comes back as it was left
- [ ] *You should see:* an empty scene turned into a little level made of cubes and planes

## M17: Gizmos and undo

The parts that make editing comfortable.

- [ ] Move / Rotate / Scale gizmos, switched with W / E / R like Unity, in local or world space
- [ ] Snapping while Ctrl is held (grid step, 15° angles)
- [ ] **Undo / Redo** (Ctrl+Z / Ctrl+Y) for every edit: creating, deleting, reparenting,
      Inspector changes, gizmo drags. One gizmo drag is one undo step.
- [ ] *You should see:* objects dragged into place with handles, and every change undoable

## M18: Play mode and builds

- [ ] **Play / Pause / Stop** in the toolbar. Play snapshots the scene (with M13's
      serialization) and runs its components. Stop restores the snapshot, like Unity, where
      changes made in Play mode are lost.
- [ ] Play mode looks through the scene's Camera; the Scene view keeps the editor camera
- [ ] A generic **`VivaPlayer`** executable that runs a project's startup scene
- [ ] **File > Build**: copies the player, the shaders and the project's assets and scenes into
      a folder you choose (Unity's Build Settings → Build)
- [ ] *You should see:* press Play and watch the Spinners turn, press Stop and everything is back;
      the built folder runs without the editor

## Later (after M18, decide together)

- **Game code in projects:** each project gets C++ components built into a DLL that the editor
  and the player load, with hot reload when the code changes. Unity does this for C# scripts;
  for C++ it's a large job (a stable interface across the DLL boundary, plus registering components).
- Asset GUIDs and `.meta` files, so moving or renaming an asset doesn't break scenes
- Importing models by dragging them into the Project window, and prefabs
- Multiple scenes per project and switching scenes at runtime

## Decisions

Michail's answers (2026-10-04), also in the `CLAUDE.md` decision log:

| Question | Options | Decided |
|---|---|---|
| Project code | data-only first, code later / code from the start | **data-only first**; a project's own C++ code comes after M18 |
| JSON | hand-written / nlohmann/json / yyjson | **hand-written** (`Core/Json`), no new dependency |
| Window layout | ImGui docking branch / fixed panels | **docking**: ImGui switches to the `v1.92.9b-docking` tag in M15 |
| Gizmos | hand-written / ImGuizmo | **hand-written**, in M17 |
