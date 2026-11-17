# Runic::Client — module contract

Group: Presentation · Headless: **no** (presentation) · Target: `Runic::Client` (`runic_client`) · Packages: CLI-1 to
CLI-9

Source: RunicDocs › Architecture › Client runtime (Game on Engine); Render data, extraction and world UI (Engine, Game)
› Extraction; Repositories, build targets and processes (Engine modules); Modules, work packages and the
implementation contract (CLI-1, CLI-2); Engine gameplay APIs (Engine) › Mechanism and policy (input mapping with
modifiers). Status: **CLI-1 implemented** (slice 5, on the preview branch `preview/slices-3-11`): the Presentation
layer, the extractor registry, the debug-shape extractor and the client camera. **CLI-2 implemented** (slice 7, same
branch): input mapping, actions, presets and the command builder. Each with its acceptance tests. The folder map is
[README.md](README.md); the slice briefs and walkthroughs are
[preview/slice-05-moving-entities.md](../../../preview/slice-05-moving-entities.md) and
[preview/slice-07-physics.md](../../../preview/slice-07-physics.md).

## Purpose

The client runtime: everything the client does between its world and the screen. CLI-1 builds the Presentation layer,
which once per frame places the camera, runs the registered extractors over the client world and publishes one
RenderFrame of plain render data; only GodotBridge reads that frame. The first extractor draws entities as debug
shapes the presenter keeps between frames. CLI-2 adds the Input layer: the keys and mouse buttons Godot's UI did not
consume become the actions the Game names, through bindings with modifiers and chords in presets the Game defines,
and command builders turn the actions into the commands the simulation applies. Later packages add the Prediction
layer (CLI-3), the mesh, skinned, anchor, light and field extractors with interpolation (CLI-4 to CLI-8)
and the RunicBot runtime (CLI-9).

## Public API

Namespace `Runic::Client`. Headers are included by their path under `Public/`. No library types.

| Header | Holds | Since |
| --- | --- | --- |
| `ExtractorRegistry.hpp` | `ExtractionView` (the frame's origin, the camera's place, the draw distance, the frame number), `Extractor` (`Extract(view, frame)`), `ExtractorInfo`, `ExtractorRegistry` (`Register`, `Emplace`, `Count`, `Describe`, `Find`, `Run`), and the metric names `kExtractTimeMetricPrefix`, `kFramesExtractedMetric` | CLI-1 |
| `DebugShapes.hpp` | The components `DebugShape` (kind, size, offset, color) and `RenderIdentity` (the entity's `RenderInstanceId`), their names `engine:debug_shape` and `engine:render_identity`, `RegisterClientComponents`; `DebugShapeExtractor` (`engine:debug_shapes`) and `DebugShapeStats` | CLI-1 |
| `ClientCamera.hpp` | `CameraLens`, `kDefaultEye`, `kDefaultTarget`, `ClientCamera` (`LookAt`, `Follow`, `StopFollowing`, `SetLens`, `Update`, `Eye`, `Target`, `ViewFrom`) | CLI-1 |
| `PresentationLayer.hpp` | `PresentationLayer` (an `Application::Layer`: `Extractors`, `Camera`, `FramesWritten`), `PresentationLayerSpec` (frames, world, metrics), `kPresentationLayerName` | CLI-1 |
| `InputMapping.hpp` | `ActionId`, `InputDevice`, `InputKey` (`KeyInput`, `MouseInput`), `kMaxChordKeys` (4), `InputBinding` (`Bind`, `Chord`), `InputPreset`, `InputMap` (`AddAction`, `FindAction`, `ActionName`, `ActionCount`, `AddPreset`, `UsePreset`, `ActivePreset`, `Bindings`, `Feed`, `ReleaseAll`, `IsHeld`, `Presses`, `Releases`, `WasPressed`, `WasReleased`, `EndFrame`); `MovementActions`, `MovementDirection`, `WorldMovement` | CLI-2 |
| `InputLayer.hpp` | `InputLayer` (an `Application::Layer`: `Map`, `AddCommandBuilder`), `CommandBuilder`, `kInputLayerName` (`Input`) | CLI-2 |

**The frame rule:** a frame is the camera's. Its origin is the camera's eye, every position in it is a 32-bit offset
from that origin, and the frame alone describes what is drawn: GodotBridge needs nothing else, and nothing above the
Presentation layer reads the world. The layer empties the frame's debug geometry before its extractors run (the slot
still holds what it carried three frames ago); every other kind is emptied by its one writer.

**The shape rule:** an entity is drawn as a debug shape when it has a `World::WorldTransform`, a `DebugShape` and a
`RenderIdentity`, and its shape's center is within the draw distance (the lens's far plane) of the camera. The
extractor writes it through a `RenderData::ShapeInstanceWriter`, so a shape that did not move in the world is
unchanged however the camera moves, and a frame in which nothing moved keeps its revision.

