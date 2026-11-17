# Slice 7 · Physics

**Runnable result:** a capsule walks on static terrain far from the origin under keyboard control.
**Packages:** PHYS-1 and PHYS-2 (Runic::Physics), SIM-8 (Runic::Simulation), CLI-2 (Runic::Client), and the RunicClient
scene that shows them. Two fixes to earlier packages ride along, each its own commit: PHYS-1 (a contact lasts while its
bodies sleep, found by SIM-8's test) and GDT-2 (the editor tests waited a fixed second for Godot's help thread and
crashed under load).
**Status:** done on the preview branch; 26 tests in `RunicPhysicsTests`, 8 new in `RunicSimulationTests`, 9 new in
`RunicClientTests`, a new RunicClient CTest and two changed ones. The Engine's debug build runs 436 tests and the
headless build 355, all passing.

```sh
cmake --preset debug && cmake --build --preset debug && ctest --preset debug
./build/debug/bin/RunicClient                      # the walk: W A S D, Shift+W, Space, R, a click
./build/debug/bin/RunicClient --moving-scene       # slice 5's walkers, as before
ctest --preset debug -R runic_client_walk           # R, Space and R pressed through Godot
```

The walk check prints, among Godot's own lines:

```text
RUNIC: AppEvent Key reached the stack (key 82)
RUNIC: AppEvent Key reached the stack (key 32)
RUNIC: AppEvent Key reached the stack (key 82)
RUNIC: presenter stopped at frame 600: 7 instances, 0 rebases
RUNIC: debug view stopped at frame 600: 8192 line vertices, 6144 triangle vertices, 1 texts in 2 surfaces
RUNIC: walk scene: the capsule walked 12.4 m on terrain 8602 km from the origin, jumps 1, marks 0, highest 1.19 m, ends on the ground
RUNIC: RunicClient exited cleanly after 600 frames
```

In a window: a 64 m terrain of shaded triangles with its wireframe, flat in the middle and rolling hills around, at
(5,000 km, 120 m, −7,000 km). On the flat part, a grey 0.3 m step the capsule climbs, a 0.6 m ledge it does not, a
wall, and three blue crates it pushes. The green capsule walks where the keys say, relative to the camera behind it;
the controls float above it. Slice 5's scene is still there with `--moving-scene`.

## What was built

| Package | What | Where |
| --- | --- | --- |
| PHYS-1 | Jolt 5.6.0 in double precision behind Runic types: the PhysicsWorld, named shared shapes (box, sphere, capsule, mesh), bodies created and removed in batches, kinematic targets, collision layers in two broad-phase groups, contacts as facts with the owners of their bodies | `Engine/SimulationCore/Physics`: `Public/` (four headers), `Source/PhysicsWorld`, `Source/CollisionLayers`, `Tests/`; `cmake/Dependencies.cmake` (the Jolt pin) |
| PHYS-2 | Characters on Jolt's `CharacterVirtual`: upright capsules with their feet at their position, walking, steps, walls, slopes, falling, jumping, riding what they stand on; characters meet each other only when their layers collide, found through a grid of 4 m columns, and block rather than push each other | `Physics/Source/Characters`, the character API in `PhysicsWorld.hpp` |
| SIM-8 | The physics bridge: `SimulationRuntime::EnablePhysics`, the `PhysicsBody` and `PhysicsCharacter` components, requests applied in PhysicsStructure, the sync on each side of the step, contacts as `PhysicsContact` facts, bodies that follow destroyed entities at every barrier | `Simulation/Public/PhysicsBridge.hpp`, `Source/PhysicsBridge`, the barrier hook in `Source/Phases` |
| CLI-2 | The Input layer and its `InputMap`: actions, presets, bindings with modifiers and chords where the more specific binding wins, presses and releases per frame, focus loss; command builders; movement relative to the camera | `Client/Public/InputMapping.hpp`, `InputLayer.hpp`, `Source/InputMapping` |
| (program) | The walk scene: the terrain (a Jolt mesh), the blocks, the crates, the capsule, the keys, the command builder, the Commands-phase system, the terrain extractor; `--moving-scene`; the walk CTest | `Engine/Programs/RunicClient`: `WalkScene.cpp`, `Main.cpp`, `CMakeLists.txt` |

## The order to read it in

1. **`Physics/Public/PhysicsWorld.hpp`**: the whole physics API in one page, bodies then characters.
   `Source/PhysicsWorld/WorldState.hpp` is what stands behind it (the Jolt system, the contact buffer, the characters);
   `PhysicsWorld.cpp`'s `Step` is six lines: characters, then Jolt, then the contacts settle.
2. **`Source/Characters/Characters.cpp`**: `StartVelocity` (what a move means) and `CharacterSet::Update` (Jolt's
   extended update per character). `CharacterSet.hpp` has the handles and the column grid.
