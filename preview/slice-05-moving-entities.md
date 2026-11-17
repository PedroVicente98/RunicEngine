# Slice 5 · Moving entities

**Runnable result:** entities move on a plane through commands and are drawn.
**Packages:** SIM-2 (Runic::Simulation), CLI-1 (Runic::Client, a new module), GDB-3 (Runic::GodotBridge), and an
addition to Runic::Core that the plan does not list (shape instances; see D7).
**Status:** done on the preview branch; 62 new tests (22 SIM-2, 12 Core, 21 CLI-1, 6 GDB-3, 1 program CTest). The
headless build runs 321 tests and the debug build (with Godot) 369, all passing.

```sh
cmake --preset debug && cmake --build --preset debug && ctest --preset debug
./build/debug/bin/RunicClient                              # a window: 16 walkers and 4 crates on the slice 2 plane
./build/debug/bin/RunicClient --headless --quit-after 300  # the same, headless; the log says what happened
ctest --test-dir build/debug -R "Presenter|godot.runic_client_moving_entities" --output-on-failure
```

Headless, the run ends with:

```text
RUNIC: presenter stopped at frame 300: 20 instances, 0 rebases
RUNIC: debug view stopped at frame 300: 972 line vertices, 12 triangle vertices, 9 texts in 3 surfaces
RUNIC: moving entities: 16 walkers steered by 27 commands, 74.4 m walked
RUNIC: Simulation layer ran 124 master ticks (hz60 124, hz30 62, hz20 42, hz10 21), 0 owed
```

In a window: the slice 2 scene (grid, shapes, labels) in the middle, sixteen capsules walking squares on a ring
around it, a box at each corner of the plane, the camera circling once a minute.

## What was built

| Package | What | Where |
| --- | --- | --- |
| SIM-2 | The simulation's world (entities, states, event hub) owned by the runtime; the seven realtime phases with a barrier after each; `CommandQueue<T>` | `Engine/SimulationCore/Simulation`: `SimulationRuntime.hpp`, `CommandQueue.hpp`, `Source/Phases`, `Source/Commands`, `Source/Scheduler/RuntimeState.hpp` |
| (Core addition) | `RenderInstanceId`; shape instances in the RenderFrame, with a writer that works out what changed | `Engine/Foundation/Core`: `PlainIds.hpp`, `RenderData/ShapeInstances.hpp`, `RenderFrame.hpp`, `Source/RenderData/ShapeInstances.cpp` |
| CLI-1 | The Presentation layer, the extractor registry, the debug-shape extractor and its components, the client camera | `Engine/Presentation/Client` (a new module) |
| GDB-3 | The presenter: shape instances in the RenderingServer by ID, its own origin and the rebase; the debug view moved by the shift | `Engine/Presentation/GodotBridge`: `GodotPresenter.hpp`, `Source/Presenter`, `Source/Debug/DebugLayers.cpp`, `GodotLayer.cpp` |
| (program) | RunicClient's stack for the slice: a local simulation, the walkers, the Presentation layer; slice 2's scene as an extractor | `Engine/Programs/RunicClient`: `Main.cpp`, `MovingEntities.cpp`, `DebugDemo.cpp`, `CMakeLists.txt` |

## The order to read it in

1. **`Simulation/Public/SimulationRuntime.hpp`**: the comment at the top, then `RealtimePhase` and the new methods
   (`Entities`, `States`, `EventHub`, `IsRunning`, the command-queue listing).
2. **`Simulation/Source/Phases/Phases.cpp`** (40 lines): a phase is its systems inside one `DeferScope`, then
   `hub.Flush()`. That is the whole barrier.
3. **`Simulation/Source/Scheduler/SimulationRuntime.cpp`**, `RunMasterTick`: a barrier first, then the slots; the
   realtime slot runs phase by phase and records which phase runs.
4. **`Simulation/Public/CommandQueue.hpp`**: `Push` appends under a mutex; `Drain` checks the phase, swaps the two
   vectors under the lock and applies outside it.
5. **`Core/Public/RenderData/ShapeInstances.hpp`**: the comment at the top is the contract between the writer and
   the presenter. Then `Source/RenderData/ShapeInstances.cpp`, `Add` and `End` (the change detection).
6. **`Client/Public/ExtractorRegistry.hpp`, `DebugShapes.hpp`, `ClientCamera.hpp`, `PresentationLayer.hpp`**, then
   `Source/PresentationLayer/PresentationLayer.cpp` (one frame, 25 lines) and
   `Source/Extraction/DebugShapeExtractor.cpp` (one query, a distance cull, one writer).
