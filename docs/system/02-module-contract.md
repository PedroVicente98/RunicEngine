# 02 — Module contract

## What counts as a module [J.1.1]

A module is a **meaningful CMake target or Go service/package** with its own
public boundary, tests, architectural contract, and implementation ownership.

- It must be large enough to justify `Public/`, `Source/`, `Tests/`,
  `ARCHITECTURE.md`, and `CMakeLists.txt` (or the Go equivalent).
- Small systems stay **subsystems** inside a larger module — for example
  CooldownSystem, DamageSystem, targeting, effects, and hit detection live inside
  `Combat`; DebugRenderer lives inside `Renderer`; physics queries live inside
  `Physics`.
- Extract a subsystem into its own module only when build isolation, independent
  deployment, or a stable reusable ABI justifies it.
- Exception: a deliberately narrow architectural seam (e.g. `NetworkAdapter`)
  may be small, because its value is isolation rather than code volume.

## Standard C++ module layout [J.3.1, J.1.2]

```text
ModuleName/
├── Public/                    # the compilation boundary: only what other modules may include
│   ├── ModuleName.hpp
│   ├── ModuleNameTypes.hpp
│   └── ...
├── Source/                    # internal implementation, third-party adapters/backends
│   └── <Backend>/             # e.g. Jolt/, Bgfx/, Flecs/
├── Tests/
├── ARCHITECTURE.md
└── CMakeLists.txt
```

Rules:

- **No nested include path.** The earlier `Public/Runic/ModuleName/` nesting is
  not used; the module directory already supplies the context.
- **Explicit file names.** Prefer `PhysicsWorld.hpp`, `PhysicsTypes.hpp` over
  `API.hpp`, `Types.hpp`. (derived) Because every module's `Public/` is a flat
  include root, public header names must be unique across all modules that a
  consumer can link — prefix them with the module or concept name.
- **Explicit namespaces.** `Physics/Public/PhysicsWorld.hpp` declares
  `namespace Runic::Physics`.
- **`Public/` is a boundary, not a dumping ground.** Third-party types stay under
  `Source/` whenever the dependency can be hidden. A consumer must not need Jolt,
  bgfx, BehaviorTree.CPP, Recast, or any backend header to use a Runic
  abstraction. Exposing a backend type publicly requires an explicitly approved
  exception recorded in the module's `ARCHITECTURE.md`.

## CMake enforces architecture [J.3.2]

```cmake
add_library(RunicPhysics)

target_include_directories(RunicPhysics
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/Public
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

target_link_libraries(RunicPhysics
    PUBLIC  Runic::Core
            Runic::World
    PRIVATE Jolt
)

add_library(Runic::Physics ALIAS RunicPhysics)
```

```cmake
# A consumer links only the Runic targets it is allowed to use.
target_link_libraries(RunicCombat
    PRIVATE Runic::Physics
            Runic::Simulation
            Runic::Gameplay
)
```

Conventions:

| Item | Convention |
| --- | --- |
| Real target name | `Runic<Module>` (e.g. `RunicPhysics`) |
| Alias used by consumers | `Runic::<Module>` (e.g. `Runic::Physics`) |
| Namespace | `Runic::<Module>` |
| `PUBLIC` links | Only Runic modules whose types appear in this module's `Public/` headers. |
| `PRIVATE` links | Backends (Jolt, bgfx, Flecs, RmlUi, …) and modules used only in `Source/`. |
| Allowed links | Exactly the "Allowed dependencies" of the module's contract; nothing else. |

A link that is not in the module's allowed list is an architecture change and
must be approved first ([03-dependency-rules.md](03-dependency-rules.md)).

## Go / Platform package layout [J.5]

Platform modules follow the Go-style equivalent:

```text
ModuleName/
├── api_or_pkg/     # public package surface other modules/repositories may import
├── internal/       # implementation; Go's internal/ rule enforces privacy
├── tests/
└── build/deploy metadata as appropriate
```

Where a Platform module also needs a C++ binding (e.g. the simulation linking
Realtime or Identity validation), the C++ part follows the C++ layout above.
The per-module language split is not yet decided ([OQ-9](11-open-questions.md#oq-9)).

## The per-module `ARCHITECTURE.md` contract [J.3.3]

Every module carries an `ARCHITECTURE.md` with these sections:

| Required section | What it must answer |
| --- | --- |
| Purpose | Why this module exists and what reusable capability it provides. |
| Public API | Headers/types/interfaces that other modules may depend on. |
| Owns | State, lifetime, threads, resources, schemas, or semantics owned here. |
| Does not own | Explicitly forbidden responsibilities. |
| Allowed dependencies | Targets/packages that may be included or linked. |
| Forbidden dependencies | Modules whose inclusion would invert the architecture. |
| Threading/tick model | Which threads/tick domains may call it and where mutations occur. |
| Data ownership | Who owns authoritative state, cached state, handles, and IDs. |
| Integration points | Commands/events/adapters used to communicate without coupling. |
| Acceptance tests | Minimum behavioral tests before dependent work may rely on it. |
| Future extensions | Permitted future work that must not leak into the current implementation. |

**Where the contract lives (convention for these repositories):**

1. Before a module directory exists, its contract lives in
   `docs/modules/<Module>.md` (or `docs/services/<Service>.md` in RunicGame).
2. When the module directory is created, copy that file to
   `<Module>/ARCHITECTURE.md`. From then on `ARCHITECTURE.md` is authoritative,
   and `docs/modules/<Module>.md` is reduced to status + a link to it.
3. Any change to Public API, Owns/Does-not-own, or dependencies updates the
   contract in the same change set as the code.

## Maturity stages [J.13.2, J.12]

| Stage | Meaning |
| --- | --- |
| **A — vertical proof** | Smallest real implementation that proves ownership boundaries and works in a production-shaped vertical slice. |
| **B — generalize** | Broaden API/data coverage once at least one real consumer proves the abstraction. |
| **C — harden** | Performance, scale, recovery, tooling, diagnostics, backwards compatibility, migration, adversarial tests. |

Do not wait for a dependency to reach Stage C before building its consumer. The
pattern is: Stage A proof → dependent Stage A → later Stage B generalization →
Stage C hardening. The architecture is validated by vertical integration, then
generalized and hardened.

## Checklist: creating a new module (derived)

1. Confirm the module is in the catalog (`docs/modules/README.md`). A new module
   not in the catalog is an owner decision.
2. Confirm its roadmap phase prerequisites are met
   ([08-implementation-roadmap.md](08-implementation-roadmap.md)).
3. Create the directory with `Public/`, `Source/`, `Tests/`, `ARCHITECTURE.md`
   (from the docs contract), and `CMakeLists.txt`.
4. Link only allowed dependencies; keep backends `PRIVATE`.
5. Implement the Stage A scope only: what the acceptance tests require.
6. Add tests; for headless modules, verify the headless build
   ([09-validation-scenarios.md](09-validation-scenarios.md#j146-headless-dependency-test)).
7. Update the module's status in `docs/modules/README.md` and the repository's
   `docs/current-state.md`.
