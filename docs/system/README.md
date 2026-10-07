# System documentation (shared across the three repositories)

> **Mirrored folder.** `docs/system/` is identical in RunicEngine, RunicPlatform,
> and RunicGame. When you change a file here, apply the same change to the other
> two repositories in the same change set. Repository-specific material belongs
> in that repository's `docs/architecture.md`, `docs/current-state.md`,
> `docs/modules/`, or `docs/services/`, never here.

## Provenance and authority

| Item | Value |
| --- | --- |
| Architecture version | **v0 — initial import.** Migrated from earlier work done with another AI agent (GPT); it will be refined in these repositories. |
| Primary source | *RunicEngine / RunicPlatform / RunicGame — Technical Architecture Appendix J: Module Boundaries, Distributed Simulation, Gameplay Flow, and Service Integration* (6 October 2026). |
| Source status | `OWNER REFINEMENT` of *Technical Architecture and Implementation Reference v2.1*. The v2.1 document remains authoritative except where Appendix J supersedes repository ownership. |
| Not yet imported | v2.1 sections A–I and the subsystem references (World Authoring/Cooking, Water, AI, Jolt Physics/Multi-Rate Tick, Combat System Context, Platform Fabric/Networking). See [11-open-questions.md](11-open-questions.md#oq-12). |

Section references such as `[J.8.2]` point to Appendix J so that decisions can be
traced back to the source document.

### What is locked and what is not

- **Invariants** listed in [01-architecture-overview.md](01-architecture-overview.md#non-negotiable-invariants)
  are architectural commitments. Changing one is an owner decision, not an
  implementation detail.
- **API sketches, file names, and class names are conceptual** unless a document
  explicitly marks them as locked. They define responsibility and dependency
  direction, not a frozen ABI, wire schema, or final spelling. [J intro]
- Text marked **(derived)** or **(proposal)** was inferred while converting the
  source into these docs. Treat it as a starting point that needs owner review.

## Reading order

| # | Document | Read it when |
| --- | --- | --- |
| 01 | [Architecture overview](01-architecture-overview.md) | Always, first. Invariants, the three-way rule, ownership, project map. |
| 02 | [Module contract](02-module-contract.md) | Before creating or changing any module. Layout, CMake, `ARCHITECTURE.md` contract, maturity stages. |
| 03 | [Dependency rules](03-dependency-rules.md) | Before adding any `#include`, link, import, or package dependency. |
| 04 | [Events and messages](04-events-and-messages.md) | When touching events, commands, replication, or service calls. |
| 05 | [Distributed simulation](05-distributed-simulation.md) | Authority, ghosts, handoff, hot-city partitioning, cross-simulation combat. |
| 06 | [GameplayFlow, quests, cutscenes](06-gameplayflow-quests-cutscenes.md) | Scripted sequences, quests, bosses, cutscenes, restrictions. |
| 07 | [Service-backed systems](07-service-backed-systems.md) | Inventory, trade, auction, economy, durable quest progression. |
| 08 | [Implementation roadmap](08-implementation-roadmap.md) | Choosing what to build next. Phases 0–16. |
| 09 | [Validation scenarios](09-validation-scenarios.md) | Writing acceptance tests and phase gates. |
| 10 | [AI development workflow](10-ai-development-workflow.md) | Every implementation task performed by Claude Code. |
| 11 | [Open questions](11-open-questions.md) | Before relying on an area marked as undecided. |
| — | [Glossary](glossary.md) | Whenever a term is unclear. |
