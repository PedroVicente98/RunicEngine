# 02 — Module contract

This file defines what a module is, how it is laid out and named, what may cross its boundary, what its `ARCHITECTURE.md` must contain, and how it matures.

Source: ARCH § Modules, work packages and the implementation contract (Sizes of work, Why the code is segmented this
way, Module contract, Maturity stages).

## Sizes of work

The code is organized at two working sizes. A **module** keeps the architecture honest; a **work package** keeps
each piece of work small enough to be built well.

| Size | What it is | Example | Who uses it |
| --- | --- | --- | --- |
| Group | A folder plus an umbrella build target that links related modules under one name; no code of its own | `Runic::GameplayAPI` = Gameplay, Combat, GameplayFlow, Quest | The Game links one name; the documents categorize by it |
| Module | A CMake target with its own folder, public headers, tests and `ARCHITECTURE.md`; the unit of ownership and dependency | `Runic::Combat` | Other modules link it and include only its public headers |
| Subsystem | A folder inside a module's `Source/`; not linkable on its own | `Combat/Cooldowns` | Only its own module |
| Work package | One Claude Code session and one pull request: a subsystem, or part of one, at a stated maturity stage, with its tests and docs | CMB-3: statuses, cooldowns and resource costs | The owner reviews and merges it; the roadmap lists it |

Rules:

- **Modules stay coarse on purpose.** Many tiny libraries add build, link and dependency overhead without making
  the code better. Big modules are split into packages, not into more libraries.
- **A subsystem becomes its own module only** when build isolation or reuse proves it necessary. That is an owner
  decision recorded in ARCH; never create a module that is not in the catalogs in
  [03-dependency-rules.md](03-dependency-rules.md).
- Work packages are defined in [12-work-packages.md](12-work-packages.md); how one is built is in
  [10-ai-development-workflow.md](10-ai-development-workflow.md).

## Folder layout

Every C++ module in the three repositories has the same shape. A module inside a group nests under the group folder:
`Engine/<Group>/<Module>/`.

```text
Engine/GameplayAPI/Combat/
├── Public/                 # the only headers other modules may include
│   ├── CombatModule.hpp
│   ├── Ability.hpp
│   └── CombatEvents.hpp
├── Source/                 # implementation; each subsystem is a folder
│   ├── Abilities/
│   ├── Cooldowns/
│   └── Damage/
├── Tests/
├── ARCHITECTURE.md         # the module's contract
└── CMakeLists.txt
```

Go packages follow the same idea with Go's own layout: one directory per package, `internal/` for private code,
tests beside the code.

## CMake: one call per module

The `runic_add_module` helper (delivered by BUILD-1) creates the target and its alias, wires `Public/`, `Source/` and
`Tests/`, sets warnings, registers tests and adds the headless check.

```cmake
# Illustrative: one call per module.
# Combat at its final shape: DistributedSimulation is linked from CMB-8
# (foreign combat) on, because a package links only what it uses.
runic_add_module(Combat
    GROUP        GameplayAPI
    PUBLIC_DEPS  Runic::Gameplay Runic::Physics
    PRIVATE_DEPS Runic::DistributedSimulation
    HEADLESS)

# A group is an umbrella that only links its modules.
add_library(RunicGameplayAPI INTERFACE)
target_link_libraries(RunicGameplayAPI INTERFACE
    Runic::Gameplay Runic::Combat Runic::GameplayFlow Runic::Quest)
add_library(Runic::GameplayAPI ALIAS RunicGameplayAPI)
```

## Groups and umbrella targets

