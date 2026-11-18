#pragma once

#include "SimulationRuntime.hpp"

// RunicServer --parallel-demo (slice 10, the worker pool's server side): a scene whose every step
// splits its work across the simulation's workers. Thousands of movers drift inside a box and hit
// each other; a hit lowers its target's health in a way that depends on the order hits arrive in,
// a mover whose health runs out is destroyed and now and then one spawns another; beside them
// crates fall on a floor and characters walk among them, the physics step on the same pool. At
// realtime tick 60 the server logs a hash of every entity's values and every fact, with the
// worker count: run it with --set simulation.workers=1 and =4 and the hash is the same.

namespace Runic::Programs {

// Builds the scene in runtime (which it gives physics) and adds its systems.
void AddParallelDemo(Simulation::SimulationRuntime &runtime);

} // namespace Runic::Programs
