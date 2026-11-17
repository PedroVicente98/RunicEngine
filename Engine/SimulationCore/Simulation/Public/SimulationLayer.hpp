#pragma once

#include <cstdint>
#include <memory>
#include <string_view>

#include "Layer.hpp"
#include "SimulationRuntime.hpp"

// The Simulation layer: the Application layer that drives a SimulationRuntime inside a process's
// LayerStack (the game server, the RunicEditor's local world). Each update it works out from the
// loop's time how many 60 Hz steps are owed and runs them. Wall-clock time only decides how many
// steps are owed; the simulation itself advances in exact fixed steps, and a step the process had
// no time for stays owed: it is measured and reported, never skipped (RunicDocs › Simulation core
// clock and threads).

namespace Runic::Simulation {

inline constexpr std::string_view kSimulationLayerName = "Simulation";

// Master steps one update may run when the simulation is behind. The rest stay owed for the next
// updates, so a long stall is caught up over several frames instead of freezing one.
inline constexpr std::uint32_t kDefaultMaxTicksPerUpdate = 5;

// The backlog gauge the layer records (master steps owed after an update), with the Hz60 label.
inline constexpr std::string_view kBacklogMetric = "simulation.backlog_ticks";

struct SimulationLayerSpec {
    std::uint32_t maxTicksPerUpdate = kDefaultMaxTicksPerUpdate;
};

class SimulationLayer final : public Application::Layer {
  public:
    explicit SimulationLayer(std::unique_ptr<SimulationRuntime> runtime,
                             const SimulationLayerSpec &spec = {});
    ~SimulationLayer() override;

    SimulationRuntime &Runtime() noexcept;
    const SimulationRuntime &Runtime() const noexcept;

    // Master steps owed after the last update: 0 while the process keeps up.
    std::uint64_t BacklogTicks() const noexcept;

    // The simulation's time starts with the first update after the attach: that update runs
    // master tick 0.
    Core::Result<void> OnAttach(Application::LayerContext &context) override;
    void OnUpdate(const Application::FrameTime &time) override;
    // Logs how many ticks each domain ran.
    void OnDetach() override;

  private:
    std::unique_ptr<SimulationRuntime> m_Runtime;
    SimulationLayerSpec m_Spec;
    Core::GaugeId m_Backlog;
    Core::Duration m_Start{};     // the loop's time at the first update
    std::uint64_t m_TicksRun = 0; // master steps this layer has run
    std::uint64_t m_BacklogTicks = 0;
    std::uint64_t m_UpdatesBehind = 0; // updates in a row that ended with a backlog
    bool m_Started = false;
};

} // namespace Runic::Simulation
