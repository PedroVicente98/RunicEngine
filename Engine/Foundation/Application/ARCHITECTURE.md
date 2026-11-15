# Runic::Application — module contract

Group: Foundation · Headless: **yes** · Target: `Runic::Application` (`runic_application`) · Packages: APP-1

Source: RunicDocs › Architecture › Application and LayerStack (Engine); Repositories, build targets and processes
(Engine modules, Three event families); Modules, work packages and the implementation contract (APP-1). Status:
**APP-1 implemented** (slice 1); awaiting the verify step. The folder map is [README.md](README.md); the slice brief is
[RunicEngine#144](https://github.com/PedroVicente98/RunicEngine/issues/144) and the package
[RunicEngine#8](https://github.com/PedroVicente98/RunicEngine/issues/8).

## Purpose

Every executable is an Application on the Engine's ApplicationHost: a stack of layers that receives events top-down
and is updated and rendered bottom-up, while the simulation's own order stays inside its scheduler. This module owns
the entry point, the host's loop and frame, the LayerStack and its one safe point per frame, the loop policies and the
application events. There is no window class: the window comes from Godot through the GodotLayer (GodotBridge).

## Public API

Namespace `Runic::Application`, with no exception (slice 1 decision D2): the class `Application` is written
`Runic::Application::Application` by the executables, once, in `CreateApplication`.

| Header | Holds |
| --- | --- |
| `EntryPoint.hpp` | `CreateApplication(const CommandLine&)`, which each executable defines; `Detail::RunMain`; `RUNIC_ENTRY_POINT()`, which defines `main()` where an executable expands it, once, next to its `CreateApplication` |
| `Application.hpp` | `ApplicationSpec` (name, loop policy, frame limit) and `Application` (its layers, `OnStopped`) |
| `ApplicationHost.hpp` | `ApplicationHost`: `Run`, `Start`, `RunFrame(const FrameTime&)`, `Shutdown`, `RequestStop` |
| `Layer.hpp` | `Layer` (attach, detach, event, update, render) and `LayerContext` (what a layer may ask of the host) |
| `LayerStack.hpp` | `LayerStack`: layers and overlays as separate ranges, changes queued for the safe point |
| `AppEvent.hpp` | `AppEvent` over key, mouse button, mouse motion, wheel, resize, focus and quit-request events |
| `FrameTime.hpp` | `FrameTime`: the frame number, the delta and the time since the first frame, in `Core::Duration` |
| `LoopPolicy.hpp` | `LoopPolicy::Fixed(hz)` and `LoopPolicy::PacedByTopLayer()`; `LoopClock` and `MakeSystemClock` |
| `CommandLine.hpp` | `CommandLine`: UTF-8 on every platform, argv[0] first, every argument unchanged; `QuitAfterFrames` |

**Where `main()` is defined.** Not in the module's static library: it would also be linked into the module's own test
executable, which already gets one from GoogleTest. Not by including a header either (APP-1 until 2026-11-15): a
function defined in a header is defined again in every source file that includes it, an ODR violation the linker
reports as a second `main()`. The executable expands `RUNIC_ENTRY_POINT()` once, so `main()` is defined in its own
source file, while its body stays the Engine's (`Detail::RunMain`) and `EntryPoint.hpp` only declares.

## Owns

- `main()`'s behavior: the console log sink (Info and below to stdout, warnings and errors to stderr), SIGINT and
  SIGTERM as a stop request, `CreateApplication`, then the host.
- The frame: the queued events top-down, every `OnUpdate` bottom-up, every `OnRender` bottom-up, then the safe point,
  the only place layers change. A stop request ends the loop after the current frame's safe point.
- The loop: `Fixed` sleeps until frame n's deadline, `TickRate::TimeOfTick(n)` after the start, so late frames never
  shift the schedule; `PacedByTopLayer` never sleeps and measures the frame on the monotonic clock.
- The event queue: 256 events in a `Core::FixedRingBuffer`, drained once per frame; a full queue drops and counts.
- Layer ownership: `std::unique_ptr` in the stack; a replaced layer detaches before its replacement attaches;
  shutdown detaches overlays, then layers, top-down.

## Does not own

- The window, input and their conversion into events (GodotBridge's GodotLayer posts them).
- Simulation order and ticks (Simulation, SIM-1): layer order never decides simulation results.
- Metrics per layer (Core's metrics registry, CORE-2, slice 3): no layer timing is exported yet.
- Configuration beyond `--quit-after` (Core, CORE-8).

## Allowed dependencies

Catalog row: Runic::Core (PUBLIC: `FrameTime`, `Result` and `FixedRingBuffer` appear in the public headers). On
Windows, the system's shell32 for the UTF-8 command line.

## Forbidden dependencies

Every other Runic module, Platform and Game code, Godot and any presentation library (the module is headless), any I/O
library.

## Threading and tick model

Everything runs on the thread that calls `Run` (or `RunFrame`), except `RequestStop`, which is thread-safe and
signal-safe. Layers never block; I/O threads enqueue through their own queues, never through the LayerStack.

## Data ownership

The host owns the application, the event queue and the `LayerContext` it hands to layers; the application owns the
LayerStack, which owns the layers. A layer may keep the context for as long as it is attached.

## Integration points

- Executables: `CreateApplication`, `ApplicationSpec`, `Application::OnStopped`, `--quit-after`.
- Layers: `Layer`, `LayerContext` (`PostEvent`, `RequestStop`, `PushLayer`, `PushOverlay`, `PopLayer`, `ReplaceLayer`,
  `Command`, `FramesRun`).
- GodotBridge: the GodotLayer posts `AppEvent`s and requests the stop when Godot quits; it relies on the top layer's
  `OnRender` running last.
- Tests and hosts that own the loop: `ApplicationHost::Start`, `RunFrame` and `Shutdown`, and a `LoopClock`.

## Work packages

| ID | Package | Done when | Status |
| --- | --- | --- | --- |
| APP-1 | Entry point, Application, ApplicationHost, Layer, LayerStack with queued changes at safe points, application events, loop policies, RunFrame; the Engine's RunicServer program on the entry point (fixed 60 Hz loop) | Layer order, events top-down until handled, updates bottom-up, safe-point push and pop, no allocation per event; RunicServer starts and stops cleanly | **Implemented** (slice 1): `RunicApplicationTests` (21 tests) and `programs.runic_server`; awaiting the verify step |

## Future extensions

- Per-layer timings in the metrics registry (with CORE-2).
- Connection-state events (with the Network layer, slice 12).
- Settings beyond `--quit-after` from process configuration (CORE-8).
