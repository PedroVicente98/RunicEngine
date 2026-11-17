#pragma once

#include <string_view>

#include "ProcessConfig.hpp"
#include "SimulationLayer.hpp"
#include "SimulationRuntime.hpp"

// The Simulation's settings in the process configuration: a program declares them before it loads
// its configuration and builds the runtime's and the layer's specs from the loaded values
// (RunicDocs › Modules, work packages › SIM-1, CORE-8).

namespace Runic::Simulation {

// Worker slots of the simulation's pool, from Core::kSingleWorker to Core::kMaxWorkers. A pod
// sizes it from its CPU allotment (RunicDocs › Platform and operations).
inline constexpr std::string_view kWorkersSetting = "simulation.workers";
// Master steps one update may run when the simulation is behind, from 1 to
// kMaxTicksPerUpdateLimit.
inline constexpr std::string_view kMaxTicksPerUpdateSetting = "simulation.max_ticks_per_update";
// One second of master steps.
inline constexpr std::uint32_t kMaxTicksPerUpdateLimit = Core::kMasterTicksPerSecond;

struct SimulationSettings {
    Core::IntSetting workers;
    Core::IntSetting maxTicksPerUpdate;
};

// Declares the settings above with their defaults (one worker, kDefaultMaxTicksPerUpdate).
SimulationSettings DeclareSimulationSettings(Core::ProcessConfig &config);

// The specs a loaded configuration asks for. The caller sets the spec's id and metrics.
SimulationSpec SimulationSpecFrom(const Core::ProcessConfig &config,
                                  const SimulationSettings &settings);
SimulationLayerSpec SimulationLayerSpecFrom(const Core::ProcessConfig &config,
                                            const SimulationSettings &settings);

} // namespace Runic::Simulation
