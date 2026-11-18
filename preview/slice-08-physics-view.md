# Slice 8 · Physics view

**Runnable result:** collision shapes are drawn and can be inspected, in the client and in the editor's viewport.
**Packages:** PHYS-3 (Runic::Physics), GDB-4 (Runic::GodotBridge), GDT-3 (Runic::GodotTools), a Core addition (cached
debug meshes and the physics layer's geometry, which CORE-3 listed and slice 2 left for their first user), and the
RunicClient and RunicEditor changes that show them. Two small changes ride along, each its own commit: the Client tests
name the new physics capacity, and the walk scene's locals no longer shadow each other (so it builds under `-Wshadow`).
**Status:** done on the preview branch; 4 new tests in `RunicCoreTests`, 7 in `RunicPhysicsTests`, 3 in
`RunicGodotBridgeTests`, 12 in `RunicGodotToolsTests`, a new RunicClient CTest and a changed RunicEditor one. The
Engine's debug build runs 463 tests and the headless build 366, all passing.

```sh
cmake --preset debug && cmake --build --preset debug && ctest --preset debug
./build/debug/bin/RunicClient                       # the walk, with its collision drawn
./build/debug/bin/RunicEditor --editor --path build/debug/godot/NewProjectCheck   # after ctest: the docks
ctest --preset debug -R "runic_client_physics_view|editor_opens_project|RunicPhysicsDock"
```

The client's check prints, among Godot's own lines:

```text
RUNIC: physics check: the capsule's collision lines up with its shape within 0.0 mm over 300 frames
RUNIC: physics geometry stopped at frame 300: 1392 line vertices, 0 triangle vertices, 1 mesh instances of 1 cached meshes (1 uploads)
RUNIC: walk scene: the capsule walked 7.3 m on terrain 8602 km from the origin, jumps 1, marks 0, highest 1.15 m, ends in the air
```

and the editor's:

```text
RUNIC: world dock shows 16 entities of simulation 1 (6 component types, 7 queries, 0 command queues)
RUNIC: server view draws 1080 line vertices, 0 triangle vertices and 0 mesh instances of 15 bodies in the editor's viewport (over the visuals)
RUNIC: physics dock shows 15 bodies of simulation 1 (14 active, 2 shapes, 0 characters)
RUNIC: server view stopped after 64 refreshes of 255 us: 1080 line vertices, 0 triangle vertices, 0 mesh instances of 0 cached meshes (0 uploads)
```

**In the client's window:** slice 7's walk, with what physics holds drawn over it: the terrain's collision as a grey
wireframe (one cached mesh, uploaded once), the blocks grey, the crates blue (dark blue once asleep), the capsule's
collision green around its shape. The development overlay has two more toggles: **Physics geometry** (on at the start)
and **See what the server sees** (off), which draws the collision over everything instead of depth-tested.

**In the editor:** a **Runic Physics** dock under the Runic World dock: the physics world's counts, the switches **See
what the server sees** and **Over the visuals**, **Pick a body in the viewport**, the bodies near the editor's camera
(body, shape, motion) and the selected body's properties (shape, kind, half extents, motion, layer, position in world
metres, rotation, velocity, friction, restitution, state, sensor, owner). The 3D viewport shows the local world's
collision, a 60 m floor, the twelve markers as kinematic boxes going round and two crates at the centre, drawn over
whatever the edited scene shows; the selected body's bounds are magenta. With **Pick a body in the viewport** pressed,
the next left click in a 3D viewport selects the body under it.

## What was built