7. **`GodotBridge/Public/GodotPresenter.hpp`**, then `Source/Presenter/Presenter.cpp`: `Apply` decides between
   nothing, the changed list and a match by ID; `Rebase`, `Write` and `Place` do the RenderingServer calls.
8. **`GodotBridge/Source/GodotLayer/GodotLayer.cpp`**, `OnUpdate`: the presenter first, its shift to the debug view.
9. **`Programs/RunicClient/Main.cpp`** (the stack) and **`MovingEntities.cpp`** (the three systems).

## One walker's turn, traced

A walker reaches a corner of its square.

1. **20 Hz producers, master tick k.** `engine:demo_driver` reads the walker's `WorldTransform` (read-only) and sees
   it within 10 cm of its corner. It does not change the walker: it pushes `Steer{walker, velocity}` into the
   `engine:demo_steer` queue. That is the only door.
2. **The barrier after the producers' phase** flushes the hub; the world had nothing held back.
3. **Realtime slot, Commands phase, same master tick** (the realtime slot runs after the producers).
   `engine:demo_steering` calls `Drain`: the queue swaps its incoming list out under the lock and applies the
   command, writing the walker's `DemoVelocity` in place.
4. **Movement phase.** `engine:demo_movement`'s query moves the walker: 64-bit position plus velocity × dt (1/30 s),
   and turns it to face its way.
5. **The rest of the master step**, then the Simulation layer returns.
6. **The Presentation layer, same frame.** The camera's `LookAt` (set by the moving-entities layer just below)
   becomes the frame's origin. `engine:debug_shapes` walks its query: the walker's centre is its position plus its
   0.9 m offset, turned with it; it is within the draw distance; `ShapeInstanceWriter::Add` compares it with the
   same index of the last frame, in world terms, and lists its index as changed. `End` gives the list revision r+1,
   base r. The frame is published.
7. **The GodotLayer, same frame.** `ReadNewest` gets the frame. The presenter checks the origin (under 4 km from its
   own: no rebase), sees revision r+1 with base r, which it applied, and no layout change: it visits the one changed
   index, computes the walker's 64-bit place minus the presenter's origin, converts once, and calls
   `instance_set_transform`. The other fifteen walkers moved too, so they are in the changed list as well: sixteen
   RenderingServer calls for the frame. The four crates are not in it and cost nothing. The debug view's root node
   and its two mesh instances move by the frame's shift, the camera node with them.
8. **`OnRender`.** Godot draws.

## What the tests prove

