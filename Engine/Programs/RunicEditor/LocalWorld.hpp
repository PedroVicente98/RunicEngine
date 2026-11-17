#pragma once

#include "SimulationRuntime.hpp"

// The RunicEditor's local world (slice 6): the simulation that runs in the editor process for the
// docks to inspect. Until the editor loads a project's world (the Cooker and region tools, slices 9
// and 21), it holds a neutral scene: one region and a ring of drifting markers, engine: names only.

namespace Runic::Programs {

// Fills runtime with the scene and adds the system that moves it.
void PopulateLocalWorld(Simulation::SimulationRuntime &runtime);

} // namespace Runic::Programs
