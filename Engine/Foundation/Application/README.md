# Runic::Application

Foundation · Headless: yes · Packages: APP-1 · Status:
empty until APP-1 (slice 1) starts the module

**Holds** (RunicDocs › Repositories, build targets and processes › Engine modules): Entry point, Application, ApplicationHost (Run and RunFrame), LayerStack, loop policies, application events; no window class (the window comes from Godot)

**May depend on** (a ceiling; `cmake/RunicModuleCatalog.cmake` enforces it): Core

## Folders

| Folder | Holds | Packages |
| --- | --- | --- |
| `Public/` | The module's API: the only headers other modules may include, by their path under `Public/` (flat unless noted below). No library types, except an approved exception recorded in `ARCHITECTURE.md`. | every package that adds API |
| [`Source/EntryPoint/`](Source/EntryPoint/README.md) | The Engine-owned `main()`: it reads the command line (UTF-8 on every platform), calls the `CreateApplication` each program defines, and hands the result to the ApplicationHost. | APP-1 (slice 1) |
| [`Source/Host/`](Source/Host/README.md) | ApplicationHost: `Run()` repeats `RunFrame()` under the loop policy, and `RunFrame()` is one complete frame (drain queued events, update bottom-up, render bottom-up, apply layer changes), so a host that does not own the loop can still drive one frame at a time. | APP-1 (slice 1) |
| [`Source/Layers/`](Source/Layers/README.md) | Layer and LayerStack: layers and overlays as separate ranges owned through `unique_ptr`; push, pop and replace queued and applied at the frame's one safe point; each layer's time measured. | APP-1 (slice 1) |
| [`Source/AppEvents/`](Source/AppEvents/README.md) | Application events (key, mouse, resize, focus, quit, connection state): dispatched top-down until handled, with no heap allocation per event; queued events drained once per frame. | APP-1 (slice 1) |
| [`Source/LoopPolicies/`](Source/LoopPolicies/README.md) | Loop policies: a fixed 60 Hz loop that sleeps until the next quantum (GameServer, RunicBot), and a loop paced by the top layer (GameClient and RunicEditor, where Godot's iteration waits for vsync or for the editor's low-processor mode). | APP-1 (slice 1) |
| `Tests/` | GoogleTest tests, registered with CTest; one test executable per module. | every package |
| `ARCHITECTURE.md` | The module contract: purpose, public API, ownership, allowed and forbidden dependencies, threading, work packages and their status. | the first package's contract session |
| `CMakeLists.txt` | One `runic_add_module(...)` call. | the first package |

- There is no window class: the window comes from Godot through the GodotLayer (GodotBridge).

## How the folders fill

Nothing here is code yet. APP-1 (slice 1) starts the module: its contract session writes `ARCHITECTURE.md`, `CMakeLists.txt`, the public headers and the failing acceptance tests; its implementation session fills the `Source/` folders the package needs. A folder whose package has not started holds only its README. Every folder exists from the start (decided 2026-11-12) so the layout never moves; a package writes inside this module's folder only, and reports any change another module needs instead of making it.

## Sources

The catalog row and package slices were read from RunicDocs › Architecture (as of 2026-11-13). When RunicDocs and this file disagree, RunicDocs wins.

- RunicDocs › Architecture › Application and LayerStack (Engine)
- RunicDocs › Architecture › Repositories, build targets and processes › Engine modules, Three event families
