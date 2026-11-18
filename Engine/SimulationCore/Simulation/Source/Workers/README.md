# Runic::Simulation › Source/Workers

Phases on the worker pool, per-worker buffers and the deterministic merge; the physics step runs through PHYS-4.

- **Filled by:** SIM-3 (slice 10)
- **Status:** SIM-3 (slice 10) wrote `ParallelWork.cpp`: the runtime's side of its parallel loops (one at a time, numbered in the order they run, their runs kept between loops). `ParallelEach` and `ParallelOutput` are templates in `Public/ParallelWork.hpp`

Private to `Runic::Simulation`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Simulation` (or `Runic::Simulation::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
