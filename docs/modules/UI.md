# UI

> **Contract seed.** When `Engine/UI/` is created, copy this file to
> `Engine/UI/ARCHITECTURE.md`; from then on that file is authoritative.
> Purpose, Owns, Does not own, Allowed dependencies, and Primary consumers come
> from Appendix J §J.4.15. Other sections are derived and are proposals until
> confirmed.

| Field | Value |
| --- | --- |
| Directory | `Engine/UI/` |
| CMake target | `RunicUI` → `Runic::UI` |
| Namespace | `Runic::UI` |
| Roadmap phase | 2 — UI shell |
| Headless | **no** |
| Status | Not started — RmlUi not yet added to dependencies |

## Purpose

Player-facing UI framework integration: RmlUi adapter and the view/model/input
bridge.

## Owns

`UISystem`, `UIView`, `UIModel`, UI input/focus plumbing, the RmlUi backend.

## Does not own

Concrete HUD/inventory/auction/quest screens (RunicGame `GameUI`); authoritative
gameplay state mutation.

## Allowed dependencies

- Core, Application, Assets.
- RmlUi — private/backend-facing.

## Forbidden dependencies

Simulation mutation, gameplay modules, Dear ImGui for player-facing UI
([OQ-6](../system/11-open-questions.md#oq-6)), RunicPlatform, RunicGame.

## Primary consumers

RunicGame UI, Client.

## Public API (conceptual)

```text
UI/
├── Public/
│   └── UI.hpp               # split into explicit headers as the API grows
├── Source/
├── Tests/
└── CMakeLists.txt
```

## Threading / tick model (derived)

Client main thread, per frame. Receives `AppEvent`s for focus/input.

## Data ownership (derived)

`UIModel`s hold presentation copies of state. UI never owns or mutates
authoritative state; user actions become *intents/commands* sent through the
client runtime.

## Integration points (derived)

- Application: input and focus events (UI may consume them).
- Client: view models fed from replicated/presentation state; UI actions become
  commands.
- Game `GameUI` builds concrete screens on this framework.

## Acceptance tests (Stage A, derived)

- [ ] Load and display a document/view from Assets.
- [ ] Bind a model value and update it; route a button action to a callback that
      produces a command (not a state mutation).
- [ ] Input focus: UI consumes events it handles; others propagate.
- [ ] No RmlUi type in `Public/`; not linked by the headless server.

## Open questions

- [OQ-6](../system/11-open-questions.md#oq-6) — ImGui is developer UI only.
