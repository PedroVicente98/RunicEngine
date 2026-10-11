# Editor

Contract seed for `Runic::Editor`: what a Claude Code session needs to implement the Editor work packages. When `Engine/Tools/Editor/` is created, this file becomes its `ARCHITECTURE.md`, and that file is authoritative from then on.

| Field | Value |
| --- | --- |
| Group | Tools |
| Directory | `Engine/Tools/Editor/` |
| CMake target | `Runic::Editor` |
| Namespace | `Runic::Editor` |
| Headless | never shipped |
| Status | Not started |

## Purpose

The Editor is a development-only executable on the Engine's ApplicationHost: the editor host and panels, the world, ECS and physics inspectors, the resolver debugger, the AI editor, live attach to development and staging servers, and the Cooker launcher (ARCH § Tools; ARCH § AI architecture, AI editor). It is never distributed to players. The Engine defines its `CreateApplication()`; its stack is the editor host with tool panels, under an ImGui overlay, in a frame loop.

## Owns

- The editor host, the panel framework, and the world and ECS inspectors (EDT-1).
- The physics inspector (EDT-2).
- The resolver debugger: which rules applied to a hit and why it landed or missed (EDT-3).
- The Cooker launcher: background cooking with progress over a local protocol, so a long cook never freezes or crashes the Editor (EDT-4).
- The AI editor (EDT-5, EDT-6) and live attach (EDT-7).

The AI editor is a native Runic ImGui editor on a node-canvas library (such as imnodes); Groot2 is not embedded. BehaviorTree.CPP remains the executor; the Editor owns the authoring experience.

| Panel | Purpose |
| --- | --- |
| AI Assets | Create, duplicate, rename, organize, open and delete trees; browse NPC definitions and personality profiles |
| Behavior Tree Canvas | Create, select and connect nodes; arrange, zoom and pan; validate structure; edit subtrees |
| Inspector | Node ports and properties plus Runic metadata and documentation |
| NPC Definition Inspector | Assign tree, personality, perception profile, leash defaults and game AI profile fields |
| Runtime Debug | The selected live NPC: active node path, working memory, target, reaction timer, utility scores, last events, last intent |
| Validation and Problems | Missing nodes, invalid ports, unknown ability or content IDs, unreachable branches, forbidden dependencies |

Interaction rules:

- Graph editing is editor state, not simulation state.
- Runtime highlights are read-only overlays unless a specific debug command is implemented.
- Live tree replacement happens only at an explicit safe point; a tree is never destroyed or replaced while it ticks.
- The node list comes from the runtime's registration plus editor metadata, never a second hard-coded list.

Live attach: in development and staging, the Editor attaches to a running GameServer through its Diagnostics layer and shows the entity inspector, live behavior trees, resolver traces and the server's debug shapes, drawn locally with the debug renderer.

| Debug view | Minimum information |
| --- | --- |
| Brain summary | Entity, AIDefinition, tree asset, BrainHandle, active or sleeping, current target |
| Tree execution | Running, succeeded and failed nodes for the current or previous tick, with the subtree path |
| Working memory | Incoming threat, reactAt, target, last combat time, alert position, leash and home state, game-exposed values |
| Utility trace | Candidates, base scores, major modifiers, decision noise, selected result |
| Event trace | Recent facts this NPC's perception consumed |
| Intent trace | Recent intents and whether the resulting command was accepted or rejected |
| Spatial debug | Warn, detect and leash radii, navigation goal, target marker |

## Does not own

- The AI runtime (AI); the Editor's AI panels use public AI definition and debug interfaces only.
- The attach endpoint and debug-shape stream (Network, NET-7).
- Cooking (Cooker): the Editor only launches it as a separate process.
- Authoritative simulation ownership; no direct mutation of live combat state.
- Game content: trees, personalities and NPC definitions are RunicGame assets the Editor edits.

## Depends on

| Kind | Allowed |
| --- | --- |
| Engine modules | Most modules (ARCH's catalog row); the contract session lists the exact set each package links |
| Third-party (PRIVATE) | Dear ImGui, imnodes (node canvas) |

### Must never depend on

- Being depended on: no Engine module, and no game runtime target, links the Editor.
- A shipped executable or a headless server build.
- Platform or Game code. The Editor attaches over the Engine's Diagnostics channel, never through Platform services.

## Public API sketch

ARCH's illustrative node registration, one call feeding both execution and editor discovery:

```cpp
struct AINodeMetadata {
    std::string_view name;
    std::string_view category;
    std::string_view description;
    bool engineNode = true;
};

template <class T>
void RegisterAINode(BT::BehaviorTreeFactory& factory, AIEditorRegistry& editorRegistry,
                    AINodeMetadata metadata) {
    factory.registerNodeType<T>(std::string(metadata.name));
    editorRegistry.Register<T>(metadata);   // category, docs, ownership, icons, allowed contexts
}
```

`BT::BehaviorTreeFactory` appears here under the approved BehaviorTree.CPP node-type exception. The Editor's own panels live in `Source/` (ARCH lists AI panels such as the tree editor, node palette, tree inspector, personality editor, runtime debugger and validation).

Headers (sketch, fixed in the contract session): `EditorHost.hpp`, `EditorPanel.hpp`, `AIEditorRegistry.hpp`.

## Threading and tick rules

- The Editor runs a frame loop: events, OnUpdate, OnRender, OnImGuiRender.
- A local world opened in the Editor runs the normal SimulationRuntime; inspectors read it and change it only through explicit debug commands at safe points.
- The Cooker runs as a separate background process; the Editor reads its progress over a local protocol.
- Trees are saved independently of the live simulation; hot reload is a development-only safe-point operation.

## Work packages

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| EDT-1 | Editor host, panel framework, world and ECS inspectors | The Editor opens a local world and inspects entities | APP-2, UI-1, SIM-2 | 6 |
| EDT-2 | Physics inspector | Bodies are selectable with their properties | EDT-1, PHYS-3 | 8 |
| EDT-3 | Resolver debugger | It shows which rules applied to a hit and why | EDT-1, CMB-4 | 17 |
| EDT-4 | Cooker launcher: background cooking with progress over a local protocol | A long cook never freezes the Editor | EDT-1, COOK-2 | 20 |
| EDT-5 | AI editor: behavior-tree canvas, palette, inspector, asset browser, XML save and load | A tree is edited, saved and reloaded | EDT-1, AI-2 | 35 |
| EDT-6 | AI tools: NPC definition and personality editors, content-ID validation, live NPC debugger | A running NPC's tree state is visible live | EDT-5, AI-5 | 35 |
| EDT-7 | Live attach to development and staging servers | Entity inspector and debug shapes come from a remote server | EDT-1, NET-7 | 39 |

## Acceptance tests

The Done when column is the acceptance list. Editor validation: unknown nodes, ports or content IDs are reported, and valid assets round-trip without loss. Selecting a live NPC shows the same BrainHandle, tree, memory and intents the runtime instrumentation records. Tests use Engine test assets.

## Open items

- ARCH's catalog row says "Most modules". Each Editor package's contract session lists the exact modules it links and asks the owner before adding one.
- ARCH says the attach channel is compiled only into development builds, yet the Editor also attaches to staging servers. The EDT-7 contract session must confirm that staging servers run development builds.
- The transport the attach channel uses in the Editor process is open (see [Network.md](Network.md)).