| Suite | Tests | What it proves (the package's "Done when") |
| --- | --- | --- |
| `RealtimePhases.*` | 4 | **Commands apply in phase order**: seven phases always in their order whatever order systems were added; a phase's systems in their order; the realtime slot between producers and consumers; `IsRunning` |
| `Barriers.*` | 7 | **Facts are visible to the next phase**, not in their own; every other slot is one phase; a fact from between steps reaches the first phase; a component added and an entity destroyed in a phase take effect at its barrier; state facts arrive as events; event IDs are the same on every run |
| `CommandQueue.*`, `CommandQueueDeathTest.*` | 10 | Commands change the world in the Commands phase in queue order, before Movement reads it; a command queued after a realtime tick waits for the next; one queued during a drain waits for the next drain; 20,000 commands from four threads while the simulation ticks, each thread's in order; the listing; the metric; no drain outside the Commands phase, between steps or from another slot; the runtime outlives its queues |
| `SimulationRuntime.OwnsAnEntityWorld…` | 1 | The runtime's world has World's components |
| `ShapeInstances.*`, `ShapeInstancesDeathTest.*` | 12 | Unchanged shapes keep the revision while the origin moves; moves, recolors, resizes, new kinds and turns are listed by index; additions, removals and reorders change the layout; capacity and reach; a reader that skipped a frame can tell; no allocation once made |
| `ExtractorRegistry.*`, `DebugShapeExtractor.*` | 10 | Extractors run in order, are found by name, timed; an entity thousands of km out is drawn at its place relative to the camera; the offset turns with it; only complete entities are drawn; the draw distance; a still world keeps its revision while the camera moves |
| `ClientCamera.*`, `PresentationLayer.*` and their death tests | 11 | **Entities are drawn from the RenderFrame only**: the frame still describes everything after its world is destroyed; one frame per update seen from the camera; debug geometry emptied every frame; following an entity; entities steered by commands in a `SimulationRuntime` drawn where the simulation put them |
| `Presenter.*` | 6 | **50,000 static instances cost nothing per frame unless they change**: 30 frames with the camera moving, 0 RenderingServer calls each. **A 20 km walk shows no jitter**: what the renderer sees stays within 0.06 mm of the exact place (limit 0.5 mm), with 4 rebases. Also: instances at their size; only what changed is touched; freed and made by ID; a skipped frame caught up |
| `godot.runic_client_moving_entities` | 1 | The runnable result, headless: 20 instances in Godot, commands applied, ticks run |

The Simulation, Core and Client tests pass with GCC in Debug and Release, with clang and warnings as errors, and
under AddressSanitizer and UBSan; the Simulation and Core tests also under ThreadSanitizer. The GodotBridge tests and
the Godot CTests pass with GCC in Debug (the only build here with Godot); their files, and RunicClient's, also compile
cleanly with GCC and clang under `-Wall -Wextra -Wpedantic -Werror`.

## What it costs

SIM-2 and CLI-1 in Release, the same shared 2-core machine as before:

| Operation | Cost |
| --- | --- |
| A master step with no systems | 34 ns |
| A used phase (its `DeferScope` and its flush) | about 25 ns: seven phases with one empty system each add about 180 ns to a realtime tick |
| A component added or removed in a phase, applied at its barrier | 65 to 71 ns per entity |
| An event emitted, flushed at the barrier and read in the next phase | 1.6 ns |
| `CommandQueue::Push`, one thread | 3.7 ns |
| `Drain` and apply (one `Get` and an add) | 10 ns per command |
| Push from four threads while the simulation drains (2 cores) | 25 ns per command, end to end |
| Allocations once warm (2,000 ticks of 1,000 commands) | 0 |
| A Presentation frame (camera, query, cull, change detection, write) | 30 to 36 ns per entity: 1,000 in 36 µs, 10,000 in 0.31 ms, 50,000 in 1.5 ms |

GDB-3, measured with the presenter's own clock in the Debug build (the presenter at -O0, Godot's editor library
optimized), 50,000 shapes:

| Frame | Cost |
| --- | --- |
| Nothing changed (the camera moved) | 0.7 µs, 0 RenderingServer calls |
| One shape moved | 10 µs, 1 call |
| 500 shapes moved | 0.56 ms, about 1.1 µs each |
| One shape added: every shape matched by ID | 4.3 ms |
| The first frame: 50,000 created | 153 ms |
| A rebase: every instance moved once | 39 ms, 50,005 calls |

What it means: the presenter's promise holds. The cost of the frame now sits in extraction (1.5 ms at 50,000
entities, every frame, in Release), which is where RunicDocs plans to cut it: extractors on the worker pool and
change-driven extraction. Two numbers to keep in mind: a match by ID costs milliseconds at 50,000 instances, so a
frame that adds or removes shapes is not free; and a rebase with 50,000 instances is a visible hitch (D14).

Precision: at 4 km a float's step is 0.49 mm, at 20 km 1.95 mm; that is why the presenter rebases.

## Decisions that await the owner

