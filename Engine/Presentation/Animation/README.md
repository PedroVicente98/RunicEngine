# Runic::Animation

Presentation · Headless: no (presentation) · Packages: ANIM-1 · Status:
empty until ANIM-1 (slice 23) starts the module

**Holds** (RunicDocs › Repositories, build targets and processes › Engine modules): Skeletons, clips, animator, blending, poses (scope set by the open animation decision)

**May depend on** (a ceiling; `cmake/RunicModuleCatalog.cmake` enforces it): Core, Assets

## Folders

| Folder | Holds | Packages |
| --- | --- | --- |
| `Public/` | The module's API: the only headers other modules may include, by their path under `Public/` (flat unless noted below). No library types, except an approved exception recorded in `ARCHITECTURE.md`. | every package that adds API |
| [`Source/Skeletons/`](Source/Skeletons/README.md) | Skeletons. | ANIM-1 (slice 23) |
| [`Source/Clips/`](Source/Clips/README.md) | Animation clips. | ANIM-1 (slice 23) |
| [`Source/Animator/`](Source/Animator/README.md) | The animator: blending and poses (scope set by the open animation decision). | ANIM-1 (slice 23) |
| `Tests/` | GoogleTest tests, registered with CTest; one test executable per module. | every package |
| `ARCHITECTURE.md` | The module contract: purpose, public API, ownership, allowed and forbidden dependencies, threading, work packages and their status. | the first package's contract session |
| `CMakeLists.txt` | One `runic_add_module(...)` call. | the first package |


## How the folders fill

Nothing here is code yet. ANIM-1 (slice 23) starts the module: its contract session writes `ARCHITECTURE.md`, `CMakeLists.txt`, the public headers and the failing acceptance tests; its implementation session fills the `Source/` folders the package needs. A folder whose package has not started holds only its README. Every folder exists from the start (decided 2026-11-12) so the layout never moves; a package writes inside this module's folder only, and reports any change another module needs instead of making it.

## Sources

The catalog row and package slices were read from RunicDocs › Architecture (as of 2026-11-13). When RunicDocs and this file disagree, RunicDocs wins.

- RunicDocs › Architecture › Client runtime (Game on Engine) — the open animation decision
- RunicDocs › Architecture › Godot integration (Engine) › Open decisions
