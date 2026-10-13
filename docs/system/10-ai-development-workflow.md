---
# obsidian-graph: generated tags and aliases
tags:
  - source/arch
  - type/system
  - mirror/system
---

# 10 — AI development workflow

This file says how one work package is built with Claude Code: what a session may touch, the contract-first cycle, the task template, and what to do when something outside the package is needed.

Source: ARCH[^arch] § Modules, work packages and the implementation contract (Why the code is segmented this way, Work
packages, Building with Claude Code, Maturity stages, Parallel work, Automated checks).

## The unit of work

**One work package = one Claude Code session = one pull request = one module.**

A package is defined by five things ([12-work-packages.md](12-work-packages.md)):

| Field | Meaning |
| --- | --- |
| Write scope | One module folder, its CMake target and its docs. Nothing else is edited |
| Interface change | The public headers it adds or changes, approved before the implementation |
| Done when | The acceptance behavior, written as automated tests |
| Needs | The packages that must be merged first. A package starts only when all of them are merged |
| Slice | The roadmap slice that first needs it. Packages are Stage A unless the table says otherwise |

Why it is cut this way:

- A session builds well what it can hold in mind at once: the module's `ARCHITECTURE.md`, the public headers of the
  modules it depends on, and its own code.
- The design decision stays with the owner: the public interface and the acceptance tests are approved before the
  implementation, so the implementing session solves a closed problem.
- One package writes inside one module. When it needs something elsewhere, it reports the interface change instead of
  making it, so changes never ripple silently across the codebase.
- Reviewable size: as a rule of thumb a package changes at most about 1,500 to 2,500 lines, which fits a careful
  review of about 30 minutes.

BUILD-1[^doc-build-build-1] (slice 0) is the one package ARCH[^arch] defines across all three repositories: it creates the build skeleton, CI,
presets, checks and submodule pins, not module code.

## Before you start

1. Read the repository's `CLAUDE.md`, `docs/README.md`, this folder's 01 to 03 and this file, the repository's
   `docs/current-state.md`, and the module's contract (`<Module>/ARCHITECTURE.md`, or `docs/modules/<Module>.md` before
   the folder exists).
2. Find the package in [12-work-packages.md](12-work-packages.md). Confirm every package in its Needs is merged. If
   not, stop and report what is missing.
3. Check [11-open-questions.md](11-open-questions.md). If the package touches an open item, ask the owner before
   deciding it. **Never resolve an open question silently in code.**
4. Fix the write scope. Everything outside it is read-only.

## The cycle

Every package goes through the same short cycle. It keeps design decisions with the owner and gives each coding
session a closed, testable problem.

| Step | Who | What happens |
| --- | --- | --- |
| 1. Contract session | Claude Code | Writes or updates the module's `ARCHITECTURE.md`, the public headers the package adds and the acceptance tests, which fail at first. **Nothing in `Source/` yet** |
| 2. Contract review | Owner | Approves the interface and the tests or asks for changes. This is the cheapest moment to change a design |
| 3. Implementation session | Claude Code, fresh session | Implements `Source/` until the tests pass, inside the write scope only. When it needs something from another module, it stops and reports the interface change |
| 4. Checks | CI | Module tests, the headless server build and the dependency checks |
| 5. Code review and merge | Owner | The pull request carries the code, the tests, the updated `ARCHITECTURE.md` and the package status |

Small packages, with a few hundred lines and no new public types, may combine steps 1 and 3 in one session.

## Stage A only

- Build the smallest real implementation that makes the approved tests pass inside the slice.
- No speculative features, no "while I am here" generalization, no placeholders in other modules.
- Generalization (Stage B) and hardening (Stage C) are later packages, started when a slice needs them.

| Stage | Meaning | When |
| --- | --- | --- |
| A — vertical proof | The smallest real implementation that proves the module's boundaries inside a roadmap slice | Every package starts here |
| B — generalize | Broader interface and data coverage once a second real consumer exists | When a later slice needs more |
| C — harden | Performance, scale, recovery, tooling, compatibility and adversarial tests | Battle scale, operations and pre-launch slices |

## Task template

Use this shape for every implementation session. Example from ARCH[^arch], which is already neutral:

```text
TASK
  Implement CMB-3 (Stage A): statuses and effects, cooldowns, resource costs.

WRITE SCOPE
  Engine/GameplayAPI/Combat/**   (Public, Source, Tests, ARCHITECTURE.md)
  the Runic::Combat target in CMake

READ-ONLY
  Public headers of Runic::Core, Events, Simulation, Gameplay
  Docs: Engine gameplay APIs, Simulation core, Modules and work packages

APPROVED INTERFACE
  [public headers from the contract session]

DONE WHEN
  [acceptance tests from the package table and ARCHITECTURE.md]

RULES
  - Edit nothing outside the write scope; report needed interface changes.
  - No library types in Public/; the server build stays headless.
  - No game rules, names or numbers; tests use engine: test IDs.
  - Never block a tick on I/O; never mutate Flecs from an I/O callback.
  - Build only what the tests need; no placeholders in other modules.
```

