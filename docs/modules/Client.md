# Client

> **Contract seed.** When `Engine/Client/` is created, copy this file to
> `Engine/Client/ARCHITECTURE.md`; from then on that file is authoritative.
> Purpose, Owns, Does not own, Allowed dependencies, Primary consumers, and layout
> come from Appendix J §J.4.12. Other sections are derived and are proposals
> until confirmed.

| Field | Value |
| --- | --- |
| Directory | `Engine/Client/` |
| CMake target | `RunicClient` → `Runic::Client` |
| Namespace | `Runic::Client` |
| Roadmap phase | 2 (local) — prediction/reconciliation in 4 |
| Headless | **no** |
| Status | Not started |

## Purpose

Reusable, non-authoritative client runtime composition: local input mapping,
prediction facade, reconciliation, interpolation, and presentation bridge.

## Owns

- `ClientRuntime`.
- Logical client world/session facade.
- Input mapping.
- Prediction/reconciliation orchestration.
- Interpolation state.

## Does not own

Server authority, Game UI content, transport internals, Game rules.

## Allowed dependencies

- Application, Events, Simulation/client simulation surface, NetworkAdapter,
  Renderer, Assets; presentation modules (Animation, Audio, UI).

## Forbidden dependencies

Platform transport internals (only through NetworkAdapter), server-only modules
or data, Editor, RunicGame.

## Primary consumers

The RunicGame client executable.

## Public API (conceptual)

```text
Client/
├── Public/
│   ├── ClientRuntime.hpp
│   ├── ClientWorld.hpp
│   ├── ClientPrediction.hpp
│   ├── ClientInterpolation.hpp
│   └── InputMapping.hpp
├── Source/
│   ├── Input/
│   ├── Prediction/
│   ├── Interpolation/
│   └── Presentation/
├── Tests/
└── CMakeLists.txt
```

The client may speculate and present, but it never decides an authoritative hit,
damage, quest completion, trade validity, or durable ownership. [J.4.12]

## Threading / tick model (derived)

- Frame loop on the main thread (Application).
- Prediction steps at the simulation tick rate; reconciliation replays unacked
  inputs by input sequence after an authoritative snapshot.
- Interpolation runs per frame between received snapshots.

## Data ownership (derived)

Owns predicted local state and interpolation buffers. Authoritative state always
comes from the server; local predictions are discarded when they disagree.

## Integration points (derived)

- `AppEvent` → `InputMapping` → simulation commands (`MoveCommand`, `UseAbility`,
  `Interact`).
- NetworkAdapter (client side) for command egress and snapshot ingress.
- Renderer/Animation/Audio/UI receive a presentation snapshot.
- Presentation cues from GameplayFlow (cutscenes) are handed to Game
  presentation code.
- The connection set (`Primary`, `WarmCandidate`, …) is managed by Platform; the
  client runtime only sees a stable `WorldSession`. [J.5.9]

## Acceptance tests (Stage A, derived)

- [ ] Phase 2: input drives `MoveCommand` into a local simulation; state is
      observed and debug-rendered.
- [ ] Phase 4: prediction + reconciliation converge to the server state after a
      correction; input sequence numbers are continuous.
- [ ] Remote entities are interpolated smoothly between snapshots.
- [ ] Disabling controls during a cutscene is presentation only; the client still
      sends nothing that bypasses server validation.

## Future extensions

Make-before-break promotion handling via Platform directives (phase 13).

## Open questions

- [OQ-15](../system/11-open-questions.md#oq-15) — what simulation code the client runs.
- [OQ-4](../system/11-open-questions.md#oq-4) — input pipeline placement.
