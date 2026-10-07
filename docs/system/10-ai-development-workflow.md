# 10 — AI development workflow (Claude Code)

These repositories are developed **one module at a time** by an AI coding agent
under owner supervision. The agent may read the whole project, but writes only
inside the selected module. [J.13.1]

## Why the boundary must be enforceable [J.13.1]

The coding model must not "help" by implementing neighboring future systems or by
moving responsibilities across module boundaries. When another module needs a
change, the model **reports the required public-interface change** instead of
making it.

## Standard procedure for a module task

1. **Orient.** Read, in order: the repository's `CLAUDE.md`, `docs/README.md`,
   `docs/system/01`–`03`, `docs/current-state.md`, and the target module contract
   (`docs/modules/<Module>.md` or `<Module>/ARCHITECTURE.md` if it exists).
2. **Check prerequisites.** Confirm the module's roadmap phase
   ([08](08-implementation-roadmap.md)) and that every allowed dependency it needs
   exists at least at Stage A. If not, stop and report what is missing.
3. **Check open questions.** If the task touches an item in
   [11-open-questions.md](11-open-questions.md), ask the owner before deciding it.
   Do not silently resolve an open question in code.
4. **Fix the write scope** (template below). Everything else is read-only.
5. **Implement Stage A only** — the smallest real implementation that satisfies
   the acceptance tests. No speculative features, no placeholder implementations
   in other modules.
6. **Test.** Module tests, plus the headless build for headless modules
   ([09 §J.14.6](09-validation-scenarios.md#j146-headless-dependency-test)).
7. **Update docs in the same change:** create/refresh `<Module>/ARCHITECTURE.md`,
   update the status column in `docs/modules/README.md`, and update
   `docs/current-state.md`.
8. **Report** any required interface change in another module or repository as a
   proposal (file, type, signature, reason) instead of implementing it.

## Task template [J.13.1]

Use this when asking Claude Code to implement a module (example: Physics).

```text
TASK:
  Implement Runic::Physics (Stage A).

WRITE SCOPE:
  Engine/Physics/**
  the Physics target in CMake only
  docs/modules/Physics.md, docs/modules/README.md, docs/current-state.md

READ-ONLY DEPENDENCIES:
  Runic::Core
  Runic::World

FORBIDDEN:
  Simulation, Combat, Renderer, Networking, Editor,
  RunicGame, RunicPlatform services

ARCHITECTURAL CONTRACT:
  - Physics answers geometric/physical questions.
  - Physics does not implement gameplay rules.
  - Jolt types do not escape the backend unless explicitly approved.
  - Headless compatibility is mandatory.

PUBLIC API:
  [approved API from ARCHITECTURE.md / docs/modules/Physics.md]

ACCEPTANCE TESTS:
  [approved tests from the module contract]

DO NOT:
  - invent missing game systems;
  - add placeholder implementations to other modules;
  - modify dependency modules to make this easier;
  - move responsibilities across module boundaries;
  - implement future functionality not required by acceptance tests.

If another module would need a change, report the required interface
change instead of implementing it.
```

The source template lists tests as `Tests/Physics/**`; this documentation uses
module-local `Engine/Physics/Tests/` per J.3.1 until
[OQ-7](11-open-questions.md#oq-7) is settled.

## Rules that always apply

- Never add a dependency that is not in the module's "Allowed dependencies".
- Never let a backend type (Jolt, bgfx, Flecs, RmlUi, BehaviorTree.CPP,
  Recast/Detour) appear in a `Public/` header without an approved exception.
- Never put RunicGame rules, numbers, content, or names into RunicEngine or
  RunicPlatform — including tests and examples. Use neutral names
  (`engine:test_ability`), never game content.
- Never block a simulation tick on I/O; never mutate Flecs from an I/O callback.
- Never let a client decide an authoritative outcome.
- Keep the server build headless.
- Mark anything you infer but cannot confirm from these docs as **(proposal)**
  in the docs you write, and surface it to the owner.

## Keeping the documentation healthy

| Change | Update |
| --- | --- |
| A module is created | `<Module>/ARCHITECTURE.md` (from the docs contract), status in `docs/modules/README.md`, `docs/current-state.md` |
| A module's public API, ownership, or dependencies change | That module's contract; `docs/system/03-dependency-rules.md` if dependencies changed (in all three repositories) |
| An open question is decided | Move it to "Decided" in `docs/system/11-open-questions.md` (all three repositories) and update affected contracts |
| A phase completes | `docs/current-state.md`; "Where we are" in `docs/system/08-implementation-roadmap.md` (all three repositories) |
| New architecture source material arrives | Fold it into the relevant docs; record provenance in `docs/system/README.md` |

`docs/system/` is mirrored. A quick consistency check from the parent folder:

```sh
diff -r RunicEngine/docs/system RunicPlatform/docs/system && \
diff -r RunicEngine/docs/system RunicGame/docs/system && echo "docs/system in sync"
```
