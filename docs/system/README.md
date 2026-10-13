---
# obsidian-graph: generated tags and aliases
tags:
  - source/arch
  - type/system
  - mirror/system
---

# System documentation (shared by RunicEngine, RunicPlatform and RunicGame)

This folder holds the architecture rules that all three repositories share; read it before you change any module.

> **Mirrored folder.** `docs/system/` is byte-for-byte identical in RunicEngine, RunicPlatform and
> RunicGame. Two of those repositories are public. Nothing in this folder may contain RunicGame rules,
> names, numbers or content (see [02 § No game content in public repositories](02-module-contract.md#no-game-content-in-public-repositories)).
> Repository-specific material belongs in that repository's own `docs/` (for example `docs/current-state.md`
> and `docs/modules/`), never here.

## Source and authority

| Item | Value |
| --- | --- |
| Source document | *RunicEngine — Technical Architecture v3[^arch]* ("ARCH"), doc revision 503, exported 2026-10-13 |
| Status of ARCH | The single source of truth for architecture, modules, work packages and the roadmap |
| Status of this folder | Generated from ARCH. It restates ARCH's rules for Claude Code sessions and humans; it adds no decisions of its own |
| On conflict | **ARCH wins.** If this folder, a module's `ARCHITECTURE.md` or any code disagrees with ARCH, ARCH is right and the other text is a bug to fix |
| Game design | Not in this folder. It lives only in RunicGame, outside `docs/system/` |

Citations use ARCH section names, for example "ARCH § Networking: replication, interest and handoff".

### What is fixed and what is not

- **Rules and decisions** (dependency direction, headless server, nothing blocks a tick, ownership of each seam) are
  architectural commitments. Changing one is an owner decision, recorded in ARCH first.
- **Numbers** (rates, sizes, budgets) are planning targets unless ARCH's decisions log marks them as decided.
  Final values come from profiling and playtests (ARCH § About this document).
- **Code** in these docs is a contract sketch. Names are illustrative, not a frozen API.
- **Open items** are listed in [11-open-questions.md](11-open-questions.md). Never resolve one silently in code.

## Reading order

| Step | Read | When |
| --- | --- | --- |
| 1 | [01 Architecture overview](01-architecture-overview.md) | Always, first |
| 2 | [02 Module contract](02-module-contract.md) | Before creating or changing any module |
| 3 | [03 Dependency rules](03-dependency-rules.md) | Before adding any include, link, import or package dependency |
| 4 | [10 AI development workflow](10-ai-development-workflow.md) | Before starting any work package with Claude Code |
| 5 | [12 Work packages](12-work-packages.md) and [08 Implementation roadmap](08-implementation-roadmap.md) | To find the package you are building, its Needs and its slice |
| 6 | The topic file for your package (04 to 07, 09) | When your package touches that topic |
| 7 | [11 Open questions](11-open-questions.md) | Before relying on anything that might be undecided |
| — | [Glossary](glossary.md) | Whenever a term is unclear |

## File index

| File | One line |
| --- | --- |
| [README.md](README.md) | This file: provenance, reading order, index, how to change the folder |
| [01-architecture-overview.md](01-architecture-overview.md) | Ownership, repositories, processes, layered build view, four axes, event families, principles, targets |
| [02-module-contract.md](02-module-contract.md) | What a module is, folder layout, naming, Public/ boundary, ARCHITECTURE.md template, maturity stages |
| [03-dependency-rules.md](03-dependency-rules.md) | Direction rules, module tables for Engine, Platform and Game, the Engine interface targets the Platform links, forbidden links, repository wiring, CPU baseline, CI checks |
| [04-events-and-messages.md](04-events-and-messages.md) | Event families, commands and facts, stable IDs, buffers and barriers, the service port, delivery classes |
| [05-distributed-simulation.md](05-distributed-simulation.md) | World space (64-bit coordinates, sparse 3D cells, volumes, activation, worlds and transitions), partition versus ownership, authority, ghosts, transfer, links by locality, interest, bandwidth, partitions |
| [06-gameplayflow-quests-cutscenes.md](06-gameplayflow-quests-cutscenes.md) | Scripted sequences, restrictions, termination, handoff policies, cutscenes, quest runtime and durable bridge |
| [07-service-backed-systems.md](07-service-backed-systems.md) | Platform and Game services, the durable-fact path, idempotency, placement, monitoring, local development |
| [08-implementation-roadmap.md](08-implementation-roadmap.md) | Every roadmap slice with its runnable result and work packages |
| [09-validation-scenarios.md](09-validation-scenarios.md) | Acceptance scenarios as runnable checklists |
| [10-ai-development-workflow.md](10-ai-development-workflow.md) | How one work package is done with Claude Code |
| [11-open-questions.md](11-open-questions.md) | Open items and the decisions already taken |
| [12-work-packages.md](12-work-packages.md) | Every Engine and Platform work package with Done when, Needs and Slice |
| [glossary.md](glossary.md) | Terms used across the folder |

## How to change this folder

1. **Change ARCH[^arch] first.** A rule, module, package, dependency or slice changes in ARCH, then this folder is
   regenerated or edited to match. Never change an architecture rule only here.
2. **Edit all three copies together.** Apply the identical change to `docs/system/` in RunicEngine,
   RunicPlatform and RunicGame in the same change set. A quick check from the parent folder:

   ```sh
   diff -r RunicEngine/docs/system RunicPlatform/docs/system && \
   diff -r RunicEngine/docs/system RunicGame/docs/system && echo "docs/system in sync"
   ```

3. **Keep it public-safe.** Before committing, check that no RunicGame rule, name, number or content entered
   the folder. Game work-package IDs may appear as IDs only.
4. **Decisions move in 11.** When an open question is decided, move it to the Decided section of
   [11-open-questions.md](11-open-questions.md) with its date and outcome, and update the affected module contracts.

<!-- obsidian-graph:start -->

---

## Connections

*Generated for the Obsidian graph. The numbered markers in the text jump to the footnotes below, which link the notes they refer to.*

- **See also:** [[../system/glossary|Glossary]]

[^arch]: #source/arch — ARCH, "RunicEngine — Technical Architecture v3": the architecture source of truth (a project document, not a note in this vault)
