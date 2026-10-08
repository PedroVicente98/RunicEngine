# Animation

> **Contract seed.** When `Engine/Animation/` is created, copy this file to
> `Engine/Animation/ARCHITECTURE.md`; from then on that file is authoritative.
> Purpose, Owns, Does not own, Allowed dependencies, and Primary consumers come
> from Appendix J §J.4.13. Other sections are derived and are proposals until
> confirmed.

| Field | Value |
| --- | --- |
| Directory | `Engine/Animation/` |
| CMake target | `RunicAnimation` → `Runic::Animation` |
| Namespace | `Runic::Animation` |
| Roadmap phase | 2 — stubs as needed; grows with client presentation |
| Headless | **no** (presentation) — see open questions |
| Status | Not started |

## Purpose

Reusable skeleton, clips, animator, graph/state, and presentation-facing
animation runtime.

## Owns

`Skeleton`, `AnimationClip`, `Animator`, `AnimationGraph`, `AnimationState`,
blending and runtime evaluation.

## Does not own

Game-specific animation assets or combat rules; the renderer backend.

## Allowed dependencies

- Core, Assets.

## Forbidden dependencies

Renderer backend (bgfx), Simulation mutation, Combat, RunicPlatform, RunicGame.

## Primary consumers

Client, Game presentation, AI/editor previews.

## Public API (conceptual)

```text
Animation/
├── Public/
│   └── Animation.hpp        # split into explicit headers as the API grows
├── Source/
├── Tests/
└── CMakeLists.txt
```

## Threading / tick model (derived)

Evaluated per frame on the client. Never on the authoritative simulation path.

## Data ownership (derived)

Owns animation runtime state per presented entity. Combat timelines (server) are
owned by Combat, not by animation playback.

## Integration points (derived)

- Assets supplies skeletons/clips.
- Client/Game presentation drives animator parameters from replicated state and
  presentation cues.
- Renderer consumes evaluated poses.

## Acceptance tests (Stage A, derived)

- [ ] Load a skeleton and clip; evaluate a pose at time t.
- [ ] Blend two clips; state transitions in a simple graph.
- [ ] Not linked by the headless server target.

## Future extensions

Animation graphs authored by Game; editor previews.

## Open questions

- If the server ever needs bone-accurate hit volumes, part of animation
  evaluation would have to be headless. Appendix J does not address this; the
  current assumption is that combat volumes come from Combat timelines, not
  skeletal animation.