For a contract session, replace APPROVED INTERFACE and DONE WHEN with "propose the public headers and write the failing
acceptance tests"; the session ends at contract review.

## Rules that always apply

- Edit nothing outside the write scope.
- Link only what the package uses, within the module's row in [03](03-dependency-rules.md); third-party libraries are
  `PRIVATE` except the approved exceptions.
- No library types in `Public/`, except the three approved exceptions
  ([02 § Public/ is the compile boundary](02-module-contract.md#public-is-the-compile-boundary)).
- Keep the server build headless.
- Never block a tick on I/O; never mutate Flecs from an I/O callback; never let a local Flecs ID leave the process.
- Never let a client decide an authoritative outcome.
- No Game rules, names or numbers in RunicEngine, RunicPlatform or `docs/system/`, tests included. Use neutral IDs such
  as `engine:test_ability`.
- Never resolve an item in [11-open-questions.md](11-open-questions.md) silently.

## Reporting a change in another module

When the package needs something outside its write scope (a new public type, a new function, a new dependency, a
change in another repository), **stop and report it**. Do not make the change, and do not work around it with a
placeholder.

Report it in the pull request description or to the owner, with:

| Field | Content |
| --- | --- |
| Module | The module (and repository) that would change |
| Change | The header, type or function, with the proposed signature |
| Reason | Which test or Done when needs it |
| Package | The existing package that should carry it, or "new package needed" |
| Rule check | Whether it adds a dependency, and whether that dependency is allowed by [03](03-dependency-rules.md) |

A change that adds a dependency outside a module's allowed list, a new module or a new package is an owner decision
recorded in ARCH[^arch] first.

## Parallel work

- Packages whose Needs are all merged can run at the same time, each in its own Claude Code session and git worktree.
  Examples: RND-1[^engine-renderer] and RND-2 alongside SIM-1[^engine-simulation]; PCT-1[^platform-contracts] alongside NET-1[^engine-network]; EDT-1[^engine-editor] alongside PHYS-1[^engine-physics].
- Two or three sessions at once is the practical limit, because every contract and pull request still needs the
  owner's review.
- **Two packages that write the same module never run at the same time.**

## Automated checks

| Check | Catches |
| --- | --- |
| Headless build of GameServer | Graphics, UI, audio or editor code reaching the server |
| Dependency check per module | Links outside the allowed list; includes of another module's Source/ |
| Public-header scan | Library types in Public/ |
| Module tests | The Done when behavior of every merged package |
| Docs check | A code change without its ARCHITECTURE.md or package-status update |

## Keeping documentation in step

| Change | Update in the same pull request |
| --- | --- |
| A package is merged | The module's `ARCHITECTURE.md` (Work packages: status), the repository's module status and `docs/current-state.md` |
| A module's public API, ownership or dependencies change | That module's `ARCHITECTURE.md`; if a dependency changes, ARCH[^arch] first, then [03](03-dependency-rules.md) in all three repositories |
| An open question is decided | Move it to Decided in [11](11-open-questions.md) in all three repositories and update the affected contracts |

`docs/system/` is mirrored byte-for-byte in all three repositories; edit all copies together
([README § How to change this folder](README.md#how-to-change-this-folder)).

<!-- obsidian-graph:start -->

---

## Connections

*Generated for the Obsidian graph. The numbered markers in the text jump to the footnotes below, which link the notes they refer to.*

- **See also:** [[../system/README|System README]] · [[../system/glossary|Glossary]]

[^arch]: #source/arch — ARCH, "RunicEngine — Technical Architecture v3": the architecture source of truth (a project document, not a note in this vault)
[^doc-build-build-1]: [[../build/BUILD-1|BUILD-1 contract]] — this repository
[^engine-renderer]: [[../../../RunicEngine/docs/modules/Renderer|Renderer]] — Engine module · Presentation
[^engine-simulation]: [[../../../RunicEngine/docs/modules/Simulation|Simulation]] — Engine module · Simulation core
[^platform-contracts]: [[../../../RunicPlatform/docs/modules/Contracts|Contracts]] — Platform module · Protobuf
[^engine-network]: [[../../../RunicEngine/docs/modules/Network|Network]] — Engine module · Networking
[^engine-editor]: [[../../../RunicEngine/docs/modules/Editor|Editor]] — Engine module · Tools
[^engine-physics]: [[../../../RunicEngine/docs/modules/Physics|Physics]] — Engine module · Simulation core
