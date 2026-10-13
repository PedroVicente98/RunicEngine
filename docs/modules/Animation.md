---
# obsidian-graph: generated tags and aliases
tags:
  - repo/engine
  - source/arch
  - type/module
  - group/presentation
  - headless/no
  - slice/23
aliases:
  - "Runic::Animation"
---

# Animation

Contract seed for `Runic::Animation`: what a Claude Code session needs to implement the Animation work package. When `Engine/Presentation/Animation/` is created, this file becomes its `ARCHITECTURE.md`, and that file is authoritative from then on.

| Field | Value |
| --- | --- |
| Group | Presentation |
| Directory | `Engine/Presentation/Animation/` |
| CMake target | `Runic::Animation` |
| Namespace | `Runic::Animation` |
| Headless | no |
| Status | Not started |

## Purpose

Animation evaluates skeletons, clips and blending on the client and produces poses; the character renderer uploads them (ARCH[^arch] § Client runtime). The server never evaluates animation: gameplay timing comes from ability timelines.

## Owns

- Skeletons, clips, the animator, blending and poses (ANIM-1).
- Pose output in the form the skinned extractor (CLI-5[^engine-client]) writes into RenderData pose buffers (CORE-4[^engine-core]).

## Does not own

- Cooking skinned meshes and clips (Cooker[^engine-cooker], COOK-5) and loading them (Assets).
- The skinned extractor (Client, CLI-5[^engine-client]) and pose upload and drawing (Renderer[^engine-renderer], RND-5).
- Gameplay timing (Combat[^engine-combat] ability timelines) and any authoritative state.
- Which animation plays for which game action (RunicGame::Client presentation).

## Depends on

| Kind | Allowed |
| --- | --- |
| Engine modules | Core[^engine-core], Assets[^engine-assets] |
| Third-party | — |

### Must never depend on

- Simulation, Physics[^engine-physics], ECS[^engine-ecs], Combat[^engine-combat] or any server module; the server never links Animation.
- Renderer[^engine-renderer] or bgfx: poses are plain data.
- RunicPlatform or RunicGame code.

## Public API sketch

Sketch, fixed in the contract session: `Skeleton.hpp`, `AnimationClip.hpp`, `Animator.hpp`, `AnimationBlend.hpp`, `Pose.hpp`.

## Threading and tick rules

- Evaluation runs on the client, once per frame, driven by the Presentation layer before extraction reads the poses.
- Evaluation reads clips and writes poses only; it never touches simulation state.

## Work packages

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| ANIM-1 | Skeletons, clips, animator, blending | Clips blend without pops | ASSET-1[^engine-assets] | 23 |

## Acceptance tests

The Done when column is the acceptance list. Slice 23 shows a skinned character animating with blended clips (with CLI-5[^engine-client], RND-5[^engine-renderer] and COOK-5[^engine-cooker]), using an Engine test rig.

## Open items

- None recorded in ARCH[^arch].

<!-- obsidian-graph:start -->

---

## Connections

*Generated for the Obsidian graph. The numbered markers in the text jump to the footnotes below, which link the notes they refer to.*

- **Depends on:** [[../modules/Core|Core]] · [[../modules/Assets|Assets]]
- **Depended on by:** [[../modules/Client|Client]]
- **See also:** [[../modules/README|Engine module catalog]] · [[../system/03-dependency-rules|03 — Dependency rules]] · [[../system/12-work-packages|12 — Work packages]] · [[../current-state|Engine current state]]

[^arch]: #source/arch — ARCH, "RunicEngine — Technical Architecture v3": the architecture source of truth (a project document, not a note in this vault)
[^engine-client]: [[../modules/Client|Client]] — Engine module · Presentation
[^engine-core]: [[../modules/Core|Core]] — Engine module · Foundation
[^engine-cooker]: [[../modules/Cooker|Cooker]] — Engine module · Tools
[^engine-renderer]: [[../modules/Renderer|Renderer]] — Engine module · Presentation
[^engine-combat]: [[../modules/Combat|Combat]] — Engine module · GameplayAPI
[^engine-assets]: [[../modules/Assets|Assets]] — Engine module · Foundation
[^engine-physics]: [[../modules/Physics|Physics]] — Engine module · Simulation core
[^engine-ecs]: [[../modules/ECS|ECS]] — Engine module · Simulation core
