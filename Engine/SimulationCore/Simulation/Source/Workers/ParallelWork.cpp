// The runtime's side of its parallel loops (ParallelWork.hpp): numbering them and keeping one at a
// time.

#include "ParallelWork.hpp"

#include "Scheduler/RuntimeState.hpp"

namespace Runic::Simulation::Detail {

ParallelLoops &ParallelAccess::Begin(SimulationRuntime &runtime) {
    ParallelLoops &loops = runtime.m_State->parallel;
    RUNIC_ASSERT_MSG(!loops.running, "a parallel loop was started inside another parallel loop");
    RUNIC_ASSERT_MSG(runtime.Workers().CurrentWorker() == Core::kDrivingWorker,
                     "a parallel loop was started off the driving thread");
    loops.running = true;
    loops.current = loops.next++;
    loops.runs.clear();
    return loops;
}

void ParallelAccess::End(SimulationRuntime &runtime) noexcept {
    runtime.m_State->parallel.running = false;
}

bool ParallelAccess::IsRunning(const SimulationRuntime &runtime) noexcept {
    return runtime.m_State->parallel.running;
}

} // namespace Runic::Simulation::Detail
