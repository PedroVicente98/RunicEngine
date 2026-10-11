# Application

Contract seed for `Runic::Application`: what a Claude Code session needs to implement the Application work packages. When `Engine/Foundation/Application/` is created, this file becomes its `ARCHITECTURE.md`, and that file is authoritative from then on.

| Field | Value |
| --- | --- |
| Group | Foundation |
| Directory | `Engine/Foundation/Application/` |
| CMake target | `Runic::Application` |
| Namespace | `Runic::Application` |
| Headless | yes (the Window class is excluded from headless builds) |
| Status | Not started |

## Purpose

Every executable is an Application on the Engine's ApplicationHost: a stack of layers that receives events top-down and updates bottom-up, while the simulation's internal order stays inside its scheduler (ARCH § Application and LayerStack). This module owns the entry point, the layer machinery, loop policies, application events and the Window class.

## Owns

- `main()`. The Engine owns it and calls `CreateApplication()`, which each executable defines: RunicGame defines it twice (GameClient and GameServer), the Engine defines it for the Editor and the Cooker.
- `Application`, `ApplicationHost`, `Layer` and `LayerStack`. Layers and overlays are separate ranges; overlays always sit above layers.
- Loop policies per executable (table below).
- Application events: key press, resize, UI focus, connection state. Delivery is top layer first; a layer can mark an event handled.
- The `Window` class on GLFW, compiled only in client and tools builds (APP-2; decided in OQ-4). It creates a `GLFW_NO_API` window, extracts the native window and display handles, and turns keyboard, mouse and gamepad input into application events.
- Transitions as layer replacements at the loop's safe point: client Boot → Login → Character select → World; server Starting → Ready → Draining → Shutdown.

| Executable | Loop | Per iteration |
| --- | --- | --- |
| GameClient | Frame loop (vsync or capped) | Events, OnUpdate, OnRender, OnImGuiRender in development builds |
| GameServer | Fixed 60 Hz loop that sleeps until the next quantum | Events, OnUpdate; no rendering |
| Editor | Frame loop | Events, OnUpdate, OnRender, OnImGuiRender |
| Cooker | Job loop | Events, OnUpdate |

## Does not own

- The concrete layers. The Engine supplies Network and Diagnostics (Network module), Simulation (Simulation module), Prediction and Presentation (Client module), and the GameUI and ImGui overlays (UI module). The Platform supplies the services-bridge and lifecycle layers. RunicGame composes all of them and adds its own.
- Simulation order: it lives in the Simulation scheduler, never in layer order.
- Input mapping, actions and the command builder (Client, CLI-2).
- Renderer setup and bgfx. The Window hands out native handles as plain data; `glfw3native.h` and its platform macros never leave the Window class.

## Depends on

| Kind | Allowed |
| --- | --- |
| Engine modules | Core |
| Third-party (PRIVATE) | GLFW, for the Window class in client and tools builds only |

### Must never depend on

- Any Engine module other than Core.
- GLFW, bgfx, RmlUi, ImGui, an audio backend or Editor code in the headless build. `OnImGuiRender` is a hook with no ImGui types; ImGui is private to UI and Editor (OQ-6).
- RunicPlatform or RunicGame code. The Platform's server bindings include the `Layer` header; the Engine never includes theirs.
- Agones, Kubernetes, gRPC, NATS or databases.

## Public API sketch

ARCH's illustrative `Layer` (ARCH § Application and LayerStack):

```cpp
// Illustrative contract, not a frozen API.
class Layer {
public:
    virtual ~Layer() = default;
    virtual void OnAttach() {}                        // acquire resources
    virtual void OnDetach() {}                        // release everything acquired
    virtual void OnEvent(Event& event) {}             // top to bottom; stop when handled
    virtual void OnUpdate(const FrameTime& time) {}   // bottom to top, once per loop
    virtual void OnRender() {}                        // client and tools only
    virtual void OnImGuiRender() {}                   // development builds only
};
```

Headers (sketch, fixed in the contract session): `EntryPoint.hpp`, `Application.hpp`, `ApplicationHost.hpp`, `Layer.hpp`, `LayerStack.hpp`, `ApplicationEvents.hpp`, `LoopPolicy.hpp`, `Window.hpp` (client and tools builds only).

`Layer.hpp` is consumed by RunicPlatform: its C++ links only `Runic::Core`, `Runic::Application`, `Runic::NetworkTransport` and `Runic::SimulationPort`, and its services-bridge and lifecycle layers (PBND-1, PBND-2, PBND-4) implement `Layer`. Application's public headers therefore carry no library types and include nothing above Core.

## Threading and tick rules

The LayerStack is organization and event propagation only; it must cost close to nothing next to the simulation.

| Rule | Prevents |
| --- | --- |
| The LayerStack owns layers through `std::unique_ptr`; popping hands ownership back or destroys the layer explicitly | Leaks and double deletes from raw pointers |
| Push, pop and replace are queued and applied at one safe point per loop | Iterator invalidation and layers changing mid-update |
| Layers and overlays are separate ranges; overlays always sit above | Insertion-index bugs |
| Input and window events dispatch immediately without heap allocation (stack events, no strings) | Per-frame memory churn |
| Other events (connections, service replies) are queued and drained once per loop by the owning layer | Re-entrancy and ordering surprises |
| No layer blocks: network, services and disk work run on their own threads and only enqueue | Stalls that steal simulation time |
| Time comes from one clock as integer ticks or double seconds, never accumulated floats | Drift over long uptimes |
| No global Application singleton reachable from simulation threads; layers receive what they need at attach | Hidden coupling and data races |
| Simulation order lives in the scheduler, never in layer order | Combat results that depend on UI layers |
| Each layer's time per loop is measured and exported | Silent overhead |

On a transition, `OnDetach` of the old layer releases its GPU, audio and network resources before `OnAttach` of the next one runs.

## Work packages

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| APP-1 | Entry point, Application, ApplicationHost, Layer, LayerStack with queued changes at safe points, application events, loop policies | Layer order, events top-down until handled, updates bottom-up, safe-point push and pop, no allocation per event | CORE-1 | 1 |
| APP-2 | Window class on GLFW, compiled only in client and tools builds; keyboard, mouse and gamepad as application events | A window opens and closes; the headless build compiles without GLFW | APP-1 | 1 |

## Acceptance tests

The Done when column is the acceptance list. Slice 1 also requires: a window opens; layers push, pop and replace at safe points; events stop when handled.

## Open items

- [OQ-4](../system/11-open-questions.md#oq-4) (decided): Window is a class here; raw input becomes application events; input mapping lives in Client; there are no Platform or Input Engine modules.
- [OQ-6](../system/11-open-questions.md#oq-6) (decided): ImGui is private to UI and Editor.
