# Runic::GodotBridge — module contract

Group: Presentation · Headless: **no** · Target: `Runic::GodotBridge` (`runic_godotbridge`) · Packages: GDB-1 to GDB-13

Source: RunicDocs › Architecture › Godot integration (Engine), Repositories, build targets and processes
(Engine modules), Render data, extraction and world UI, Debug drawing (Engine). Status: **GDB-1 implemented** (slice 1)
on the slice 0 LibGodot host (RunicDocs › Architecture › Godot integration (Engine) › Slice 0 spike results (GDT-1));
**GDB-2 implemented** (slice 2): the debug view, the development overlay and their acceptance tests; awaiting the
verify step. **GDB-3 implemented** (slice 5, on the preview branch `preview/slices-3-11`): the presenter, with its
acceptance tests; slice 5's brief and walkthrough are
[preview/slice-05-moving-entities.md](../../../preview/slice-05-moving-entities.md). **GDB-4 implemented** (slice 8,
same branch): the physics geometry layer, cached debug meshes and "see what the server sees", with their acceptance
tests; slice 8's brief is [preview/slice-08-physics-view.md](../../../preview/slice-08-physics-view.md). The folder map is [README.md](README.md); the slice 2 brief is
[RunicEngine#145](https://github.com/PedroVicente98/RunicEngine/issues/145) and the plan
[RunicEngine#11](https://github.com/PedroVicente98/RunicEngine/issues/11).

## Purpose

The only Engine module that talks to Godot at runtime. Godot is interface and renderer only: GodotBridge owns the
LibGodot instance, the window and input it provides (the GodotLayer), the presenter that applies the RenderFrame
to the RenderingServer, the debug layers, world UI, the view-model binder and the audio backend on the
AudioServer. The RenderFrame is the only door from the Runic client into Godot.

## Public API

| Header | Types | Since |
| --- | --- | --- |
| `GodotRuntime.hpp` | `GodotRuntime` (load, create, start, iterate, destroy one LibGodot instance), `GodotTarget` (editor or template library), `GodotInitLevel` and level hooks for class registration | slice 0 |
| `GodotLayer.hpp` | `GodotLayer` (a `Runic::Application::Layer`: starts Godot in `OnAttach`, runs one Godot frame in `OnRender`, posts unconsumed input as AppEvents, asks the host to stop when Godot quits), `GodotLayerSpec` (library target, arguments, input forwarding), `ScriptedClick` and `ScriptedKey` (synthetic input for automated checks, released `kReleaseAfterFrames` after the press), `kGodotLayerName` | GDB-1 |
| `GodotLayer.hpp` | `GodotLayerSpec::frames` (the `RenderData::RenderFrameExchange` the layer reads) and `developmentOverlay`; `GodotLayer::OnUpdate` (applies the newest frame), `SetDebugLayerVisible`, `IsDebugLayerVisible`, `DebugStats`, `ReadBackDebugSurface` (what Godot holds, decoded, for checks) | GDB-2 |
| `GodotLayer.hpp` | `GodotLayer::PresenterReport` (what the presenter did with the last frame) and `ReadBackInstance` (an instance as the presenter gave it to Godot, for checks) | GDB-3 |
| `GodotLayer.hpp` | `GodotLayer::ReadBackPhysicsSurface` (the physics layer's surfaces in Godot's space), `ReadBackMeshInstance` (a cached mesh's instance as drawn), `PhysicsDrawnOnTop` | GDB-4 |
| `GodotPresenter.hpp` | `kRebaseDistanceM` (4 km), `PresenterStats` (frame, instances, created, updated, freed, RenderingServer calls, whether the frame was matched by ID, apply time, rebases, the presenter's origin), `PresentedInstance` | GDB-3 |
| `DebugView.hpp` | `DebugLayer` (`DepthShapes`, `OverlayShapes`, `Text`), `kDebugLayerCount`, `DebugLayerLabel`, `DebugViewStats`, and the scene nodes the view adds (`kDebugViewNodeName`, `kDebugCameraNodeName`, `kDevelopmentOverlayNodeName`, `DebugLayerToggleName`) | GDB-2 |
| `DebugView.hpp` | `DebugLayer::PhysicsGeometry` and `DebugLayer::ServerView` (`kDebugLayerCount` is 5), their labels and toggles; `DebugViewStats::physicsLineVertices`, `physicsTriangleVertices`, `meshInstances`, `cachedMeshes`, `meshUploads` | GDB-4 |
| `GodotArguments.hpp` | The Godot flags Runic programs read or add (`kPathFlag`, `kHeadlessFlag`, `kEditorFlag`, `kEditorShortFlag`, `kEditorPidFlag`) and `InsertProjectPath` | GDB-1 |

No Godot or godot-cpp type appears in a public header: the catalog row has no public exception, and
registration hooks use Runic types (`GodotInitLevel`). GodotTools, whose public headers may carry godot-cpp
types, registers its classes through these hooks.

## Owns

- The process's single LibGodot instance: the opened library, its `libgodot_create_godot_instance` and
  `libgodot_destroy_godot_instance` entry points, the argument storage Godot keeps pointers into, and the
  GDExtension entry point of the host executable (`libgodot://main`).
- The GodotLayer (GDB-1) at the top of the client LayerStack: the window from the DisplayServer (none with
  `--headless`), one Godot iteration as the last step of each frame, and the input sink, a node at the scene tree's
  root (`Source/Input`) whose `_unhandled_input` receives only what Godot's UI did not consume and posts it as
  key, mouse-button, mouse-motion and wheel AppEvents, with window focus and resize.
- The debug view (GDB-2, `Source/Debug`): from each new RenderFrame, one RenderingServer mesh per batch (a lines and a
  triangles surface, rebuilt from DebugGeometry's streams copied as bytes: no per-vertex work), two unshaded
  vertex-color materials (depth-tested; overlay without depth test and on top), a Camera3D set from the frame's camera,
  and a pool of Label3D for text. The debug layers (depth-tested shapes, overlay shapes, text) and their visibility.
- The development overlay (GDB-2): a CanvasLayer panel at the window's top-right with one CheckBox per debug layer and
  a line of what the last frame drew; off unless the spec asks for it.
- The presenter (GDB-3, `Source/Presenter`): one RenderingServer instance per shape instance of the frame, found by
  its `RenderInstanceId`, with no node per instance; unit meshes per shape kind (a box, a sphere, a cylinder, scaled
  to the shape's size; a capsule mesh per size to the millimetre, so its ends stay round); one material per color
  (transparent when its alpha is under 255). And Godot's space: the presenter's own 64-bit origin, which is Godot's
  (0, 0, 0), moved to the frame's origin only when that is more than `kRebaseDistanceM` away.
- The physics geometry layer (GDB-4, `Source/Debug`): the frame's physics geometry (`RenderFrame::physics`) in its
  own two batch meshes with their own materials, visible by its own toggle, moved by the same shift as everything
  else; the cached debug meshes (`DebugMeshCache`): one RenderingServer mesh per mesh of the frame stream's library,
  uploaded when it appears or changes, freed when it goes, and a pool of instances placed at each frame's mesh
  instances (of the debug shapes and of the physics layer); "see what the server sees", which draws the physics layer
  with the overlay material, on top of everything. Its two toggles join the overlay.
- Later packages: mesh and skinned instances, world UI, binder, audio backend.

## Does not own

- Which library a process loads, and its arguments: the host executable decides (RunicEditor: editor target;
  GameClient: editor target in development, template when shipping).
- What a frame holds and who writes it: RenderData is Runic::Core's (CORE-3, and the slice 5 shape instances); the
  client writes frames (CLI-1's Presentation layer), and the program owns the `RenderFrameExchange`.
- What changed in a frame: the shape writer says (`ShapeInstanceList`'s revisions and changed indices); the presenter
  trusts it and never compares a frame it does not have to.
- Building libgodot: `cmake/RunicGodot.cmake` and `scripts/godot/` (BUILD-1).
- Editor tools, plugins and docks: Runic::GodotTools.
- Any game rule, simulation or physics state: Godot never reads Flecs, Jolt or domain modules. What the physics layer
  draws is Physics' extraction (PHYS-3) into plain debug geometry, written by the client's extractor.

## Allowed dependencies

Catalog row (ceiling): Runic::Application, Runic::Client, Runic::Audio, Runic::Assets; third parties libgodot,
godot-cpp (private) and the system loader (`dl`). GDB-1 links Runic::Application PUBLIC (`GodotLayer` derives from
its `Layer`), which also brings Runic::Core's types; the row does not allow Core directly, so the module never links
it itself.

## Forbidden dependencies

Physics, Simulation or any domain module state (the presenter draws an extracted frame); GodotTools or Godot's
editor classes in code the shipping client runs; Platform and Game targets. No headless target may reach this
module: it is marked presentation and the configure-time headless check fails on any path to it.

## Threading and tick model

Every `GodotRuntime` call happens on the thread that called `Create()` (Godot's scene tree is not thread-safe);
the class refuses calls from another thread. The debug view runs on that thread too: the GodotLayer is the reader of
the frame exchange, and the writer may be another thread (CORE-3 guarantees neither waits). One `Iterate()` is one Godot frame; in the client it is the last
thing the frame does: the GodotLayer is the top layer, and its `OnRender` runs after every other layer's. Input
posted from inside Godot's frame is dispatched at the start of the next host frame. LibGodot allows one instance per process and never a second one after the first
is destroyed; `Create()` reports both cases.

## Data ownership

The host owns the `GodotRuntime` object. The library stays mapped after `Destroy()` because Godot registers exit
handlers and thread-locals inside it. godot-cpp's bindings are invalid after `Destroy()`.

## Integration points

- `AddInitializeHook` / `AddDeinitializeHook`: run inside Godot's initialization once per level from Scene up
  (Editor only with the editor library), and in reverse on shutdown. GodotTools registers the RunicEditor's
  classes here; the client registers presentation helpers here from GDB-1.
- Arguments reach Godot unchanged, because Godot's project manager and Play buttons relaunch the host executable
  with their own arguments.
- `GodotLayer::Runtime()` lets a program add registration hooks before the layer attaches (the RunicEditor calls
  `GodotTools::RegisterEditorTools` on it). `GodotLayerSpec::scriptedClicks` and `scriptedKeys` push synthetic input
  into the root viewport for automated checks (RunicClient's `--input-check`).
- `GodotLayerSpec::frames`: the program hands the layer the frames to show; the layer applies the newest one in
  `OnUpdate`, after every layer below has written its part, and Godot draws it in `OnRender`.

## The presenter's rules (GDB-3)

- **Nothing changed, nothing done.** A frame whose shape revision the presenter already applied costs one comparison
  and no RenderingServer call, however many instances there are and however far the camera moved.
- **Only what changed.** When the frame's base revision is the one the presenter applied and the layout did not
  change, it visits the frame's changed indices only: a move or a turn is one `instance_set_transform`, a new color
  one `instance_geometry_set_material_override`, a new kind or size one `instance_set_base` and a transform.
- **By ID otherwise.** When the layout changed (shapes added, removed or reordered) or the presenter missed a frame,
  it matches every shape by ID: new IDs are created (`instance_create2`, a material, a transform), missing ones freed,
  and the rest updated only when they differ. On this path a place within 1 mm of where the presenter put the shape is
  the same place: the difference is the rounding of two frame origins. Two shapes of one frame with the same ID are a
  fault (asserted; without asserts the first wins).
- **Godot's space.** A frame is relative to the camera's eye, which moves every frame; Godot's is relative to the
  presenter's origin. The shift between the two (under `kRebaseDistanceM`) moves the debug view's root node (the
  camera and the labels) and both debug mesh instances, so no vertex is touched (slice 2 decision D7). An instance's
  Godot position is its 64-bit place minus the presenter's origin, computed in double precision and converted once.
- **The rebase.** When the frame's origin is more than 4 km from the presenter's, the presenter's moves to it and
  every instance gets one new transform. Near the camera, positions inside Godot then stay within 4 km, where a
  float's step is under 0.5 mm; a shape at the edge of a frame (10 km from the camera) is at most 14 km out, a 1 mm
  step, far from the eye.

## The physics geometry layer's rules (GDB-4)

- **Collision lines up with what it collides for.** The physics geometry is relative to the frame's origin like every
  shape, and the layer's meshes move by the presenter's shift, so a collision box and its gameplay shape stay
  together to the presenter's precision across rebases (measured: under 0.5 mm over a 9 km walk with two rebases).
- **Large shapes cross once.** A cached mesh is uploaded when the library's revision says it appeared or changed
  (cleared and refilled in place, so its instances keep their base), never per frame; each frame places the pool's
  instances (a transform each, a material when it changes) and hides the rest.
- **One toggle, one switch.** The physics geometry layer shows or hides its meshes and its instances; "see what the
  server sees" (off at the start) gives its depth-tested batch the overlay material, so collision draws over the
  visuals.

## Work packages

| ID | Package | Done when | Status |
| --- | --- | --- | --- |
| (slice 0) | LibGodot host for the slice 0 spike: `GodotRuntime` | The client and the RunicEditor start Godot through it; `GodotRuntime.StartsGodotHeadlessAndExits` passes when libgodot is built | Done in slice 0 |
| GDB-1 | GodotLayer: the LibGodot instance at the top of the client LayerStack; window from the DisplayServer; input forwarded as AppEvents after Godot's UI; headless mode; the Engine's RunicClient program, and the RunicEditor on the entry point | RunicClient starts Godot windowed and headless and exits cleanly; the RunicEditor still opens and exits cleanly; UI clicks never reach the stack | **Implemented** (slice 1): `GodotLayer.*` unit tests, `godot.runic_client_headless`, `godot.runic_client_input`, `godot.editor_spike`, `godot.editor_project_manager`; the windowed run is the owner's (slice 1 check 7) |
| GDB-2 | Debug drawing on Godot: Core's DebugGeometry uploaded as immediate meshes with depth-tested and overlay materials; toggled layers; development overlay panel | Every debug shape and text renders batched in one frame; layers toggle | **Implemented** (slice 2): `DebugViewTests` (6, headless: Godot's layout, the read-back, one frame batched, the toggles, the camera, no frames); `godot.runic_client_headless` expects the debug view's line; decisions D4, D5 and D7 in RunicEngine#145; the windowed run is the owner's (check 7) |
| GDB-3 | Presenter: RenderFrame instances created, moved and freed in the RenderingServer by RenderInstanceId; presenter origin with a rare rebase (about every 4 km) | 50,000 static instances cost nothing per frame unless they change; a 20 km walk shows no jitter | **Implemented** (slice 5, preview branch): `PresenterTests` (6, headless, below), `DebugViewTests` still pass with the shift, and RunicClient's `godot.runic_client_moving_entities`; awaiting the owner's review |
| GDB-4 | Physics geometry layer: DebugGeometry from PHYS-3 with cached debug meshes for large shapes; the "see what the server sees" overlay | Collision shapes line up with gameplay shapes and the layer toggles | **Implemented** (slice 8, preview branch): `PhysicsLayerTests` (3, headless, below), the other GodotBridge tests unchanged, and RunicClient's `godot.runic_client_physics_view`; awaiting the owner's review |
| GDB-5 to GDB-13 | Mesh and skinned instances, world UI, binder, audio backend, production look | See RunicDocs › Modules, work packages | Not started |

GDB-3's acceptance tests (`PresenterTests.cpp`, each in its own process with Godot headless): `Presenter.*` (one Godot
instance per shape at its size, with a capsule mesh of its own size and a transparent color; 50,000 still shapes cost
no RenderingServer call in 30 frames while the camera moves, measured at under a microsecond per frame; only the shapes
that changed are touched, a recolor costs its material only; what leaves the frame is freed and what arrives is made,
matched by ID; a frame the GodotLayer skipped is caught up by ID; a 20 km walk far from the world's origin keeps the
error of what the renderer sees under 0.5 mm (measured: 0.06 mm), with four rebases).

GDB-4's acceptance tests (`PhysicsLayerTests.cpp`, each in its own process with Godot headless): `PhysicsLayer.*`
(**a crate's collision box and its gameplay shape stay within 0.5 mm of each other while the camera walks 9 km far
from the origin, through two rebases**; **a cached mesh is uploaded once however many frames draw it**, its
instances follow their places, a replaced mesh costs one more upload and a removed one is freed; **the layer toggles
from the API and the overlay**, and "see what the server sees" draws it on top).

Decisions taken in GDB-4 that await the owner (also in the slice brief): the physics geometry is its own RenderFrame
geometry (a slice 8 Core addition) rather than a channel of the debug geometry; the layer is visible at the start and
the server view is not; the server view reuses the overlay material; cached meshes' instances of the debug shapes
follow their batch's toggle; a cached mesh is culled by its own bounds, the frame batches never.

Decisions taken in GDB-3 that await the owner (also in the slice brief): shape instances are a slice 5 addition to
Core (the plan's first instance kind is CORE-4's, slice 22); a rebase is triggered strictly past 4 km; on a match by
ID a place within 1 mm is not a move; capsules get a mesh per size to the millimetre; shape materials are lit, not
unshaded, so their form reads.

## Future extensions

- Repeated meshes of a cell as one MultiMesh (GDB-5), and mesh and skinned instances beside the shapes, sharing the
  presenter's table and origin.
- A budget per frame for creations, so a frame that brings thousands of new instances spreads them over frames.
- The "see what the server sees" layer fed from a development server's stream (RunicDocs: the RunicEditor draws
  debug geometry streamed from development and staging servers), beside the local physics.

Restarting Godot in the same process and drawing into a host-owned surface are still being merged upstream;
neither is needed while Godot owns the window. Shipping clients should link godot-cpp built for
`template_release` (`RUNIC_GODOT_CPP_TARGET`), so no editor-flavored binding code ships to players.
