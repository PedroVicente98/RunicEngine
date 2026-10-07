# Renderer

> **Contract seed.** When `Engine/Renderer/` is created, copy this file to
> `Engine/Renderer/ARCHITECTURE.md`; from then on that file is authoritative.
> Purpose, Owns, Does not own, Allowed dependencies, Primary consumers, layout,
> and API sketch come from Appendix J §J.4.11. Other sections are derived and are
> proposals until confirmed.

| Field | Value |
| --- | --- |
| Directory | `Engine/Renderer/` |
| CMake target | `RunicRenderer` → `Runic::Renderer` |
| Namespace | `Runic::Renderer` |
| Roadmap phase | 2 — local client |
| Headless | **no** — never linked by the server |
| Status | Not started (bgfx revisions already pinned) |

## Purpose

bgfx-backed rendering and visualization of client state, including production
scene primitives and debug shapes.

## Owns

- `Renderer` / `RenderDevice`.
- Render resources.
- Render scene.
- Camera.
- Debug rendering.
- The bgfx backend.

## Does not own

Authoritative simulation meaning, combat rules, Jolt/Flecs mutation, Game art
policy.

## Allowed dependencies

- Core, Assets, World as needed.
- bgfx — **private** backend dependency.

## Forbidden dependencies

Simulation mutation, ECS world mutation, Physics/Jolt, Combat, AI, networking,
RunicPlatform, RunicGame. [J.11.2]

## Primary consumers

Client, Editor, Game presentation.

## Public API (conceptual)

```text
Renderer/
├── Public/
│   ├── Renderer.hpp
│   ├── RenderDevice.hpp
│   ├── RenderTypes.hpp
│   ├── RenderResource.hpp
│   ├── RenderScene.hpp
│   ├── Camera.hpp
│   └── DebugRenderer.hpp
├── Source/
│   ├── Scene/
│   ├── Debug/
│   └── Bgfx/
├── Tests/
└── CMakeLists.txt
```

```cpp
debug.DrawLine(a, b, color);
debug.DrawSphere(center, radius, color, opacity);
debug.DrawCapsule(a, b, radius, color, opacity);
```

Renderer consumes plain Runic/math presentation data. Simulation, Jolt, combat,
AI, and networking never issue bgfx commands directly. [J.4.11]

## Threading / tick model (derived)

Runs per frame on the client's render/main thread. It reads presentation data
prepared by Client (interpolated snapshots), never the live Flecs world during a
simulation tick.

## Data ownership (derived)

Owns GPU resources and the render scene. Holds no authoritative state.

## Integration points (derived)

- Client builds a presentation snapshot → `RenderScene`.
- `PhysicsDebug` data → `DebugRenderer` (drawn here, extracted in Physics).
- Native window handle comes from the Application OS/window adapter
  ([OQ-4](../system/11-open-questions.md#oq-4)).

## Acceptance tests (Stage A, derived)

- [ ] Initialize with a window handle and clear/present a frame.
- [ ] Debug draw: lines, spheres, capsules from plain math data.
- [ ] Phase 2 proof: debug-render a simulation's state observed by the client.
- [ ] Phase 3 proof: render a cooked mesh loaded through Assets.
- [ ] No bgfx type or header in `Public/`.
- [ ] The headless server target does not link Renderer or bgfx.

## Future extensions

Production scene features (materials, lighting, streaming). The earlier README
notes a Vulkan roadmap that is not implemented; bgfx remains the backend.

## Open questions

- [OQ-4](../system/11-open-questions.md#oq-4) — window/surface ownership.
- Renderer/UI specifics from v2.1 §G are not imported
  ([OQ-12](../system/11-open-questions.md#oq-12)).
