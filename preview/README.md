# preview/ — slices 3 to 11 on one branch

This folder exists only on the branch `preview/slices-3-11`. The branch was cut from `slice/2` to build slices 3 to 11
in one place, so the owner can see where the project is heading without mixing that work into the slice branches,
`main`, the GitHub issues or RunicDocs. Nothing here is merged anywhere, and no issue or RunicDocs note was changed.

Each slice was built in the order of the roadmap, package by package, the way the slice pipeline asks (RunicDocs ›
Building with Claude Code): the contract first (the module's `ARCHITECTURE.md`, its public headers, its acceptance
tests), then the code, then the checks. Every package is one commit named after it (`CORE-2: …`), and every slice ends
with a commit that adds its note here. If the work stops, it stops after a finished package: the table below says how
far it got, and the next package in it is where to continue.

## Status

| Slice | Packages | Status | Note |
| --- | --- | --- | --- |
| 3 · Fixed-step clock | CORE-2, CORE-8, SIM-1 | **Done** | [slice-03-fixed-step-clock.md](slice-03-fixed-step-clock.md) |
| 4 · ECS and world coordinates | ECS-1, EVT-1, ECS-2, WORLD-1 | **Done** | [slice-04-ecs-and-world-coordinates.md](slice-04-ecs-and-world-coordinates.md) |
| 5 · Moving entities | SIM-2, CLI-1, GDB-3 (and a Core addition) | **Done** | [slice-05-moving-entities.md](slice-05-moving-entities.md) |
| 6 · Editor shell | GDT-2, GAME-1 | Not started | |
| 7 · Physics | PHYS-1, PHYS-2, SIM-8, CLI-2 | Not started | |
| 8 · Physics view | PHYS-3, GDB-4, GDT-3 | Not started | |
| 9 · First cooker proof | CNT-1, ASSET-1, COOK-1, GDT-4 | Not started | |
| 10 · Worker pool | PHYS-4, SIM-3 | Not started | |
| 11 · Spatial index | WORLD-2 | Not started | |

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
cmake --preset debug && cmake --build --preset debug && ctest --preset debug            # with Godot: slices 5, 6, 8, 9
```

Every package was also built with clang and warnings as errors, in Release, and run under ThreadSanitizer,
AddressSanitizer and UBSan (GCC), in build folders that are not part of the repository.