3. **`Simulation/Public/PhysicsBridge.hpp`**: the three rules in its opening comment. Then
   `Source/PhysicsBridge/PhysicsBridge.cpp`: `ApplyRequests`, `SyncBeforeStep`, `WriteBack`, `FollowDestroyed`.
4. **`Client/Public/InputMapping.hpp`**: the binding rule in its opening comment; `Source/InputMapping/InputMap.cpp`'s
   `Remap` (about 30 lines) is all of it.
5. **`Programs/RunicClient/WalkScene.cpp`**: how a program puts the four together.

## One key press, traced

The player holds W; the camera looks along −Z.

1. Godot sees the key. No control has focus, so it reaches the GodotLayer's input sink through `_unhandled_input`,
   which posts `KeyEvent{87, pressed}`. A key a focused text field takes never gets this far.
2. At the next frame's start the LayerStack dispatches it top-down. The Input layer's `OnEvent` feeds it to the map:
   `W` is now down, `Remap` lists the bindings that hold (W → `engine:demo_forward`), none covers it, the action is
   pressed.
3. In the frame's update the Input layer runs the walk scene's builder: `MovementDirection` gives (0, 1),
   `WorldMovement` turns it to (0, 0, −1), and a `WalkCommand` of 4 m/s along −Z is pushed into
   `engine:demo_walk`. The map's frame ends.
4. The Simulation layer's next realtime tick: in the Commands phase the scene's system drains the queue, keeps the
   latest velocity and writes it into the capsule's `PhysicsCharacter::move`.
5. PhysicsStep: the bridge hands the move to Physics (and clears it) and steps once. Physics updates the character
   before Jolt's bodies: on walkable ground its velocity becomes the ground's plus the walk; Jolt's extended update
   sweeps the capsule, climbs anything up to 0.4 m, keeps it on the floor and stops it at walls.
6. PostPhysics: the bridge writes the character's feet into the entity's `WorldTransform`, 8,600 km out in 64 bits.
   Contacts of the step (a crate it pushed into the step) are emitted as facts, readable from Queries on.
7. The Presentation layer follows the capsule with the camera and its extractors write the frame relative to the
   camera; the presenter moves the capsule's instance in Godot.

## What the tests prove

