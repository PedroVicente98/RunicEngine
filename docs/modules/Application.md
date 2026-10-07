# Application

> **Contract seed.** When `Engine/Application/` is created, copy this file to
> `Engine/Application/ARCHITECTURE.md`; from then on that file is authoritative.
> Purpose, Owns, Does not own, Allowed dependencies, Primary consumers, layout,
> and API sketch come from Appendix J §J.4.2. Other sections are derived and are
> proposals until confirmed.

| Field | Value |
| --- | --- |
| Directory | `Engine/Application/` |
| CMake target | `RunicApplication` → `Runic::Application` |
| Namespace | `Runic::Application` |
| Roadmap phase | 0 — bootstrap |
| Headless | yes (OS/window adapters are client/editor-only) |
| Status | Not started |

## Purpose

Graphics-independent process shell and coarse runtime composition.

## Owns

- `ApplicationHost`, `Layer`, `LayerStack`.
- Process lifecycle.
- Deferred layer mutations.
- Process-level application events (`AppEvent`).

## Does not own

Authoritative simulation scheduling, combat, persistence, renderer internals,
Platform transport.

## Allowed dependencies

- Core.
- Optional OS/window adapters are downstream or private
  ([OQ-4](../system/11-open-questions.md#oq-4)).

## Forbidden dependencies

Simulation, Renderer, UI, any gameplay module, any backend in `Public/`,
RunicPlatform, RunicGame. A window/GLFW dependency must never reach the headless
build.

## Primary consumers

Client, Editor, headless server bootstrap.

## Public API (conceptual)

```text
Application/
├── Public/
│   ├── Application.hpp
│   ├── ApplicationHost.hpp
│   ├── Layer.hpp
│   ├── LayerStack.hpp
│   └── AppEvent.hpp
├── Source/
├── Tests/
└── CMakeLists.txt
```

```cpp
class ApplicationHost {
public:
    void RequestStop();
    void Dispatch(AppEvent& event);
    LayerStack& Layers();
    RuntimeContext& Context();
};
```

**Rule.** Application layers are process composition only. They must not be used
to order movement, combat, physics, AI, replication, or quest consumers. [J.4.2]

## Threading / tick model (derived)

- Runs on the process main thread.
- `AppEvent`s are dispatched immediately through the LayerStack and may be
  consumed ("handled") — unlike gameplay facts. [J.7.1]
- Layer push/pop requested during dispatch is deferred to a safe point.
- The authoritative simulation keeps its own fixed-step clock; the host does not
  drive it per frame.

## Data ownership (derived)

The host owns layer lifetimes and the `RuntimeContext`. It owns no gameplay
state.

## Integration points (derived)

- OS/window adapter (client/editor only) → produces `AppEvent`s.
- Client and Editor are composed as layers; the headless server uses the host
  without any window.

## Acceptance tests (Stage A, derived)

- [ ] A headless host starts, runs, and stops via `RequestStop()` with no window
      or graphics library linked. [J.14.6]
- [ ] LayerStack dispatch order is deterministic; a consumed event stops
      propagation.
- [ ] Pushing/popping layers during dispatch is deferred and applied safely.

## Future extensions

OS/window adapter for client/editor builds. Nothing that schedules gameplay.

## Open questions

- [OQ-4](../system/11-open-questions.md#oq-4) — window and input placement.
- What `RuntimeContext` contains is not specified in the source.
