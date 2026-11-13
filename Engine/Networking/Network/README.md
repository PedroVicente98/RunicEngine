# Runic::Network

Networking · Headless: yes · Packages: NET-1, NET-2, NET-3, NET-4, NET-5, NET-6, NET-7, NET-8, NET-9, NET-10 · Status:
empty until NET-1 (slice 12) starts the module

**Holds** (RunicDocs › Repositories, build targets and processes › Engine modules): Transport interface (header-only Runic::NetworkTransport target), ingress and egress queues, all wire encoding (snapshots, quantization, deltas, simulation-link messages such as ghost deltas and transfers), interest and priority budgets, prediction and reconciliation, parallel replication, world transitions; the Network and Diagnostics layers

**May depend on** (a ceiling; `cmake/RunicModuleCatalog.cmake` enforces it): Application, Simulation, DistributedSimulation, Gameplay, Perception

## Folders

| Folder | Holds | Packages |
| --- | --- | --- |
| `Public/` | The module's API: the only headers other modules may include, by their path under `Public/` (flat unless noted below). No library types, except an approved exception recorded in `ARCHITECTURE.md`. | every package that adds API |
| [`NetworkTransport/`](NetworkTransport/README.md) | Headers of the header-only `Runic::NetworkTransport` interface target: the transport interface and the connect-ticket hook the Platform's GameNetworkingSockets transport implements. | NET-1 (slice 12) |
| [`Source/NetworkLayer/`](Source/NetworkLayer/README.md) | The Network layer: tick-safe ingress and egress queues and the connect-ticket hook, over the transport interface the Platform implements. | NET-1 (slice 12) |
| [`Source/Encoding/`](Source/Encoding/README.md) | All wire encoding, envelope included: quantization, deltas, the snapshot codec. | NET-2 (slice 13), NET-5 (slice 36) |
| [`Source/Snapshots/`](Source/Snapshots/README.md) | Snapshot extraction, deltas against the last acknowledged state, interpolation buffers. | NET-2 (slice 13) |
| [`Source/Reconciliation/`](Source/Reconciliation/README.md) | Prediction and reconciliation: input history, replay, blend or snap. | NET-3 (slice 14) |
| [`Source/Interest/`](Source/Interest/README.md) | Interest: candidates by cell, priority and budgets, visibility tiers applied, detail levels and the client's focus hint; later occlusion-aware interest. | NET-4 (slice 25), NET-10 (slice 44+) |
| [`Source/SimulationLinks/`](Source/SimulationLinks/README.md) | Ghost deltas and transfer messages over simulation-to-simulation links. | NET-5 (slice 36) |
| [`Source/ParallelReplication/`](Source/ParallelReplication/README.md) | Parallel replication and the degradation order. | NET-6 (slice 38) |
| [`Source/DiagnosticsLayer/`](Source/DiagnosticsLayer/README.md) | The Diagnostics layer: network and replication metrics into the Core registry, and the RunicEditor attach channel (development and staging builds only). | NET-7 (slice 39) |
| [`Source/WorldTransitions/`](Source/WorldTransitions/README.md) | World transitions: leaving one world's simulation after a checkpoint and joining another with a new connect ticket. | NET-8 (slice 44+) |
| [`Source/Integrity/`](Source/Integrity/README.md) | The client-integrity channel for a commercial anti-cheat provider, on its own lane. | NET-9 (slice 44+) |
| `Tests/` | GoogleTest tests, registered with CTest; one test executable per module. | every package |
| `ARCHITECTURE.md` | The module contract: purpose, public API, ownership, allowed and forbidden dependencies, threading, work packages and their status. | the first package's contract session |
| `CMakeLists.txt` | One `runic_add_module(...)` call. | the first package |


## How the folders fill

Nothing here is code yet. NET-1 (slice 12) starts the module: its contract session writes `ARCHITECTURE.md`, `CMakeLists.txt`, the public headers and the failing acceptance tests; its implementation session fills the `Source/` folders the package needs. A folder whose package has not started holds only its README. Every folder exists from the start (decided 2026-11-12) so the layout never moves; a package writes inside this module's folder only, and reports any change another module needs instead of making it.

## Sources

The catalog row and package slices were read from RunicDocs › Architecture (as of 2026-11-13). When RunicDocs and this file disagree, RunicDocs wins.

- RunicDocs › Architecture › Networking replication, interest and handoff (Engine)
- RunicDocs › Architecture › Networking stack, sessions and responsiveness
