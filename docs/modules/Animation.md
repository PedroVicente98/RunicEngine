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

Animation evaluates skeletons, clips and blending on the client and produces poses; the character renderer uploads them (ARCH § Client runtime). The server never evaluates animation: gameplay timing comes from ability timelines.

## Owns

- Skeletons, clips, the animator, blending and poses (ANIM-1).
- Pose output in the form the skinned extractor (CLI-5) writes into RenderData pose buffers (CORE-4).

## Does not own

- Cooking skinned meshes and clips (Cooker, COOK-5) and loading them (Assets).
- The skinned extractor (Client, CLI-5) and pose upload and drawing (Renderer, RND-5).
- Gameplay timing (Combat ability timelines) and any authoritative state.
- Which animation plays for which game action (RunicGame::Client presentation).

## Depends on

| Kind | Allowed |
| --- | --- |
| Engine modules | Core, Assets |
| Third-party | — |

### Must never depend on

- Simulation, Physics, ECS, Combat or any server module; the server never links Animation.
- Renderer or bgfx: poses are plain data.
- RunicPlatform or RunicGame code.

## Public API sketch

Sketch, fixed in the contract session: `Skeleton.hpp`, `AnimationClip.hpp`, `Animator.hpp`, `AnimationBlend.hpp`, `Pose.hpp`.

## Threading and tick rules

- Evaluation runs on the client, once per frame, driven by the Presentation layer before extraction reads the poses.
- Evaluation reads clips and writes poses only; it never touches simulation state.

## Work packages

| ID | Package | Done when | Needs | Slice |
| --- | --- | --- | --- | --- |
| ANIM-1 | Skeletons, clips, animator, blending | Clips blend without pops | ASSET-1 | 23 |

## Acceptance tests

The Done when column is the acceptance list. Slice 23 shows a skinned character animating with blended clips (with CLI-5, RND-5 and COOK-5), using an Engine test rig.

## Open items

- None recorded in ARCH.
