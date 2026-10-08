# RunicEngine documentation

RunicEngine is the public (MIT) C++20 library of **reusable real-time and game
mechanisms** for a server-authoritative MMORPG: ECS integration, world model,
physics, fixed-step simulation, gameplay events, combat/AI/quest/gameplay-flow
frameworks, water, rendering, client runtime, distributed-simulation semantics,
and tooling. It contains no RunicGame rules or content and no networking,
database, or orchestration implementation.

> Architecture version **v0** — imported from earlier design work and being
> refined here. See [system/README.md](system/README.md) for provenance.

## Start here

1. [system/01-architecture-overview.md](system/01-architecture-overview.md) — invariants, three-way rule, ownership.
2. [architecture.md](architecture.md) — how RunicEngine is organized.
3. [current-state.md](current-state.md) — what exists today and the gap to the target.
4. [modules/README.md](modules/README.md) — module catalog; open the module you will work on.
5. [system/10-ai-development-workflow.md](system/10-ai-development-workflow.md) — how a module task is executed.

## Layout of this folder

| Path | Content | Scope |
| --- | --- | --- |
| `system/` | Cross-repository architecture: invariants, module contract, dependency rules, events, distributed simulation, GameplayFlow/quests, services, roadmap, validation, workflow, open questions, glossary | **Mirrored** in RunicPlatform and RunicGame — edit all three |
| `architecture.md` | RunicEngine structure, targets, third-party placement, executables | this repository |
| `current-state.md` | Bootstrap inventory, gaps, migration checklist | this repository |
| `modules/` | One contract per Engine module (25) | this repository |
