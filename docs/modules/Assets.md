# Assets

Contract seed for `Runic::Assets`: what a Claude Code session needs to implement the Assets work packages. When `Engine/Foundation/Assets/` is created, this file becomes its `ARCHITECTURE.md`, and that file is authoritative from then on.

| Field | Value |
| --- | --- |
| Group | Foundation |
| Directory | `Engine/Foundation/Assets/` |
| CMake target | `Runic::Assets` |
| Namespace | `Runic::Assets` |
| Headless | yes |
| Status | Not started |

## Purpose

Assets is the runtime side of cooked data: asset IDs and handles, a cache, the native cooked formats, random-access packages and manifests, client and server package selection, and asynchronous streaming. The shipping client and server read only Runic-native packages, never Blender or glTF files (ARCH § World authoring and cooking).

## Owns

- Handles and the cache keyed by `AssetId`, a plain value type in Core that Assets gives meaning to (ASSET-1).
- Loading of the native cooked mesh and collision formats (ASSET-1).
- Random-access packages and manifests with Zstandard decompression (ASSET-2). A cooked region can be loose entries or one package (for example `A01.runicpak`) with an internal random-access table of contents. A continent is never compressed as one sequential stream.
- Client and server package selection (ASSET-2). The server never loads textures, vegetation shaders or other presentation data.
- Asynchronous streaming that loads cells without stalling a tick or a frame (ASSET-2).

| Client cook | Server cook |
| --- | --- |
| Meshes, mesh LOD and HLOD | Jolt collision data |
| KTX2 textures and material data | Detour navigation tiles |
| Vegetation and presentation instances | Spawn and gameplay markers, spatial metadata |
| Audio and presentation assets | Region rules and server world objects |
| Client water render data | Server water fragments and flow representation |
| Client prediction data as needed | Authoritative simulation data |

## Does not own

- Authoring and cooking (Cooker).
- `ContentId`, schemas and definition registries (Content).
- Deciding which cells to load: World's activation volumes (WORLD-3) emit load and unload requests (see OQ-3).
- GPU upload (Renderer), collision bodies (Physics), nav tiles at runtime (Navigation), water fragments at runtime (Water).

## Depends on

| Kind | Allowed |
| --- | --- |
| Engine modules | Core, Content |
| Third-party (PRIVATE) | Zstandard for package decompression (ASSET-2) |

### Must never depend on

- World or any other module outside the list above. The catalog allows only Core and Content; OQ-3's proposal also keeps World from linking Assets.
- Renderer, bgfx or any presentation library (the module is headless).
- RunicPlatform or RunicGame code; Agones, Kubernetes, gRPC, NATS or databases.

## Public API sketch

Sketch, fixed in the contract session: `AssetId.hpp`, `AssetHandle.hpp`, `AssetCache.hpp`, `CookedMesh.hpp`, `CookedCollision.hpp`, `AssetPackage.hpp`, `AssetManifest.hpp`, `AssetStreamer.hpp`.

## Threading and tick rules

- Disk work runs on its own threads and only enqueues; no layer or tick blocks on it (ARCH § Application and LayerStack).
- A load completes into a queue that its consumer drains at a safe point; I/O callbacks never touch Flecs.
- Packages allow independent random access so cells can stream.

## Work packages

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| ASSET-1 | AssetId, handles, cache, loading of native cooked mesh and collision formats | A cooked test mesh loads on client and server | CNT-1 | 9 |
| ASSET-2 | Random-access packages and manifests with Zstandard decompression, client and server package selection, asynchronous streaming | Cells load from a package without stalling a tick or a frame | ASSET-1, WORLD-3 | 21 |

## Acceptance tests

The Done when column is the acceptance list. Slice 9 cooks a Blender test mesh (COOK-1) that this module loads; slice 21 walks across regions, down into a cave volume and up a cliff with activation-volume streaming.

## Open items

- [OQ-3](../system/11-open-questions.md#oq-3) (still open, narrowed): Assets depends only on Core and Content; World's streaming state emits load and unload requests. Who performs the load (Client or the server runtime) and why ASSET-2 needs WORLD-3 must be confirmed in the ASSET-2 contract session. Proposal: the catalog wins, no World → Assets edge. Ask the owner; do not decide it silently.
