# Editor

> **Contract seed.** When `Engine/Editor/` is created, copy this file to
> `Engine/Editor/ARCHITECTURE.md`; from then on that file is authoritative.
> Purpose, Owns, Does not own, Allowed dependencies, and Primary consumers come
> from Appendix J §J.4.25. Other sections are derived and are proposals until
> confirmed.

| Field | Value |
| --- | --- |
| Directory | `Engine/Editor/` (+ editor executable) |
| CMake target | `RunicEditor` → `Runic::Editor` |
| Namespace | `Runic::Editor` |
| Roadmap phase | 0 (executable shell) — grows with each module; AI Editor subsystem in 11 |
| Headless | **no** — leaf; never a dependency of anything |
| Status | Not started |

## Purpose

Leaf developer-tooling application/module that composes inspectors and authoring
tools without becoming a runtime dependency.

## Owns

- Editor application, context, panels.
- ECS, Physics, World, AI, and Network inspectors.
- Profiling/debug views.
- Editor-only authoring integrations.

## Does not own

Authoritative gameplay logic; headless server dependencies; AI runtime ownership.

## Allowed dependencies

- Application, Renderer, ECS, World, Physics, AI, Water, Content, Assets,
  Simulation, diagnostics.
- Dear ImGui (proposal, [OQ-6](../system/11-open-questions.md#oq-6)).

## Forbidden dependencies

Being linked by any runtime target. RunicGame (Game-specific editor tooling
belongs in RunicGame on top of this). Platform production infrastructure.

## Primary consumers

RunicEngine developers and RunicGame authoring workflows.

## Public API (conceptual)

```text
Editor/
├── Public/
│   └── Editor.hpp           # split into explicit headers as the API grows
├── Source/
├── Tests/
└── CMakeLists.txt
```

## Threading / tick model (derived)

Client-style frame loop; may host a local simulation instance for previews and
inspection, observing it at safe points.

## Integration points (derived)

- Inspectors read ECS/World/Physics/AI state; edits go through the same commands
  and APIs as runtime code.
- `PhysicsDebug` and `DebugRenderer` for visualization.
- Content registry for authoring/validation.

## Acceptance tests (Stage A, derived)

- [ ] Phase 0: the editor executable builds and opens an empty shell.
- [ ] Headless dependency test: no headless target links `RunicEditor`, ImGui, or
      editor sources. [J.14.6]

## Future extensions

AI Editor subsystem (phase 11), quest tooling (after phase 10), world authoring.
