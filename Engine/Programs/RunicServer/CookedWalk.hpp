#pragma once

#include <filesystem>
#include <memory>

#include "Application.hpp"
#include "Math.hpp"
#include "Result.hpp"
#include "SimulationRuntime.hpp"

// RunicServer --cooked <folder> (slice 9, the first cooker proof's server side): the server loads
// the cells of a cooked folder into its simulation's physics as static bodies — every placement's
// cooked collision and every collider, at the cell's 64-bit origin plus their offsets — and a
// character spawned near the first cell's origin walks on them. When the server stops it says
// where the character ended. Until world streaming (WORLD-3, COOK-3, slice 21) owns loading cells,
// the program does it here; the editor's GodotTools does the same for its local world.

namespace Runic::Programs {

struct CookedWalkSpec {
    std::filesystem::path folder;
    Core::Vec3 spawn{0.0f, 2.0f, 0.0f};    // from the first cell's origin
    Core::Vec3 velocity{3.0f, 0.0f, 0.0f}; // m/s, every realtime tick
};

// Loads the folder into runtime (which it gives physics) and returns the layer that reports the
// walk when the server stops; an error when the folder cannot be read.
Core::Result<std::unique_ptr<Application::Layer>>
MakeCookedWalk(Simulation::SimulationRuntime &runtime, const CookedWalkSpec &spec);

} // namespace Runic::Programs
