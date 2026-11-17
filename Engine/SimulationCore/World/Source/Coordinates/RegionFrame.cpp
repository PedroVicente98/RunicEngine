// Region-local coordinates (RegionFrame.hpp).

#include "RegionFrame.hpp"

namespace Runic::World {

bool RegionFrame::Reaches(Core::WorldPosition position) const noexcept {
    const Core::WorldDelta offset = position - origin;
    return offset.x * offset.x + offset.y * offset.y + offset.z * offset.z <
           Core::kMaxLocalOffsetM * Core::kMaxLocalOffsetM;
}

} // namespace Runic::World