| Suite | Tests | What it proves (the package's "Done when") |
| --- | --- | --- |
| `PhysicsWorld.*`, `CollisionLayers.*`, `PhysicsShapes.*` (PHYS-1) | 12 | **Bodies collide without jitter far from the origin**: a crate dropped 8,600 km out rests at the same height as at the origin, still to a tenth of a millimetre; batches of 1,000; kinematic targets; one contact fact per pair with owners; a contact lasts while its bodies sleep; Jolt makes no thread |
| `Characters.*` (PHYS-2) | 14 | **A capsule walks, climbs steps and stops at walls**: walks 2 m in a second; climbs 0.3 m; stops at 0.6 m and at a wall, sliding along it at 45°; up 20° not 65°; falls off an edge and lands; jumps only from the ground; rides a platform; the same course at the origin and 8,600 km out within 1 mm at every step; layers, the grid, teleports, no pushing; handles; bad descriptions |
| `PhysicsBridge.*` (SIM-8) | 8 | **The capsule's transform matches its CharacterVirtual after every step** (exactly, far out); **destroying an entity removes its body at the next barrier** (in a phase, and between steps); **a contact reaches the next phase as a fact**, read once with its entities; bodies arrive in PhysicsStructure; kinematic in, dynamic out; requests |
| `InputMapping.*` (CLI-2) | 9 | **Key presses become movement commands; a modifier plus a key maps to its own action; input a layer above handled never reaches the actions**; chords, taps, repeats, mouse buttons, focus, presets, movement relative to the camera |
| `godot.runic_client_walk` | 1 | **The capsule walks on terrain 8,600 km out under keyboard control**: R, Space and R pressed through Godot; it walked, jumped once and ends on the ground |
| `godot.runic_client_input` (changed) | 1 | **Input Godot's UI consumed never reaches the actions**: the click on the test button is not the mark action; the click beside it is |

Physics and Simulation pass under AddressSanitizer with UBSan, ThreadSanitizer, clang and a Release build; Client
under ASan, clang and Release with presentation on; RunicClient's sources compile cleanly with GCC and clang under
`-Wall -Wextra -Wpedantic -Werror`.

## What it costs

Release, far from the origin, one thread, on the 2-core build machine.

| Operation | Cost |
| --- | --- |
| Creating a body | 0.75 µs (1,000 in 0.75 ms, in one batch) |
| A step of resting boxes / sliding boxes | 100: 87 / 320 µs; 1,000: 1.2 / 4.1 ms; 5,000: 5.1 / 17.9 ms |
| A walking character per step | 0.9 µs (1,000: 0.9 ms; 5,000: 4.8 ms) |
| The same with character collision on | 1.2 µs (5,000: 5.8 ms); 9 µs each in a crowd packed 0.7 m apart (1,000: 9.1 ms). Without the column grid, 5,000 cost 1.6 s per step |
| The bridge's write-back | about 29 ns per entity with a body or a character |
| The bridge's look for destroyed entities | 0.2 µs per barrier when nothing went; destroying 100 of 2,000 tracked: 85 µs at the next barrier |
| 1,000 crates and 1,000 walking characters pushing them | 5.1 ms per realtime tick, nearly all Jolt |
| A key event through the map (60 actions, 60 bindings) | 215 ns; the movement helpers 6 ns |

## Decisions that await the owner

| # | Decision taken | Why | The alternative |
| --- | --- | --- | --- |
| D1 | Jolt 5.6.0 with RTTI on, asserts and floating-point exceptions off; single-threaded until PHYS-4 | UBSan's checks need Jolt's typeinfo; no thread outside the Core pool | RTTI off outside sanitizer builds |
| D2 | Two collision steps per 30 Hz step | Jolt is most stable at 60 Hz collision steps | One |
| D3 | One contact fact per body pair (began, ended), none per sub-shape and no "persisted" fact; a pair stays touching while its bodies sleep | Facts, not a stream; a crate asleep on a plate still presses it | Jolt's raw callbacks |
| D4 | Four Engine layers (static, moving, character, sensor) in two broad-phase groups; characters do not meet characters by default | The Engine's minimum; MMOs rarely body-block players | A character layer that meets itself |
| D5 | A character is a capsule given by radius and height, shapes shared per size; no inner body, so bodies and sensors do not see characters and character contacts are not facts yet | Stage A: walking needs no more | A registry shape; an inner body now |
| D6 | A move lasts one step; on the ground the character takes the ground's velocity plus the move; in the air it keeps its horizontal velocity (no air control); a jump is a speed the caller gives, taken only from settled walkable ground | Jolt's sample without inertia; movement values are GPL-1 and GPL-2's | Air control, inertia |
| D7 | Characters update before the bodies, every step, moved or not | A platform's velocity carries its riders before it moves (RunicDocs) | After |
| D8 | Characters block each other and never push | One character must not shove another off a ledge | Jolt's default (a walker pushes a stander) |
| D9 | Characters near each other are found through Physics' own 4 m column grid | Linear cost; World's spatial index (WORLD-2) is for gameplay volumes | WORLD-2's index |
| D10 | Physics is opt-in per runtime and the runtime owns the bridge; only the bridge adds and removes its components | A world without physics pays nothing; one owner of the handles | Always on |
| D11 | A destroyed entity is noticed by comparing the counts of the bridge's components with what it keeps, at every barrier | Runic::ECS has no removal hook; the check is 0.2 µs | An ECS removal hook (an interface change) |
| D12 | PostPhysics writes a character's position only; its rotation is the entity's own. Static bodies never follow their entity | An upright capsule turns nothing; statics never move | Write the rotation too |
| D13 | The more specific binding wins (not an exact modifier match); a chord holds in any order; one flat preset is active | W walks with Shift held unless Shift+W is bound | Exact matches; stacked contexts |
| D14 | "Layouts" read as keyboard layouts: nothing to do, since Godot passes the logical key | A binding to W follows the letter | Physical keys (GodotBridge would pass the physical code too) |
| D15 | The Input layer handles no event; focus loss releases every action | Layers below may still want input; a release may go to the UI | Mapped events handled |
| D16 | RunicClient's default scene is slice 7's walk; slice 5's runs with `--moving-scene`; R toggles walking on its own | The client shows the newest slice; a scripted key is a one-frame tap, so the check needs a toggle | A held scripted key |

## Notes for other packages and for the plan

- **PHYS-1 had a bug its tests did not catch:** Jolt drops a sleeping body's contacts, so a resting crate "ended" its
  contact after two seconds. SIM-8's contact test found it; PHYS-1's fix and its test are their own commit.
- **The editor tests' Godot crash** (slice 6 noted "the tests let the editor run one more second") is a Godot 4.7.2
  bug: the help cache loads on a thread that ends by queuing the extensions' docs, and `Main::cleanup` flushes that
  call after the docs are deleted. Under load the second was not enough. The harness now opens a help page first
  (which waits for the thread). The RunicEditor itself can still crash on a quit in its first seconds on a loaded
  machine, until Godot checks for the deleted docs.
- **GodotBridge's scripted keys are one-frame taps.** A held scripted key (press and release frames) would let checks
  hold W; an interface change to GDB-1's spec, not made here.
- **Runic::ECS has no removal hook** and **Physics cannot list the bodies Jolt moved.** Both would make the bridge
  cheaper at scale (no scan, write-back only for active bodies); neither is needed at today's numbers.
- **Characters are invisible to sensors and bodies.** When a slice needs a moving trigger that detects players (a
  boss aura, a ship's deck), PHYS gets Jolt's inner body or character contacts as facts.
- PHYS-1's catalog row allows World; Physics does not link it, nothing needed it.