**The binding rule** (CLI-2): a binding holds while every key of its chord (one to four keys or mouse buttons) is
down and at least its modifiers are held. Of the bindings that hold, one that another holding binding covers (whose
keys and modifiers include its own, and more) does not count: Shift+1 is its own action and not also 1's, and the
chord Q+E is not also Q's or E's, while W still walks with Shift held when nothing binds Shift+W. An action is held
while one of its counting bindings holds. Its presses and releases since the last frame are counted, so a tap shorter
than a frame is a press and a release; a key's repeats are not presses. The Engine names no action and binds no key:
actions are registered by `namespace:name`, and the Game's presets bind them; the player picks a preset, and
switching releases everything.

**The input rule** (CLI-2): the Input layer sits below the GodotLayer, which posts only what Godot's UI did not
consume, and it ignores an event a layer above handled; it handles none itself. Losing the window's focus releases
every action, since the releases of keys held then may go to the UI and never come. Once per update the layer runs its
command builders in the order they were added, then ends the map's frame. A builder only pushes commands into the
simulation's command queues: input changes the world only in the Commands phase (SIM-2's command rule).

## Owns

- The Presentation layer's order of work in a frame: the camera, then the frame's origin and view, then the
  extractors in registration order, then the publish.
- The extractor registry: names, order, timings.
- The debug-shape extractor and its two components.
- The Input layer, its `InputMap` (actions, presets, the keys down and the modifiers held, the actions' states) and
  its command builders (CLI-2); the movement helpers that turn four actions into a ground direction relative to the
  camera.
- The client camera: its 64-bit eye and target, following an entity, the lens, and the view's rotation (computed from
  the 64-bit direction, so it is as exact at any distance from the world's origin).

## Does not own

- The RenderData types, the exchange and the shape-instance writer (Runic::Core, CORE-3 and the slice 5 addition).
- Drawing: GodotBridge reads the frames (GDB-2's debug view, GDB-3's presenter).
- The client world's content: in slice 5 a program's local `SimulationRuntime` (Runic::Simulation); later the world
  replication fills (Network, SIM-6) and the Prediction layer (CLI-3).
- Where an entity's `RenderInstanceId` comes from: the program or game that makes the entity; later the replicated
  `GlobalEntityId`.
- Key codes: the GodotLayer passes Godot's (GodotBridge's input sink), and the Game's presets use them. Which key a
  player's keyboard layout puts where is Godot's: a binding to W follows the letter.
- The commands and what they mean: each game or program defines its commands, its builders and the Commands-phase
  system that applies them; camera controls from input are a builder's or a program's (the RunicClient's walk scene
  follows the capsule); interpolation between snapshots (CLI-4, slice 22).

## Allowed dependencies

Catalog row: Application, Simulation, Network, Perception, Environment, Water, Animation, Audio, Assets. CLI-1 links
Application and Simulation (PUBLIC: the layer is an `Application::Layer`, and the client world, its components and
its queries are the Simulation's ECS and World types, which `Runic::Simulation` exposes publicly). Core's RenderData
comes through Application.

## Forbidden dependencies

Everything outside the row: GodotBridge and Godot (the Client never includes a Godot header: it writes plain data),
Physics, the other SimulationCore and WorldSystems modules except through the Simulation, the Tools, RunicPlatform and
RunicGame. No third-party library.

## Threading and tick model

- The Presentation layer runs on the thread that runs the application's loop, after the layers below it (the
  Simulation layer, later Network and Prediction) and before the GodotLayer. It is the exchange's one writer.
- Extractors read the world between simulation steps, when nothing changes it; they never write it. Their queries
  hold changes back while they iterate, so a stray write would wait, but none is made.
- Parallel extraction on the worker pool, into per-worker arenas merged in a fixed order, is a later extension
  (RunicDocs › Extraction › Parallel); CLI-1's extractors run one after another.
- The Input layer runs on the same thread: events are dispatched to it top-down at the start of a frame, its builders
  run in its update. Mapping an event allocates nothing once the map has seen as many keys down at once as it will.
- A frame allocates nothing once the extractors are warm: the frame's storage is reserved when the exchange is made,
  and the shape writer's when the extractor is made.

## Data ownership

The layer owns its registry, its camera and its count of frames. The registry owns its extractors. The exchange, the
world and the metrics registry belong to the program and outlive the layer. A frame belongs to the exchange; what an
extractor writes into it stays valid until the slot is written again.

## Integration points

- A client program makes a `RenderData::RenderFrameExchange`, pushes a `PresentationLayer` above its world's layers
  and below the GodotLayer, hands the GodotLayer the same exchange, and registers extractors at setup: the Engine's
  `DebugShapeExtractor` under `kDebugShapeExtractorName`, and its own under `game:` names.
- Entities become visible by getting a `DebugShape` and a `RenderIdentity` (`RegisterClientComponents` registers
  them).
- A client program pushes an `InputLayer` below the GodotLayer, registers its actions and presets on `Map()`, picks a
  preset and adds its command builders; each builder pushes into a `Simulation::CommandQueue` drained in the Commands
  phase.
- The camera is placed with `LookAt` or made to follow an entity with `Follow`.
- Metrics, when a registry is given:

