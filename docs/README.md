---
# obsidian-graph: generated tags and aliases
tags:
  - repo/engine
  - source/arch
  - type/index
---

# RunicEngine documentation

This folder documents RunicEngine: the shared system rules, this repository's architecture and state, and one contract seed per Engine module.

Everything here is generated from ARCH[^arch] (RunicEngine — Technical Architecture v3). When a file here disagrees with ARCH, ARCH wins; report the difference.

## Start here

1. [system/01-architecture-overview.md](system/01-architecture-overview.md) — the three repositories, ownership and invariants.
2. [system/02-module-contract.md](system/02-module-contract.md) and [system/03-dependency-rules.md](system/03-dependency-rules.md) — module shape and what may link what.
3. [architecture.md](architecture.md) — how RunicEngine is organized.
4. [current-state.md](current-state.md) — what exists today and the next packages.
5. [modules/README.md](modules/README.md) — the module catalog; open the module you will work on.
6. [system/12-work-packages.md](system/12-work-packages.md) and [system/10-ai-development-workflow.md](system/10-ai-development-workflow.md) — the packages and how one is built.

## Layout

| Path | Content | Scope |
| --- | --- | --- |
| `system/` | Cross-repository rules and plans | Mirrored in RunicEngine, RunicPlatform and RunicGame; edit all three together |
| `architecture.md` | Groups, folder layout, build bands, layers, render data, public-header rule, third-party placement | This repository |
| `current-state.md` | What exists, gaps against v3, consumers, next steps | This repository |
| `modules/` | One contract seed per Engine module (27) plus the catalog | This repository |
| `build/` | Contracts of the build packages: [BUILD-1](build/BUILD-1.md) (build skeleton, in review) | This repository; canonical for all three |

## System documents

| File | Content |
| --- | --- |
| [system/README.md](system/README.md) | Index and how the mirrored folder is maintained |
| [system/01-architecture-overview.md](system/01-architecture-overview.md) | Repositories, ownership, principles and targets |
| [system/02-module-contract.md](system/02-module-contract.md) | Module folder shape, targets, public headers, tests, maturity stages |
| [system/03-dependency-rules.md](system/03-dependency-rules.md) | Direction rules, module tables, forbidden links, repository wiring, CI checks |
| [system/04-events-and-messages.md](system/04-events-and-messages.md) | Application events, simulation facts, distributed messages |
| [system/05-distributed-simulation.md](system/05-distributed-simulation.md) | Authority, ghosts, transfer and handoff |
| [system/06-gameplayflow-quests-cutscenes.md](system/06-gameplayflow-quests-cutscenes.md) | Sequences, restrictions, quests and cutscenes |
| [system/07-service-backed-systems.md](system/07-service-backed-systems.md) | The service port, durable operations and services |
| [system/08-implementation-roadmap.md](system/08-implementation-roadmap.md) | Vertical-slice roadmap and the packages per slice |
| [system/09-validation-scenarios.md](system/09-validation-scenarios.md) | Acceptance scenarios across modules |
| [system/10-ai-development-workflow.md](system/10-ai-development-workflow.md) | How a Claude Code session builds one work package |
| [system/11-open-questions.md](system/11-open-questions.md) | Open questions and recorded decisions; never decide one silently |
| [system/12-work-packages.md](system/12-work-packages.md) | Every work package with Done when, Needs and Slice |
| [system/glossary.md](system/glossary.md) | Terms |

## Modules

[Core](modules/Core.md), [Application](modules/Application.md), [Events](modules/Events.md), [Content](modules/Content.md), [Assets](modules/Assets.md), [ECS](modules/ECS.md), [World](modules/World.md), [Physics](modules/Physics.md), [Simulation](modules/Simulation.md), [Gameplay](modules/Gameplay.md), [Combat](modules/Combat.md), [GameplayFlow](modules/GameplayFlow.md), [Quest](modules/Quest.md), [Perception](modules/Perception.md), [Navigation](modules/Navigation.md), [AI](modules/AI.md), [Environment](modules/Environment.md), [Water](modules/Water.md), [Network](modules/Network.md), [DistributedSimulation](modules/DistributedSimulation.md), [Client](modules/Client.md), [Renderer](modules/Renderer.md), [Animation](modules/Animation.md), [UI](modules/UI.md), [Audio](modules/Audio.md), [Editor](modules/Editor.md), [Cooker](modules/Cooker.md).

## Rules for these docs

- Write in plain English with short sentences and tables for anything with rows. Cite ARCH[^arch] sections by name.
- No game rules, names, numbers or content anywhere in this folder; this repository is public.
- Change a module's contract, its catalog row and `current-state.md` in the same pull request as its code.

<!-- obsidian-graph:start -->

---

## Connections

*Generated for the Obsidian graph. The numbered markers in the text jump to the footnotes below, which link the notes they refer to.*

- **Start here:** [[../docs/architecture|Engine architecture]] · [[../docs/current-state|Engine current state]] · [[../docs/modules/README|Engine module catalog]] · [[../docs/build/BUILD-1|Engine BUILD-1 contract]] · [[../docs/system/01-architecture-overview|01 — Architecture overview]] · [[../docs/system/02-module-contract|02 — Module contract]] · [[../docs/system/03-dependency-rules|03 — Dependency rules]] · [[../docs/system/04-events-and-messages|04 — Events and messages]] · [[../docs/system/05-distributed-simulation|05 — Distributed simulation]] · [[../docs/system/06-gameplayflow-quests-cutscenes|06 — Gameplayflow quests cutscenes]] · [[../docs/system/07-service-backed-systems|07 — Service backed systems]] · [[../docs/system/08-implementation-roadmap|08 — Implementation roadmap]] · [[../docs/system/09-validation-scenarios|09 — Validation scenarios]] · [[../docs/system/10-ai-development-workflow|10 — Ai development workflow]] · [[../docs/system/11-open-questions|11 — Open questions]] · [[../docs/system/12-work-packages|12 — Work packages]] · [[../docs/system/README|System README]] · [[../docs/system/glossary|Glossary]]

[^arch]: #source/arch — ARCH, "RunicEngine — Technical Architecture v3": the architecture source of truth (a project document, not a note in this vault)