- A group is a folder plus an `INTERFACE` target that only links its modules. It has no code of its own.
- Each module inside a group keeps its own boundary, its own tests and its own `ARCHITECTURE.md`.
- `Runic::GameplayAPI` links Gameplay, Combat, GameplayFlow and Quest so the Game links one name.
- Engine groups: Foundation, Simulation core, GameplayAPI, World systems, Networking, Presentation, Tools
  (membership in [03 § Engine modules](03-dependency-rules.md#engine-modules)). Their folders are `Foundation`,
  `SimulationCore`, `GameplayAPI`, `WorldSystems`, `Networking`, `Presentation` and `Tools`; every module lives at
  `Engine/<Group>/<Module>/`.
- The transitional bundles `Runic::Runtime` and `Runic::Presentation` stay until RunicGame links per-module targets
  (decided, see [11 § OQ-8](11-open-questions.md#oq-8)). Do not add new code to them.

## Naming

| Item | Engine | Platform | Game |
| --- | --- | --- | --- |
| Target alias | `Runic::<Module>` | `RunicPlatform::<Module>` | `RunicGame::<Module>` |
| Namespace | Matches the module, for example `Runic::Physics` | Matches the module | Matches the module |
| Group target | `INTERFACE` target, for example `Runic::GameplayAPI` | — | — |
| Interface target for the Platform | Header-only, Core types only: `Runic::NetworkTransport`, `Runic::SimulationPort` | — (implements them) | — |

- **One target per module.** A module may also export **one header-only interface target** for an interface the
  Platform implements: `Runic::NetworkTransport` (from Network, NET-1) and `Runic::SimulationPort` (from Simulation,
  SIM-5). Those headers include only Core types. No other module exports such a target unless ARCH adds it.
- **Explicit file names.** Public files say what they hold (`PhysicsWorld.hpp`, never `Types.hpp`).
- RenderData is a separate folder and namespace inside Core: `Core/Public/RenderData/`, `Runic::RenderData`.

## Public/ is the compile boundary

- Only `Public/` headers cross a module boundary. Never include another module's `Source/`.
- **Library types never appear in a public header.** Flecs, Jolt, bgfx, RmlUi, ImGui, BehaviorTree.CPP, Detour and
  GameNetworkingSockets stay in `Source/` behind Runic types.
- An exception must be approved and recorded in the module's `ARCHITECTURE.md`. **Exactly three are approved:**
  1. GLM math types in Core's public headers (the Engine's math vocabulary).
  2. The Flecs world handle, for modules that register systems into the simulation (for example `AIModule`). ECS
     re-exports Flecs only to the Engine modules that register systems.
  3. BehaviorTree.CPP node base types, for Game custom AI nodes.
- Jolt, bgfx, RmlUi, Detour and GameNetworkingSockets always stay hidden. There is no exception for them.
- CI scans `Public/` for library types ([03 § How CI enforces the rules](03-dependency-rules.md#how-ci-enforces-the-rules)).

## PUBLIC and PRIVATE dependencies

- **Allowed dependencies are a ceiling.** A package links only what it uses, even if the module table allows more.
  Example: Combat links DistributedSimulation only from CMB-8 on.
- A module dependency is `PUBLIC` only when its types appear in this module's public headers. Otherwise it is
  `PRIVATE`.
- Third-party libraries are `PRIVATE`, except the approved exceptions: GLM is a `PUBLIC` dependency of Core.
- A link that is not in the module's allowed list is an architecture change. Stop and report it
  ([10 § Reporting a change in another module](10-ai-development-workflow.md#reporting-a-change-in-another-module)).

## Headless modules

- Headless modules never link GLFW, bgfx, RmlUi, ImGui, an audio backend or editor code.
- The Window class in Application is compiled only in client and tools builds; Application is headless without it.
- CI builds the server target alone and fails if any of those libraries appears.
- Which modules are headless is the `Headless` column of [03 § Engine modules](03-dependency-rules.md#engine-modules).

## Tests live inside the module

- Tests are in the module's `Tests/` folder and register with CTest.
- One C++ test framework is used in all three repositories. It is chosen in slice 0 by the BUILD-1 session, which
  asks the owner. Go uses its standard `testing` package.
- A package's **Done when** is written as automated tests in its contract session, before the implementation.

## No game content in public repositories

- RunicEngine and RunicPlatform are public. Game rules, names and numbers never appear in Engine or Platform code,
  **tests included**, nor in `docs/system/`.
- Tests and examples use neutral IDs such as `engine:test_ability`. `game:` IDs appear only as format examples
  (`game:<something>`).
- Never copy a RunicGame class, item, status, place or value into a public repository, even as a test fixture.

## The module's ARCHITECTURE.md

Each module's `ARCHITECTURE.md` is written from its catalog row in [03](03-dependency-rules.md) and the ARCH
sections that concern it. It changes **in the same pull request** as the module's code. Until the module folder
exists, the same contract lives in the owning repository's `docs/modules/<Module>.md`.

Start the file with a header copied from the catalog row:

```text
Target:   Runic::<Module>        Group: <Group>
Headless: yes | no | never shipped | tool
Packages: <PREFIX>-1 to <PREFIX>-n
```

Then the sections ARCH requires, in this order:

| Section | What it answers |
| --- | --- |
| Purpose | Why the module exists and what it provides |
| Public API | The headers and types other modules may use (a sketch until a contract session approves them) |
| Owns | State, lifetimes, threads, resources and meanings owned here |
| Does not own | Responsibilities that belong elsewhere |
| Allowed dependencies | Modules and libraries it may link (the catalog row), and any approved Public/ exception |
| Forbidden dependencies | Links that would invert the architecture |
| Threading and tick model | Which threads and tick domains call it, and where mutation happens |
| Data ownership | Who owns authoritative state, caches, handles and IDs |
| Integration points | Commands, events, hooks and registries used to talk to other modules |
| Work packages | Its packages, each with its acceptance tests (Done when) and status |
| Future extensions | Work allowed later that must not leak into current packages |

If an item in [11-open-questions.md](11-open-questions.md) affects the module, link it from the section it affects.

## Maturity stages

| Stage | Meaning | When |
| --- | --- | --- |
| A — vertical proof | The smallest real implementation that proves the module's boundaries inside a roadmap slice | Every package starts here |
| B — generalize | Broader interface and data coverage once a second real consumer exists | When a later slice needs more |
| C — harden | Performance, scale, recovery, tooling, compatibility and adversarial tests | Battle scale, operations and pre-launch slices |

- Packages are Stage A unless the package table says otherwise.
- A consumer never waits for its dependency to reach Stage C. Vertical slices validate the architecture first; then
  it is generalized and hardened.
- Build only what the Stage A tests need. No speculative features, no placeholders in other modules.

## Checklist: creating a module

1. Confirm the module is in the catalog in [03](03-dependency-rules.md). A module that is not there is an owner
   decision, not an implementation detail.
2. Confirm the package's Needs are merged ([12-work-packages.md](12-work-packages.md)).
3. Create `Public/`, `Source/`, `Tests/`, `ARCHITECTURE.md` and `CMakeLists.txt` with one `runic_add_module` call.
4. Link only what the package uses, within the allowed list; third-party libraries `PRIVATE`.
5. Keep library types out of `Public/` unless the module uses one of the three approved exceptions.
6. Write the tests; for headless modules make sure the server build stays headless.
7. Update the module's `ARCHITECTURE.md`, the package status and the repository's `docs/current-state.md` in the
   same pull request.