| Metric | Kind | Domain label | Meaning |
| --- | --- | --- | --- |
| `client.extract_time.<extractor>` | Timing | none | Time one extractor took to write a frame |
| `client.frames_extracted` | Counter | none | Frames the extractors wrote |

## Where the code lives

- `Source/PresentationLayer/PresentationLayer.cpp`: the layer's frame.
- `Source/Extraction/ExtractorRegistry.cpp`: the registry; `Source/Extraction/DebugShapeExtractor.cpp`: the
  components' registration and the extractor (one cached query over the three components; a distance cull against
  the camera; one `ShapeInstanceWriter`).
- `Source/Camera/ClientCamera.cpp`: the camera. A view straight up or down takes -Z as its up, as a map seen from above.
- `Source/InputMapping/InputMap.cpp`: the actions, presets and bindings, `Bind`, `Chord`, and the movement helpers;
  after every key or button change it lists the bindings that hold, drops the covered ones and counts each action's
  presses and releases. `Source/InputMapping/InputLayer.cpp`: the layer.

## Work packages

| ID | Package | Done when | Status |
| --- | --- | --- | --- |
| CLI-1 | Presentation layer and extractor registry; the first extractor writes entities as debug shapes; camera | Entities are drawn from the RenderFrame only | **Implemented** (slice 5, preview branch): `RunicClientTests` passes with the 21 tests below (18 where asserts are compiled out); awaiting the owner's review |
| CLI-2 | Input mapping, actions and command builder over the AppEvents the GodotLayer forwards: modifiers, chords, press and release events, and the layouts and presets the Game defines | Key presses become movement commands; a modifier plus a key maps to its own action; input Godot's UI consumed never reaches them | **Implemented** (slice 7, preview branch): `RunicClientTests` passes with the 9 tests below added; RunicClient's CTests `godot.runic_client_input` (a click on Godot's button never becomes the click action; one beside it does) and `godot.runic_client_walk` pass; awaiting the owner's review |
| CLI-3 | Prediction layer | | Not started (slice 14) |
| CLI-4 to CLI-8 | Mesh, skinned, anchor, light and field extractors | | Not started (slices 22, 23, 23a, 25a, 42a) |
| CLI-9 | RunicBot runtime | | Not started (slice 38) |

CLI-1's acceptance tests (`RunicClientTests`): `ExtractorRegistry.*` (each extractor runs once per frame in
registration order; found and described by name; timings and frames recorded) and `ExtractorRegistryDeathTest.*`;
`DebugShapeExtractor.*` (an entity thousands of km from the world's origin is drawn at its place relative to the
camera, with its ID, kind, size and color; the offset turns with the entity; only entities with all three components
are drawn; the draw distance; a still world keeps its revision while the camera moves; a moving entity is listed as
changed); `ClientCamera.*` (the default view; exact far from the origin; straight down; following an entity until it
is gone) and `ClientCameraDeathTest.*`; `PresentationLayer.*` (one frame per update, seen from the camera; debug
geometry emptied every frame; the frame alone describes what is drawn, after the world is destroyed; the camera
follows an entity of the world; entities steered by commands in a `SimulationRuntime` are drawn where the
simulation put them) and `PresentationLayerDeathTest.*`.

CLI-2's acceptance tests (`RunicClientTests`): `InputMapping.*` (**key presses become movement commands the
simulation applies in its Commands phase, relative to the camera, diagonals of length 1; a modifier plus a key maps to
its own action, and a key with a modifier nothing binds still maps; a chord is its own action and not also its keys**;
a tap shorter than a frame is not lost and repeats are not presses; mouse buttons bind like keys; **input a layer above
handled never reaches the actions**; losing focus releases every action; presets are checked and switching releases
everything; movement follows the camera's heading 8,600 km out).

Decisions taken in CLI-2 that await the owner (also in the slice brief): the more specific binding wins (rather than an
exact modifier match); a chord holds while its keys are down, in any order; "layouts" are read as keyboard layouts,
which need nothing here because Godot passes the logical key (a binding by physical position would need GodotBridge to
pass the physical code too, an interface change); a preset is one flat list of bindings and one preset is active
(contexts that stack, such as a vehicle's bindings over the walk's, are left for when a slice needs them); the Input
layer handles no event; focus loss releases everything.

Decisions taken in CLI-1 that await the owner (also in the slice brief): a frame's origin is the camera's eye; the
draw distance is the lens's far plane; an entity's `RenderInstanceId` is a component (`RenderIdentity`) its maker
fills, until replication gives every entity a `GlobalEntityId`; the camera's up is +Y, and -Z when it looks straight
down.

## Future extensions

- Extractors on the worker pool, into per-worker arenas merged in a fixed order.
- Change-driven extractors (`ExtractRate::OnChange` in RunicDocs) for slow sources: fields, water, environment.
- A per-kind cap and frustum culling in each extractor, beyond the distance cull.
- Camera controls from input and a smoothed follow.
- Stacked input contexts (a mount's or a menu's bindings over the walk's), rebinding saved per player, axes from
  gamepads and the mouse.
