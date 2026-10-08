# Assets

> **Contract seed.** When `Engine/Assets/` is created, copy this file to
> `Engine/Assets/ARCHITECTURE.md`; from then on that file is authoritative.
> Purpose, Owns, Does not own, Allowed dependencies, Primary consumers, and layout
> come from Appendix J §J.4.10. Other sections are derived and are proposals
> until confirmed.

| Field | Value |
| --- | --- |
| Directory | `Engine/Assets/` |
| CMake target | `RunicAssets` → `Runic::Assets` |
| Namespace | `Runic::Assets` |
| Roadmap phase | 2 (basic) — native formats in 3, packages in 7 |
| Headless | yes (the server loads server packages such as collision/navigation data) |
| Status | Not started |

## Purpose

Runtime loading, handles, packages, caches, and native cooked asset formats.

## Owns

- `AssetId`, asset handles.
- Package registry and loading.
- Runtime cache.
- Client/server package selection.

## Does not own

Blender source import, mesh optimization, texture transcode, Recast/Jolt cooking
(all Cooker); concrete Game assets.

## Allowed dependencies

- Core, Content.

## Forbidden dependencies

Renderer, Animation, Audio, UI (they consume Assets), Cooker and its tool-only
libraries, Simulation, gameplay modules, RunicPlatform, RunicGame.

## Primary consumers

Renderer, Animation, Audio, UI, World streaming, Game client/server.

## Public API (conceptual)

```text
Assets/
├── Public/
│   ├── AssetId.hpp
│   ├── AssetTypes.hpp
│   ├── AssetPackage.hpp
│   ├── AssetRegistry.hpp
│   ├── AssetLoader.hpp
│   └── AssetHandle.hpp
├── Source/
│   ├── Loading/
│   ├── Cache/
│   └── Packages/
├── Tests/
└── CMakeLists.txt
```

Runtime applications consume native cooked assets. They never load `.blend`
source files. [J.4.10]

## Threading / tick model (derived)

Loading is asynchronous (worker threads); handles become ready later. The
simulation thread never blocks on asset I/O; readiness is observed at safe
boundaries.

## Data ownership (derived)

The cache owns loaded data; handles are reference-counted or generation-checked
views. Asset bytes are never authoritative gameplay state.

## Integration points (derived)

- Cooker writes the native formats/packages that Assets reads.
- Renderer/Animation/Audio/UI request assets by `AssetId`.
- Server builds select server packages (no client-only data); client builds
  select client packages (no server-only data).

## Acceptance tests (Stage A, derived)

- [ ] Load a native package from disk; resolve assets by `AssetId`.
- [ ] Cache hit returns the same data; handle lifetime is safe after unload
      requests.
- [ ] Missing/corrupt asset produces an error, not a crash.
- [ ] Server package selection excludes client-only assets.
- [ ] Builds headless.

## Future extensions

Phase 7: Region/Sector/Cell-driven package streaming.

## Open questions

- [OQ-3](../system/11-open-questions.md#oq-3) — World ↔ Assets dependency.
- [OQ-17](../system/11-open-questions.md#oq-17) — client/server package split.
- Where native format definitions shared with Cooker live (see [Cooker](Cooker.md)).