| # | Decision taken | Why | The alternative |
| --- | --- | --- | --- |
| D1 | Every slot other than the realtime one is a single phase with a barrier after it | Producers' facts must be readable by the realtime slot of the same tick | Barriers only between realtime phases |
| D2 | A barrier opens every master step | Facts emitted between steps (a service result at a safe point) reach the step's first phase | They wait for the end of the first phase |
| D3 | A value written into an existing component changes in place during a phase; only changes that would move an entity are held back | Runic::ECS's rule (slice 4, D3); the walker's velocity and position are such writes | Every write held back to the barrier |
| D4 | A command queue is drained by its owner in a system of the Commands phase (asserted); it is unbounded | RunicDocs: commands are applied in the first realtime phase; a bound belongs with the network's limits | A bounded queue now |
| D5 | Runic::Simulation links World PUBLIC, and the runtime registers World's components on its entities | Every user of the simulation's entities places them with `WorldTransform` | Each module registers World's components itself |
| D6 | The runtime owns one `EntityWorld`, one `EntityStates`, one `EventHub`, and the buffers `engine:state_entered` and `engine:state_exited` | Slice 4's D5 left state facts for SIM-2 to put into events | A buffer per state |
| D7 | **Shape instances and `RenderInstanceId` are added to Core in slice 5**, in their own commit | GDB-3's Done when needs instances in the frame, and the plan's first instance kind is CORE-4's (slice 22). Without one, slice 5 cannot show a presenter at all | Move `RenderInstanceId` and a first instance kind into CORE-3 or a new package in RunicDocs (the plan's call) |
| D8 | A shape's change is measured in world terms; lists carry a revision, a base revision and the changed indices | The frame's origin moves every frame with the camera, so frame-relative numbers always change | The presenter compares every instance every frame |
| D9 | A frame's origin is the camera's eye | RunicDocs: "camera-relative" | A snapped origin (to a grid of a few metres) |
| D10 | The draw distance is the lens's far plane (4 km by default) | One number until extraction has frustum culling and caps | A separate setting |
| D11 | An entity's `RenderInstanceId` is a component (`RenderIdentity`) its maker fills | The client world has no `GlobalEntityId` until replication | Derive it from the local entity (not allowed to leave the process in RunicDocs' sense) |
| D12 | The camera's up is +Y, and -Z when it looks straight down | A view along +Y has no up | Keep the last up |
| D13 | The Presentation layer empties the frame's debug geometry; the shape list has exactly one writer, which empties it | Several extractors add debug geometry; the shape writer's change detection needs one writer | A layer-level reset of every kind |
| D14 | A rebase happens strictly past 4 km and moves every instance in one frame | RunicDocs' sketch | Spread a rebase over frames, or rebase in a frame where little else happens: 39 ms at 50,000 instances (Debug) |
| D15 | On a match by ID, a place within 1 mm of where the presenter put the shape is not a move | Two frame origins round the same place differently | Always set the transform on a match by ID |
| D16 | A capsule gets a mesh per size, to the millimetre; other kinds are unit meshes scaled; materials are lit | Scaling a capsule turns its ends into ellipsoids; lit shapes read as 3D | Unshaded colors; capsules scaled |
| D17 | The presenter converts places up to 14 km from its origin to floats itself | A shape at the edge of a frame (10 km from the camera) with the camera 4 km from the origin; `ToLocal` stops at 10 km | Shorten the frame's reach to 6 km |
| D18 | RunicClient keeps slice 2's scene as an extractor beside the walkers | The slice 2 CTest keeps checking the debug view | Replace it |

## Notes for other packages and for the plan

- **The plan:** D7. RunicDocs' Modules note gives GDB-3 (slice 5) instances by `RenderInstanceId` and CORE-4
  (slice 22) the first instance kind. Either a piece of CORE-4 moves into slice 5, or CORE-4 starts from these shape
  instances and adds meshes beside them.
- **Command queue lifetime:** a queue must be destroyed before its runtime (asserted). In a program, a layer above the
  Simulation layer owns its queues and goes first (layers detach top-down). A module that owns queues without a
  layer will need an owner that the runtime outlives; nothing in the plan says who that is yet.
- **No interpolation yet:** walkers move at 30 Hz and frames draw at 60 Hz or more, so in a window they step every
  other frame. Interpolation is CLI-4 (slice 22).
- **Formatting:** the GDB-3 commit first reformatted four slice 0 files by mistake; the next commit restored them.

## What is finished and what is set up for later

- **Finished:** everything the three packages' "Done when" asks, and the runnable result.
- **Set up for slice 6:** the editor shell can show the Presentation layer's extractors (`Describe`), the command
  queues (`CommandQueueCount`, `DescribeCommandQueue`) and the presenter's report.
- **Set up for slice 7:** PHYS-2 and SIM-8 register their systems in `PhysicsStructure`, `PhysicsStep` and
  `PostPhysics`; CLI-2's command builder pushes into a `CommandQueue`.
- **Set up for slice 8:** GDB-4's physics layer joins the debug view, which already moves with the presenter's shift.
- **Set up for slice 10:** SIM-3 runs a phase's systems on the pool between the same barriers.
- **Not built, by the rule "Stage A only":** parallel or change-driven extraction, frustum culling and caps, a
  bounded command queue, interpolation, mesh instances, MultiMesh, a rebase spread over frames.

## Noticed while writing it

- A test of the 20 km walk with round coordinates measured an error of exactly 0: every number in it was
  representable. The final test uses fractional coordinates and measures 0.06 mm.
- The frame writer drops shapes more than 10 km from the camera (a 32-bit position's reach). The walk's markers
  cover 20 km, so a frame never holds all of them; the test checks that the presenter holds exactly those in reach.
