// The Simulation's settings (SimulationSettings.hpp).

#include "SimulationSettings.hpp"

namespace Runic::Simulation {

namespace {

constexpr std::string_view kWorkersHelp =
    "Worker slots of the simulation's pool, the driving thread included";
constexpr std::string_view kMaxTicksPerUpdateHelp =
    "Master steps one update may run when the simulation is behind";
constexpr std::int64_t kFewestTicksPerUpdate = 1;

} // namespace

SimulationSettings DeclareSimulationSettings(Core::ProcessConfig &config) {
    SimulationSettings settings;
    settings.workers =
        config.DeclareInt(kWorkersSetting, Core::kSingleWorker,
                          Core::IntRange{Core::kSingleWorker, Core::kMaxWorkers}, kWorkersHelp);
    settings.maxTicksPerUpdate = config.DeclareInt(
        kMaxTicksPerUpdateSetting, kDefaultMaxTicksPerUpdate,
        Core::IntRange{kFewestTicksPerUpdate, kMaxTicksPerUpdateLimit}, kMaxTicksPerUpdateHelp);
    return settings;
}

SimulationSpec SimulationSpecFrom(const Core::ProcessConfig &config,
                                  const SimulationSettings &settings) {
    SimulationSpec spec;
    spec.workers = static_cast<std::uint32_t>(config.Get(settings.workers));
    return spec;
}

SimulationLayerSpec SimulationLayerSpecFrom(const Core::ProcessConfig &config,
                                            const SimulationSettings &settings) {
    SimulationLayerSpec spec;
    spec.maxTicksPerUpdate = static_cast<std::uint32_t>(config.Get(settings.maxTicksPerUpdate));
    return spec;
}

} // namespace Runic::Simulation
