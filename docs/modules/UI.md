# UI

Contract seed for `Runic::UI`: what a Claude Code session needs to implement the UI work packages. When `Engine/Presentation/UI/` is created, this file becomes its `ARCHITECTURE.md`, and that file is authoritative from then on.

| Field | Value |
| --- | --- |
| Group | Presentation |
| Directory | `Engine/Presentation/UI/` |
| CMake target | `Runic::UI` |
| Namespace | `Runic::UI` |
| Headless | no |
| Status | Not started |

## Purpose

UI integrates RmlUi (the GameUI layer for players) and Dear ImGui (the development overlay) as layers, and owns the world-UI mechanism: diegetic panels, anchored elements, the batched fallback, budgets and input routing (ARCH § Render data, extraction and world UI). Most UI lives in the world. Every world widget is authored once in RmlUi by the game, and the Engine decides how it is drawn from distance, on-screen count and frame budget.

## Owns

- The ImGui integration as the development overlay layer, present only in development builds (UI-1).
- The RmlUi integration as the GameUI layer; input reaches the UI first (UI-2).
- Anchored world UI: RmlUi elements that follow world anchors, with distance scaling, culling and budgets (UI-3).
- Diegetic panels: RmlUi contexts rendered into world textures, with input by ray into panel coordinates (UI-4).
- Mode fallbacks between diegetic, anchored, batched and hidden by distance, count and battle budgets; the batched atlas is built through RmlUi (UI-5).

| Mode | What it is | Cost |
| --- | --- | --- |
| Diegetic panel | An RmlUi document rendered into a texture on a quad or a mesh surface in the world; input by ray into the panel | One offscreen render per visible panel, only when its content changes |
| Anchored element | An RmlUi element that follows an entity on screen, scaled and tilted with distance through RCSS transforms | Layout when content changes; a position update each frame |
| Batched | Labels and bars drawn by one instanced Renderer call, using an atlas that RmlUi renders from the same RCSS style | One draw call for all; the atlas changes only when a name or a style changes |
| Screen HUD | Ordinary RmlUi screens | Standard RmlUi |

Rules:

- A widget declares its preferred mode, its fallbacks with their distance thresholds, a budget per mode and the fields it binds. The values are the game's.
- RmlUi does not batch separate elements by itself, so the batched path is the Engine's own feature (Renderer, RND-10), fed by RmlUi.
- Visibility follows gameplay: anchors carry the PvP visibility tier, so a Hidden player shows no nameplate and a Silhouette shows a reduced one.
- Input reaches the UI first. A click on a diegetic panel becomes a ray, the ray hits the panel's quad, the hit becomes panel coordinates, and that panel's RmlUi context receives the event. Any action becomes a normal command the server validates.
- The UI never reads Flecs: positions come from world anchors in the RenderFrame; content comes from the game's view models.
- Budgets protect the frame: beyond a mode's cap, widgets fall back to the next mode, and battle mode lowers the caps.

## Does not own

- World anchors: the Client extracts them (CLI-6) into Core RenderData.
- Render targets, world quads and the instanced draw (Renderer, RND-9 and RND-10).
- Input mapping and the command builder (Client); raw input events (Application).
- Widget content: RML documents, RCSS styles, view models and screens (RunicGame::Client).

## Depends on

| Kind | Allowed |
| --- | --- |
| Engine modules | Application, Renderer, Assets |
| Third-party (PRIVATE) | RmlUi, Dear ImGui |

### Must never depend on

- Flecs, ECS, Simulation or any simulation state.
- Client (Client depends on UI).
- RmlUi or ImGui types in `Public/`.
- Any headless build: ImGui never reaches a server, and shipping client builds leave out the ImGui layer (OQ-6).
- RunicPlatform or RunicGame code.

## Public API sketch

Sketch, fixed in the contract session: `GameUILayer.hpp`, `DevOverlayLayer.hpp`, `WorldWidget.hpp`, `AnchoredElements.hpp`, `DiegeticPanel.hpp`, `WidgetBudgets.hpp`.

## Threading and tick rules

- UI layers run in the client frame loop as overlays above the client layers; events reach them first, top-down.
- UI reads the current RenderFrame and view models only; it never blocks the frame on layout of unchanged content.
- A diegetic panel re-renders offscreen only when its content changes; the batched atlas changes only when a name or a style changes.

## Work packages

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| UI-1 | ImGui integration as the development overlay layer | The overlay exists only in development builds | APP-1, RND-1 | 2 |
| UI-2 | RmlUi integration as the GameUI layer; input reaches the UI first | UI clicks never become attacks | APP-1, RND-2 | 19 |
| UI-3 | Anchored world UI: RmlUi elements following world anchors, with distance scaling, culling and budgets | Nameplates follow entities and respect visibility tiers | UI-2, CLI-6 | 23a |
| UI-4 | Diegetic panels: RmlUi contexts rendered into world textures; input by ray into panel coordinates | A click on a world board reaches its RmlUi context and becomes a command | UI-3, RND-9 | 23a |
| UI-5 | Mode fallbacks: widgets move between diegetic, anchored, batched and hidden by distance, count and battle budgets; the atlas is built through RmlUi | In a 500-player fight the frame budget holds and the target keeps its full nameplate | UI-3, RND-10 | 38 |

## Acceptance tests

The Done when column is the acceptance list. Slice 2 shows the ImGui overlay over the debug renderer; slice 23a shows nameplates and bars following entities and a world panel accepting clicks; slice 38 shows nameplates falling back to the batched path. Tests use Engine test documents and styles.

## Open items

- [OQ-6](../system/11-open-questions.md#oq-6) (decided): ImGui is private to UI (development overlay, client development builds) and Editor; never on servers.
