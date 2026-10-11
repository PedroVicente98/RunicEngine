# Cooker

Contract seed for `Runic::Cooker`: what a Claude Code session needs to implement the Cooker work packages. When `Engine/Tools/Cooker/` is created, this file becomes its `ARCHITECTURE.md`, and that file is authoritative from then on.

| Field | Value |
| --- | --- |
| Group | Tools |
| Directory | `Engine/Tools/Cooker/` |
| CMake target | `Runic::Cooker` |
| Namespace | `Runic::Cooker` |
| Headless | tool (never shipped; never linked by a runtime) |
| Status | Not started |

## Purpose

RunicCooker is one Runic executable: the cooking orchestration layer and the Runic native-format writers. It turns authored assets and world data into runtime formats and integrates mature libraries for the algorithms the Engine should not reimplement. The Editor launches it as a background worker, the Runic Blender add-on calls it, and build machines run the same binary (ARCH § World authoring and cooking). The shipping client and server never read Blender or glTF files.

## Owns

- Orchestration, third-party library integration, Runic format writers, validation and, later, incremental builds.
- One authored truth, many runtime products: from one source the Cooker emits GPU mesh LODs and HLOD, KTX2 textures, Jolt collision shapes, Recast and Detour nav tiles, terrain cells and masks, world placements and metadata, water fragments and flow fields, server-only gameplay and spatial data, and client-only presentation data.
- Region, sector and cell splitting with aprons, so derived data sees neighbor context and borders show no seam.
- Separate client and server packages (decided in OQ-17).

| Pipeline stage | Technology |
| --- | --- |
| glTF import | fastgltf |
| Mesh optimization and LOD | meshoptimizer |
| Tangents | MikkTSpace |
| Optional UV and lightmap atlas | xatlas |
| Texture cooking | KTX-Software and Basis Universal (KTX2 container) |
| Physics cooking | Jolt Physics |
| Navmesh cooking | Recast |
| Package compression | Zstandard |
| Orchestration and package writer | RunicCooker (C++) |

| Client cook | Server cook |
| --- | --- |
| Meshes, mesh LOD and HLOD | Jolt collision data |
| KTX2 textures and material data | Detour navigation tiles |
| Vegetation and presentation instances | Spawn and gameplay markers, spatial metadata |
| Audio and presentation assets | Region rules and server world objects |
| Client water render data | Server water fragments and flow representation |
| Client prediction data as needed | Authoritative simulation data |

Cooked world layout (ARCH's illustration): a small top-level world file says what exists and how the world is organized (world ID and instance policy, coordinate and global-origin policy, world bounds, the SpatialLayer, continent and region registries, global ocean and environment references, cloud and transition policies, package and dependency indices). A logical region becomes a header, cells, terrain pages, collision pages, nav tiles and HLOD, as loose entries or packed into one random-access package. A continent is never compressed as one sequential stream.

Water pipeline (COOK-7): export the curve and metadata, validate the source, sample the 3D centerline, generate surface and LOD data, generate bounds and depth data, bake the flow representation including ocean current fields, build the connection graph, split into Region, Sector and Cell fragments, generate boundary aprons, emit client water assets, emit server water assets. Validation rejects zero or negative width, invalid depth, unintended self-intersection, large elevation jumps in a Smooth connection, missing or dangling connections, neighbor boundary mismatches, a waterfall modeled without a FlowVolume, invalid route topology and duplicate IDs.

## Does not own

- Authoring: Blender, the Runic Blender add-on (an Engine tool outside the C++ module catalog that exports sources and launches cooks) and all world content (RunicGame).
- Runtime loading and caches (Assets); runtime use of collision (Physics), nav tiles (Navigation) and water fragments (Water).
- Launching from the Editor (Editor, EDT-4).
- Definition registries and schemas (Content); the Cooker validates through them (COOK-2).

## Depends on

| Kind | Allowed |
| --- | --- |
| Engine modules | Core, Content, Assets, World |
| Third-party (PRIVATE) | fastgltf, meshoptimizer, MikkTSpace, xatlas (optional), KTX-Software and Basis Universal, Jolt (cooking), Recast, Zstandard |

### Must never depend on

- Being linked by the server or a shipping client: build targets keep toolchain libraries out of runtimes.
- Renderer, Client, UI, Audio or Editor.
- RunicPlatform or RunicGame code. Game content arrives as input data, never as linked code.

## Public API sketch

The Cooker is an executable with a job loop. Headers (sketch, fixed in the contract session): `CookJob.hpp`, `CookPipeline.hpp`, `CookValidation.hpp`, `CookProtocol.hpp` (progress over the local protocol used by EDT-4), plus one writer per runtime format added by the package that first needs it.

## Threading and tick rules

- The Cooker runs a job loop: events, OnUpdate. It is never inside a simulation tick.
- It runs as its own process; a long or failed cook never freezes or crashes the Editor.
- Package output always allows independent random access so cells can stream.

## Work packages

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| COOK-1 | First cooker proof: glTF through fastgltf into a Runic mesh and Jolt collision | A Blender mesh renders and is walkable | ASSET-1, PHYS-1 | 9 |
| COOK-2 | Validation of data-driven definitions | Invalid definitions fail the cook with clear errors | CNT-2 | 20 |
| COOK-3 | World cooking: regions, sectors, cells, manifests, packages, Zstandard | Two regions on two layers cook into random-access packages | COOK-1, WORLD-2 | 21 |
| COOK-4 | Textures (KTX2 and Basis), mesh LODs (meshoptimizer), tangents (MikkTSpace) | Textured meshes with LODs cook into client-only packages | COOK-1 | 22 |
| COOK-5 | Skinned meshes and animation clips | A rigged character and its clips cook | COOK-1 | 23 |
| COOK-6 | Navigation tiles with Recast | Tiles cook per cell | COOK-3 | 31 |
| COOK-7 | Water fragments and current fields | A river and an ocean field cook with their cells | COOK-3 | 40 |
| COOK-8 | Incremental cooking | Only changed inputs cook again | COOK-3 | 44+ |

## Acceptance tests

The Done when column is the acceptance list. The first cooker proof: a test region exported from Blender to GLB is cooked into one Runic mesh and one Jolt collision asset; the client renders the mesh, client and server collision work, and a character walks on it. KTX2, Recast, xatlas, HLOD, terrain splatmaps, world manifests, vegetation, compression, region splitting and the complete add-on all wait until after this proof. Test sources are Engine test assets, never RunicGame content.

## Open items

- The Cooker writes formats that Physics (collision), Navigation (nav tiles) and Water (fragments) read, but depends on none of them. Each format's contract session (COOK-1, COOK-6, COOK-7) must ask which module owns the format definition.
- ARCH places the Runic Blender add-on under Engine tools but in no module. Where its source lives is decided when the first package needs more than the basic export (slice 44+ for the full add-on).
