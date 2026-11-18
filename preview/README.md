# preview/ — slices 3 to 11 on one branch

This folder exists only on the branch `preview/slices-3-11`. The branch was cut from `slice/2` to build slices 3 to 11
in one place, so the owner can see where the project is heading without mixing that work into the slice branches,
`main`, the GitHub issues or RunicDocs. Nothing here is merged anywhere, and no issue or RunicDocs note was changed.

Each slice was built in the order of the roadmap, package by package, the way the slice pipeline asks (RunicDocs ›
Building with Claude Code): the contract first (the module's `ARCHITECTURE.md`, its public headers, its acceptance
tests), then the code, then the checks. Every package is one commit named after it (`CORE-2: …`); a RunicGame package (GAME-1) is committed on RunicGame's
branch of the same name, `preview/slices-3-11`. Every slice ends
with a commit that adds its note here. The work stopped only after finished packages: the table below says how far it
got, and every slice in it is done.

## Status

| Slice | Packages | Status | Note |
| --- | --- | --- | --- |
| 3 · Fixed-step clock | CORE-2, CORE-8, SIM-1 | **Done** | [slice-03-fixed-step-clock.md](slice-03-fixed-step-clock.md) |
| 4 · ECS and world coordinates | ECS-1, EVT-1, ECS-2, WORLD-1 | **Done** | [slice-04-ecs-and-world-coordinates.md](slice-04-ecs-and-world-coordinates.md) |
| 5 · Moving entities | SIM-2, CLI-1, GDB-3 (and a Core addition) | **Done** | [slice-05-moving-entities.md](slice-05-moving-entities.md) |
| 6 · Editor shell | GDT-2, GAME-1 (RunicGame, same branch name) | **Done** | [slice-06-editor-shell.md](slice-06-editor-shell.md) |
| 7 · Physics | PHYS-1, PHYS-2, SIM-8, CLI-2 (and fixes to PHYS-1 and GDT-2) | **Done** | [slice-07-physics.md](slice-07-physics.md) |
| 8 · Physics view | PHYS-3, GDB-4, GDT-3 (and a Core addition) | **Done** | [slice-08-physics-view.md](slice-08-physics-view.md) |
| 9 · First cooker proof | CNT-1, ASSET-1, COOK-1, GDT-4 | **Done** | [slice-09-first-cooker-proof.md](slice-09-first-cooker-proof.md) |
| 10 · Worker pool | PHYS-4, SIM-3 | **Done** | [slice-10-worker-pool.md](slice-10-worker-pool.md) |
| 11 · Spatial index | WORLD-2 | **Done** | [slice-11-spatial-index.md](slice-11-spatial-index.md) |

## The whole branch, checked (2026-11-18)

Slices 3 to 11 are done: 26 packages. 25 are Engine packages, on this branch since `slice/2` (`git log
origin/slice/2..` lists them); GAME-1 is on RunicGame's branch. Every package has its own commit or commits, named after it, and
every slice ends with its note's commit.

- **The Engine, both builds.** The debug build (Godot, tools) runs 532 tests and the headless build 420, all passing.
  That includes the build checks (`checks.catalog_sync`, the public-header and include scans, the headless check)
  and every program's CTest.
- **Every headless module under every checker.** Core, Application, Events, ECS, Content, Assets, World, Physics and
  Simulation pass under AddressSanitizer with UBSan, ThreadSanitizer, clang and Release. The Client module's tests
  pass in the presentation trees (ASan, clang, Release).
- **RunicGame** (`preview/slices-3-11`) rebuilds against this Engine: its editor library and RunicEditor build, and
  its checks pass. Its server and client build tests still fail, for the reason slice 6 gave: RunicGame's `main` links
  the `Runic::Runtime` bundle the Engine removed in slice 1, and GAME-0 is not on it.
- **RunicPlatform** (`main`, untouched by these slices) builds against this Engine, and its one test passes when run
  directly. CTest cannot list it, because its discovery script is not found. The same happens against `slice/2`, so
  this branch did not cause it; it is the Platform's to look at.
- **Where the plan goes next.** Slice 12 (Transport) is the Platform's GameNetworkingSockets transport, so this branch
  ends here. The notes' "Decisions that await the owner" tables are the review list: about a dozen per slice, each with
  its reason and its alternative.

## How this differs from the normal pipeline

- **No owner review between steps.** The pipeline has the owner approve each contract before code and each slice's
  walkthrough before the next slice. Here every step ran without those stops, so every design choice the pipeline
  would have asked about is listed under "Decisions that await the owner" in the slice's note. They are choices, not
  facts: any of them may change when the slice is built for real.
- **Slice notes live here**, not in GitHub issues: one file per slice with its brief (what was built, the decisions)
  and its walkthrough (the order to read the code in, one tick or frame traced through it, what each test proves, the
  cost of the hot paths, what is left for later slices).
- **Module documents are the real ones.** Each module's `ARCHITECTURE.md` and `README.md` were written and updated as
  the pipeline requires, with the status "implemented on the preview branch, awaiting the owner's review".

## Building and checking

```sh
cmake --preset headless && cmake --build --preset headless && ctest --preset headless   # no Godot: slices 3, 4, 7, 10, 11
cmake --preset debug && cmake --build --preset debug && ctest --preset debug            # with Godot: slices 5, 6, 8, 9, and 7's and 11's client scenes
```

Every package was also built with clang and warnings as errors, in Release, and run under ThreadSanitizer,
AddressSanitizer and UBSan (GCC), in build folders that are not part of the repository.
