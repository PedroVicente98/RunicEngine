# Runic::GodotBridge

Presentation · Headless: no (presentation) · Packages: GDB-1, GDB-2, GDB-3, GDB-4, GDB-5, GDB-6, GDB-7, GDB-8, GDB-9, GDB-10, GDB-11, GDB-12, GDB-13 · Status:
has code; contract in [ARCHITECTURE.md](ARCHITECTURE.md)

**Holds** (RunicDocs › Repositories, build targets and processes › Engine modules): The GodotLayer (LibGodot instance, window, input), the presenter (RenderFrame to RenderingServer), debug layers, world UI (anchored, diegetic, batched), the view-model binder, the audio backend on the AudioServer; the production look later

**May depend on** (a ceiling; `cmake/RunicModuleCatalog.cmake` enforces it): Application, Client, Audio, Assets (libgodot and godot-cpp private)

## Folders

| Folder | Holds | Packages |
| --- | --- | --- |
| `Public/` | The module's API: the only headers other modules may include, by their path under `Public/` (flat unless noted below). No library types, except an approved exception recorded in `ARCHITECTURE.md`. | every package that adds API |
| [`Source/LibGodot/`](Source/LibGodot/README.md) | The LibGodot host: `GodotRuntime` (load libgodot, create the one instance with the command line, start, iterate, destroy; class-registration hooks by initialization level) and `SharedLibrary`, the loader that opens libgodot itself (not GDExtensions). | slice 0 (code) |
| [`Source/GodotLayer/`](Source/GodotLayer/README.md) | The GodotLayer: the LibGodot instance inside the LayerStack, at the top of the client's stack; the window from the DisplayServer; headless mode; Godot's `iteration()` as the last step of each frame. | GDB-1 (slice 1) |
| [`Source/Input/`](Source/Input/README.md) | The input sink: a node at the scene-tree root that receives only the input Godot's UI did not consume and turns it into AppEvents for the stack. | GDB-1 (slice 1) |
| [`Source/Debug/`](Source/Debug/README.md) | Debug drawing on Godot: Core's DebugGeometry (CORE-3, slice 2) uploaded as immediate meshes with depth-tested and overlay materials, toggled debug layers, the development overlay panel, and the physics geometry layer. | GDB-2 (slice 2), GDB-4 (slice 8) |
| [`Source/Presenter/`](Source/Presenter/README.md) | The presenter: RenderFrame instances created, moved and freed in the RenderingServer by RenderInstanceId, the presenter origin rebased about every 4 km, then meshes and MultiMesh, skinned instances, lights and fog, water and field grids. | GDB-3 (slice 5), GDB-5 (slice 22), GDB-6 (slice 23), GDB-7 (slice 25a), GDB-8 (slice 42a) |
| [`Source/UIBridge/`](Source/UIBridge/README.md) | The UI bridge: one Godot UI scene per client state and the view-model binder that writes only changed fields to controls. | GDB-9 (slice 19) |
| [`Source/WorldUI/`](Source/WorldUI/README.md) | World UI: anchored Label3D and Sprite3D, diegetic panels in SubViewports, batched MultiMesh nameplates and the mode fallbacks by distance, count and battle budget. | GDB-10 (slice 23a), GDB-11 (slice 38) |
| [`Source/AudioBackend/`](Source/AudioBackend/README.md) | The audio backend: implements Runic::Audio's backend interface on Godot's AudioServer. | GDB-12 (slice 24) |
| [`Source/ProductionLook/`](Source/ProductionLook/README.md) | The production look: materials, terrain, water shading, VFX and decals as Godot resources fed by RenderData. | GDB-13 (slice 44+) |
| `Tests/` | GoogleTest tests, registered with CTest; one test executable per module. | every package |
| [`ARCHITECTURE.md`](ARCHITECTURE.md) | The module contract: purpose, public API, ownership, allowed and forbidden dependencies, threading, work packages and their status. | exists (slice 0) |
| [`CMakeLists.txt`](CMakeLists.txt) | One `runic_add_module(...)` call. | exists (slice 0) |


## How the folders fill

This module has code. Its contract is [ARCHITECTURE.md](ARCHITECTURE.md); a folder whose package has not started holds only its README. Every folder exists from the start (decided 2026-11-12) so the layout never moves; a package writes inside this module's folder only, and reports any change another module needs instead of making it.

## Sources

The catalog row and package slices were read from RunicDocs › Architecture (as of 2026-11-13). When RunicDocs and this file disagree, RunicDocs wins.

- RunicDocs › Architecture › Godot integration (Engine)
- RunicDocs › Architecture › Client runtime (Game on Engine)
- RunicDocs › Architecture › Render data, extraction and world UI (Engine, Game)
- RunicDocs › Architecture › Debug drawing (Engine)
- RunicDocs › Architecture › Audio (Engine, Game)
