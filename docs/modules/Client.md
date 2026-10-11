# Client

Contract seed for `Runic::Client`: what a Claude Code session needs to implement the Client work packages. When `Engine/Presentation/Client/` is created, this file becomes its `ARCHITECTURE.md`, and that file is authoritative from then on.

| Field | Value |
| --- | --- |
| Group | Presentation |
| Directory | `Engine/Presentation/Client/` |
| CMake target | `Runic::Client` |
| Namespace | `Runic::Client` |
| Headless | no |
| Status | Not started |

## Purpose

Client is the Engine's client runtime: the Prediction and Presentation layers, the render-extractor registry and the Engine's extractors, the camera, interpolation, input mapping and the command builder (ARCH § Client runtime; ARCH § Render data, extraction and world UI). A game's client executable composes it with its own presentation; the client predicts with the shared game code, presents an interpolated world and runs the UI the server never has.

## Owns

- The Presentation layer, the extractor registry, the first extractor (entities as debug shapes) and the camera (CLI-1).
- Input mapping, actions and the command builder: raw application events become commands (CLI-2; decided in OQ-4).
- The Prediction layer, which drives the prediction subset and reconciliation each frame (CLI-3).
- Engine extractors: mesh instances with interpolation and camera-relative conversion (CLI-4), skinned instances with poses from Animation (CLI-5), world anchors (CLI-6), lights, environment and volumes (CLI-7), water surfaces and field grids (CLI-8).

Client layer stack, bottom to top: Network → Prediction → Presentation (interpolation, camera, render extraction, audio), with GameUI (RmlUi) and, in development builds only, the ImGui overlay above them. Input reaches the top first, so UI clicks never become attacks.

### Extraction rules

- Extraction runs in the Presentation layer once per frame, after the latest simulation results are applied. Extractors read Flecs and the domain APIs (Water, Perception, Environment, Animation) read-only and write RenderData; they never change the simulation.
- A registry of extractors: each fills one or more RenderData kinds. The Engine registers its own under `engine:` IDs; RunicGame registers its own under `game:` IDs.
- Parallel: independent extractors run on the worker pool into per-worker arenas merged into the frame packet in a fixed order.
- Change-driven when the source is slow: field grids, water state and the environment re-extract only when their source version changes.
- Culled and capped at the source: each extractor culls by frustum and distance and respects a per-kind cap (lights, anchors).
- Interpolation and precision here: remote entities are drawn slightly in the past between snapshots, with a short buffer kept small for nearby enemies; 64-bit positions are converted to camera-relative 32-bit floats once, during extraction.
- Anchors carry the PvP visibility tier, so a Hidden player produces no anchor and a Silhouette produces a reduced one.

## Does not own

- Drawing: both render pipelines (Renderer). Client fills the RenderFrame; the Renderer reads only that frame.
- The world UI mechanism (UI), animation evaluation (Animation), sound and music playback (Audio).
- The prediction subset itself (Simulation, SIM-4) and the code it runs (RunicGame::Shared); reconciliation math (Network, NET-3).
- The Window and raw input events (Application).
- RenderData types (Core).
- Game extractors, camera rules, GameUI screens, input bindings and presentation content (RunicGame::Client).

## Depends on

| Kind | Allowed |
| --- | --- |
| Engine modules | Application, Simulation, Network, Perception, Environment, Water, Renderer, Animation, Audio, UI, Assets |
| Third-party | — |

### Must never depend on

- Editor or Cooker.
- RunicPlatform or RunicGame code. The game composes Client; Client never includes game headers.
- Mutation of the simulation: extractors are read-only.
- Client is not headless, so no headless module may link it, and the server never links it.

## Public API sketch

ARCH's illustrative extractor registration (Engine extractors only):

```cpp
// Illustrative
extraction.Register("engine:meshes",  MeshExtractor{});                 // Flecs Transform + RenderMesh
extraction.Register("engine:skinned", SkinnedExtractor{animation});      // poses from Runic::Animation
extraction.Register("engine:anchors", AnchorExtractor{perception});      // carries the PvP visibility tier
extraction.Register("engine:lights",  LightExtractor{perception});       // the server's emitters
extraction.Register("engine:fields",  FieldExtractor{water, environment}, ExtractRate::OnChange);
// RunicGame::Client registers its own extractors under game: IDs.
```

Headers (sketch, fixed in the contract session): `PresentationLayer.hpp`, `PredictionLayer.hpp`, `ExtractorRegistry.hpp`, `Camera.hpp`, `Interpolation.hpp`, `InputMapping.hpp`, `CommandBuilder.hpp`.

## Threading and tick rules

- The client runs a frame loop (vsync or capped). Extraction writes frame N+1 while the Renderer reads frame N; the two never lock each other.
- Extractors never write the simulation and never run during a simulation phase that mutates what they read.
- The client applies its own input immediately and keeps a short history of numbered inputs; the server returns authoritative state with the last input it processed, and the client replays the inputs after it. Small errors blend; large ones snap.
- Skill starts are predicted cosmetically and cancelled in the rare case the server refuses.

## Work packages

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| CLI-1 | Presentation layer and extractor registry; the first extractor writes entities as debug shapes; camera | Entities are drawn from the RenderFrame only | ECS-1, RND-2, CORE-3 | 5 |
| CLI-2 | Input mapping, actions and command builder | Key presses become movement commands | APP-2, SIM-2 | 7 |
| CLI-3 | Prediction layer driving the prediction subset and reconciliation each frame | The local player moves at once and corrects on server state | SIM-4, NET-3 | 14 |
| CLI-4 | Mesh-instance extractor with interpolation and camera-relative conversion | Cooked meshes appear at their entities' positions far from the origin | CLI-1, CORE-4 | 22 |
| CLI-5 | Skinned-instance extractor with poses from Animation | Characters animate in the debug pipeline | CLI-4, ANIM-1 | 23 |
| CLI-6 | World-anchor extractor: position, distance, visibility tier and priority per entity | Anchors follow entities; Hidden players produce none | CLI-1, CORE-4 | 23a |
| CLI-7 | Light, environment and volume extractors from Perception and Environment | Darkness and torches on screen match the server's light values | CLI-1, CORE-5, PRC-1 | 25a |
| CLI-8 | Water-surface and field-grid extractors from Water and Environment, change-driven | Grids re-extract only when the current or the wind changes | CLI-1, CORE-6, WATER-5 | 42a |

## Acceptance tests

The Done when column is the acceptance list. Runnable results by slice: 5, entities move on a plane through commands and are drawn; 7, a capsule walks under keyboard control; 14, responsive movement with correction under simulated lag; 22, cooked meshes draw flat-shaded far from the origin; 23, a skinned character animates; 23a, nameplates and bars follow entities; 25a, lights and darkness on screen match the server; 42a, water surfaces, currents and wind draw. Tests use Engine test content.

## Open items

- Extractors read Flecs, while ECS keeps Flecs private (see the open item in [ECS.md](ECS.md)). The CLI-1 contract session must ask how Client reads Flecs components.
- [OQ-4](../system/11-open-questions.md#oq-4) (decided): input mapping lives in Client; raw input is application events from the Window class.
- [OQ-15](../system/11-open-questions.md#oq-15) (decided): prediction runs RunicGame::Shared on the client.