| Package | What | Where |
| --- | --- | --- |
| (Core, slice 8 addition) | `DebugMeshId`; `DebugMesh` and the thread-safe `DebugMeshLibrary` (a revision per change, snapshots by ID); `DebugMeshInstance` in `DebugGeometry` (4,096 a frame); the frame's own physics geometry (`RenderFrame::physics`) and the exchange's library (`RenderFrameExchange::Meshes`) | `Engine/Foundation/Core`: `Public/RenderData/DebugMeshes.hpp`, `DebugGeometry.hpp`, `RenderFrame.hpp`, `Source/RenderData/DebugMeshes.cpp` |
| PHYS-3 | Jolt's debug renderer inside Physics: the bodies and characters near a point drawn into Core debug geometry relative to it, by motion and sleep in their colors, faces on request, bounds and velocities in the overlay batch; a shape of 256 triangles or more captured once into a cached mesh and drawn by an instance; the descriptions tools read (`FindBodies`, `DescribeBody`, `DescribeShape`); the option `RUNIC_PHYSICS_DEBUG_VIEW` | `Engine/SimulationCore/Physics`: `PhysicsWorld.hpp`, `Source/DebugExtraction`, `cmake/Dependencies.cmake` |
| GDB-4 | The physics geometry layer: its own batches and materials, the cached meshes synced from the library (cleared and filled in place) and a pool of instances, "see what the server sees" on the overlay material, the overlay's toggles and numbers, the read-back for checks | `Engine/Presentation/GodotBridge`: `Public/DebugView.hpp`, `GodotLayer.hpp`, `Source/Debug/DebugLayers`, `DebugMeshCache`, `DevelopmentOverlay` |
| GDT-3 | The physics inspector: its model (`PhysicsInspector`: summary, bodies near a point, properties, picking with a ray, the selection, the server view's geometry), the Runic Physics dock, the "see what the server sees" layer in the editor's viewports, the plugin forwarding a click while the dock picks | `Engine/Tools/GodotTools`: `Public/PhysicsInspector.hpp`, `Source/Docks/PhysicsInspector.cpp`, `RunicPhysicsDock`, `ServerViewLayer`, `InspectorText.hpp`, `Source/Plugin/RunicEditorPlugin` |
| (programs) | RunicClient: the physics geometry extractor, `--physics-check` and its CTest. RunicEditor: the local world runs physics (a floor, the markers as kinematic boxes, two crates) | `Engine/Programs/RunicClient/WalkScene.cpp`, `Main.cpp`; `Engine/Programs/RunicEditor/LocalWorld.cpp` |

## The order to read it in

1. **`Core/Public/RenderData/DebugMeshes.hpp`**: the whole idea of cached meshes in its opening comment: a mesh
   crosses to the reader once, each frame carries only its instances.
2. **`Physics/Source/DebugExtraction/DebugCapture.hpp`**: Jolt's `DebugRendererSimple` turned into Core geometry. Then
   `DebugExtraction.cpp`'s `ExtractDebugGeometry`: a box query around the origin, each body drawn by its shape
   (or placed as an instance of its cached mesh), characters, bounds, velocities.
3. **`GodotBridge/Source/Debug/DebugLayers.cpp`**: `ApplyPhysics` (upload the two batches, sync the meshes, place the
   instances) and `PhysicsMaterial` (the server view is the overlay material). `DebugMeshCache.cpp`'s `Sync` is the
   reader's half of the library.
4. **`GodotTools/Public/PhysicsInspector.hpp`**: the inspector's rules in one page; `PhysicsInspector.cpp`'s `Pick`
   (rays against the shapes in the body's own space, in doubles) and `DrawServerView` (Physics' view plus the
   selection).
5. **`GodotTools/Source/Docks/RunicPhysicsDock.cpp`**: `RefreshServerView` (when the layer redraws) and `ViewOrigin`
   (the camera's 16 m cell); `ServerViewLayer.cpp` is GDB-4's upload for the editor's world.

## One frame of the server view, traced

The editor runs, the local simulation steps at 30 Hz, a marker moves.

1. The Simulation layer, below the GodotLayer, runs a realtime tick: the marker's drift moves its `WorldTransform`;
   in PhysicsStep the bridge hands its kinematic target to Physics, which steps.
2. Later in the same frame Godot runs the editor's frame, and the physics dock's `_process` asks whether the layer
   must redraw: the physics world stepped (its step count moved), so yes.
3. `ViewOrigin` takes the editor camera's position in the scene, snaps it to its 16 m cell and adds
   `editedSceneOrigin`: a world position in 64 bits.
4. `PhysicsInspector::DrawServerView` asks Physics for its debug view 250 m around that position, into the overlay
   batch (over the visuals). Physics finds the bodies with a box query, and Jolt draws each through the capture, which
   writes lines relative to the origin, in 32-bit floats a few metres long. The floor is under 256 triangles, so it is
   lines too; the selected body's bounds are added, 2 cm larger, in magenta.
5. `ServerViewLayer::Show` replaces the two RenderingServer meshes' surfaces with the batches' bytes (Godot lays them
   out as DebugGeometry does, checked once) and moves their instances to the cell's place in the scene.
6. The editor's 3D viewports render the world the edited scene lives in; the layer is in it, with no depth test and
   the highest priority, so the collision shows over the visuals.

## What the tests prove

| Suite | Tests | What it proves (the package's "Done when") |
| --- | --- | --- |
| `DebugMeshLibrary.*`, `DebugGeometry.*` (Core) | 4 | Meshes by ID with a revision per change; a replaced mesh still drawn until synced; instances in a frame, dropped past the capacity; the physics geometry travels apart |
| `PhysicsDebugView.*`, `PhysicsInspection.*` (PHYS-3) | 7 | **Bodies and shapes come out as plain lines and triangles**: a box 8,600 km out spans its size to the millimetre relative to the origin; only what is near; colors by motion and sleep; characters from their feet; **a 512-triangle mesh becomes one cached mesh put once and drawn by an instance**, or lines without a library; bounds and velocities on top; the descriptions. **Physics links no GodotBridge** (its catalog row and the headless check) |
| `PhysicsLayer.*` (GDB-4) | 3 | **Collision shapes line up with gameplay shapes**: a crate's collision box and its gameplay shape within 0.5 mm while the camera walks 9 km out through two rebases; **a cached mesh is uploaded once** however many frames draw it, replaced costs one upload, removed is freed; **the layer toggles** from the API and the overlay, and the server view draws on top |
| `godot.runic_client_physics_view` | 1 | The walk with its collision drawn: one cached mesh uploaded once, the capsule's collision lines within 0.0 mm of its shape over 300 frames |
| `PhysicsInspector.*` (GDT-3) | 10 | A body's properties in world space; the nearest body a ray meets (from above, sideways, past a sphere's silhouette, a turned box by its own shape; a mesh left to the list); the server view over the visuals or depth-tested, relative to its origin; the selected body's bounds on top; a gone body unselected |
| `RunicPhysicsDock.*` (GDT-3, Godot in the test process) | 2 | **Bodies are selectable with their properties**: the dock lists three bodies 8,600 km out, selects one through its list and shows its world position, then picks with a ray as a click does. **The collision draws over the visual**: the layer's lines, read back from Godot, span the floor and the box at their place in the edited scene, with no depth test; depth-tested they move to the other batch |
| `godot.editor_opens_project` (changed) | 1 | The RunicEditor's physics dock lists its 15 bodies and the layer draws them |

Physics passes under AddressSanitizer with UBSan, ThreadSanitizer, clang and a Release build, and with the debug
view built out (`RUNIC_PHYSICS_DEBUG_VIEW=OFF`, its tests skipped); Core and Client under the same; the physics
inspector's model under ASan and UBSan and with clang. The Godot-side sources of GDB-4 and GDT-3, and the two programs'
changed files, compile cleanly with GCC under `-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion`.

## What it costs

Release unless said, far from the origin, one thread, on the 2-core build machine.

| Operation | Cost |
| --- | --- |
| Physics' debug view of a box | 0.36 µs (1,000 boxes: 0.36 ms) |
| An 8,192-triangle terrain | cached: 2 ms once, then 11 µs a frame for its instance; drawn as lines every frame: 200 µs and 49,000 line vertices |
| The client's physics layer in Godot | frame time unchanged (6.9 ms, Godot's headless pacing) with 24,000 line vertices |
| The physics inspector on 500 boxes | listing and inspecting them 0.66 ms (1.3 µs a body); a pick 15 µs; the server view's geometry 0.48 ms (36,000 line vertices) |
| The editor's layer, one refresh | 255 µs for the RunicEditor's 15 bodies, 2.4 ms for 500 moving boxes: extraction and upload to Godot (debug build, the only one with libgodot here) |

A frame's geometry holds 65,536 vertices a batch: about 900 boxes drawn as Jolt's wireframes. Past that, Core counts
what it dropped and the dock says so.

## Decisions that await the owner

| # | Decision taken | Why | The alternative |
| --- | --- | --- | --- |
| D1 | The physics geometry is its own `RenderFrame` geometry, with its own capacity and toggles (a Core addition) | The physics layer toggles apart from the debug shapes, and cached meshes come with it | A channel of `RenderFrame::debug` (RunicDocs' frame has one) |
| D2 | Jolt's debug renderer is in every build unless `RUNIC_PHYSICS_DEBUG_VIEW` is off | The editor and development servers need it; shipped builds can drop it | Only in development builds |
| D3 | Shapes are drawn through Jolt's `Shape::Draw` (Path B), wireframe, faces on request | Exact shapes for free; Path A would redraw every primitive | Physics' own primitives |
| D4 | Colors: static grey, moving blue, asleep dark blue, sensors cyan, characters green, velocities white; faces at alpha 64 | Core's debug palette | Others |
| D5 | A large shape is one of 256 triangles or more, cached once with its wireframe in the color it had first | Terrains and buildings; a crate is cheaper as lines | Another threshold; a color per instance |
| D6 | The client's layer is visible at the start, the server view is not; the server view reuses the overlay material | Collision is what slice 8 shows; on top is a choice | Off at the start |
| D7 | The client's physics extractor lives in RunicClient | A program shows the slice; a Client package should own a reusable `engine:physics_geometry` extractor | A Client extractor now (a Client package) |
| D8 | Until the Cooker (slice 9), the editor's "cooked collision" is the local simulation's own physics, which GDT-4 fills from cooked cells | There is no cooked data before COOK-1 | Wait for slice 9 |
| D9 | The editor's layer draws over the visuals by default; a switch makes it depth-tested | "Draws over the visual": a rock without collision shows | Depth-tested by default |
| D10 | Viewport picking is a dock button and the next forwarded click; any other button gives up | No shortcut taken from the editor's own selection | A modifier on the editor's click |
| D11 | Picking uses the shapes' bounds (exact for boxes and spheres), leaves meshes to the list, until PHYS-5's rays | Physics has no ray query before slice 15 | Wait for PHYS-5 |
| D12 | The editor's layer and list follow the editor camera's 16 m cell, 250 m out | A camera moving inside a cell redraws nothing | The whole world |
| D13 | `EditorToolsSpec::editedSceneOrigin` says where the edited scene stands in the world: the world's origin until region tools | Region tools (GDT-8) open a region at its own origin | Ask now |
| D14 | The dock binds check methods for tests through Godot, and the editor harness is shared by the editor tests | Its class is private; tests use only Godot's reflection and Public headers | Public check methods |

## Notes for other packages and for the plan

- **Two interface changes reported, not made** (one package, one module): GodotBridge's debug layers draw the same
  geometry for the client but are private to GodotBridge and make their own camera current, which the editor's root
  window must not have, so GodotTools' `ServerViewLayer` repeats their upload (about 300 lines). A public "draw this
  DebugGeometry in that scenario" in GodotBridge would share it. And Physics' `ShapeInfo` gives a shape's bounds'
  size but not their centre: a mesh's selection is an arrow onto its origin, not its bounds.
- **The editor's layer runs on the editor's thread between steps.** When PHYS-4 (slice 10) moves Jolt to the worker
  pool, extraction still reads between steps: the rule holds, nothing changes here.
- **What the server view still lacks** (RunicDocs' list for the layer): volumes, spawn points and nav tiles, which
  arrive with their packages; the stream from a development server, beside the local physics.
- **Characters are not in the dock's list** (they are not bodies): the bridge knows them as `PhysicsCharacter`
  entities, a small addition when a slice needs it.
- **Godot's headless editor prints `Index p_which = 0 is out of bounds`** at every start (the Create Plugin dialog
  with no scripting language); the editor tests also print "Can't open dynamic library" for scratch projects without
  an editor library. Both are harmless and were there before.
