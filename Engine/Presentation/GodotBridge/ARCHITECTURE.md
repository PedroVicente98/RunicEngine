# Runic::GodotBridge — module contract

Group: Presentation · Headless: **no** · Target: `Runic::GodotBridge` (`runic_godotbridge`) · Packages: GDB-1 to GDB-13

Source: RunicDocs › Architecture › Godot integration (Engine), Repositories, build targets and processes
(Engine modules), Render data, extraction and world UI. Status: **slice 0 code only** (the LibGodot host, written
for the slice 0 spike, see [docs/build/GDT-1.md](../../../docs/build/GDT-1.md)); GDB-1 is not started.

## Purpose

The only Engine module that talks to Godot at runtime. Godot is interface and renderer only: GodotBridge owns the
LibGodot instance, the window and input it provides (the GodotLayer), the presenter that applies the RenderFrame
to the RenderingServer, the debug layers, world UI, the view-model binder and the audio backend on the
AudioServer. The RenderFrame is the only door from the Runic client into Godot.

## Public API

| Header | Types | Since |
| --- | --- | --- |
| `GodotRuntime.hpp` | `GodotRuntime` (load, create, start, iterate, destroy one LibGodot instance), `GodotTarget` (editor or template library), `GodotInitLevel` and level hooks for class registration | slice 0 |

No Godot or godot-cpp type appears in a public header: the catalog row has no public exception, and
registration hooks use Runic types (`GodotInitLevel`). GodotTools, whose public headers may carry godot-cpp
types, registers its classes through these hooks.

## Owns

- The process's single LibGodot instance: the opened library, its `libgodot_create_godot_instance` and
  `libgodot_destroy_godot_instance` entry points, the argument storage Godot keeps pointers into, and the
  GDExtension entry point of the host executable (`libgodot://main`).
- From GDB-1: the GodotLayer at the top of the client LayerStack, the window from the DisplayServer, input
  forwarded as AppEvents after Godot's UI. Later packages: presenter, debug layers, world UI, binder, audio backend.

## Does not own

- Which library a process loads, and its arguments: the host executable decides (RunicEditor: editor target;
  GameClient: editor target in development, template when shipping).
- Building libgodot: `cmake/RunicGodot.cmake` and `scripts/godot/` (BUILD-1).
- Editor tools, plugins and docks: Runic::GodotTools.
- Any game rule, simulation or physics state: Godot never reads Flecs, Jolt or domain modules.

## Allowed dependencies

Catalog row (ceiling): Runic::Application, Runic::Client, Runic::Audio, Runic::Assets; third parties libgodot,
godot-cpp (private) and the system loader (`dl`). Slice 0 links only godot-cpp and `dl`.

## Forbidden dependencies

Physics, Simulation or any domain module state (the presenter draws an extracted frame); GodotTools or Godot's
editor classes in code the shipping client runs; Platform and Game targets. No headless target may reach this
module: it is marked presentation and the configure-time headless check fails on any path to it.

## Threading and tick model

Every `GodotRuntime` call happens on the thread that called `Create()` (Godot's scene tree is not thread-safe);
the class refuses calls from another thread. One `Iterate()` is one Godot frame; in the client it is the last
thing the frame does (GDB-1). LibGodot allows one instance per process and never a second one after the first
is destroyed; `Create()` reports both cases.

## Data ownership

The host owns the `GodotRuntime` object. The library stays mapped after `Destroy()` because Godot registers exit
handlers and thread-locals inside it. godot-cpp's bindings are invalid after `Destroy()`.

## Integration points

- `AddInitializeHook` / `AddDeinitializeHook`: run inside Godot's initialization once per level from Scene up
  (Editor only with the editor library), and in reverse on shutdown. GodotTools registers the RunicEditor's
  classes here; the client registers presentation helpers here from GDB-1.
- Arguments reach Godot unchanged, because Godot's project manager and Play buttons relaunch the host executable
  with their own arguments.

## Work packages

| ID | Package | Done when | Status |
| --- | --- | --- | --- |
| (slice 0) | LibGodot host for the slice 0 spike: `GodotRuntime` | The client and the RunicEditor start Godot through it; `GodotRuntime.StartsGodotHeadlessAndExits` passes when libgodot is built | Done in slice 0 |
| GDB-1 | GodotLayer: the LibGodot instance at the top of the client LayerStack; window from the DisplayServer; input forwarded as AppEvents after Godot's UI; headless mode | The client starts Godot windowed and headless and exits cleanly; UI clicks never reach the stack | Not started (needs APP-1) |
| GDB-2 to GDB-13 | Debug drawing, presenter, render data, world UI, binder, audio backend, production look | See RunicDocs › Modules, work packages | Not started |

## Future extensions

Restarting Godot in the same process and drawing into a host-owned surface are still being merged upstream;
neither is needed while Godot owns the window. Shipping clients should link godot-cpp built for
`template_release` (`RUNIC_GODOT_CPP_TARGET`), so no editor-flavored binding code ships to players.
