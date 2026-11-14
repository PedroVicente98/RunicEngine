// ToLocal (WorldSpace.hpp): the only way from the 64-bit world down to 32-bit floats.

#include "WorldSpace.hpp"

#include "Assert.hpp"

namespace Runic::Core {

Vec3 ToLocal(WorldDelta delta) noexcept {
    RUNIC_ASSERT_MSG(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z <
                         kMaxLocalOffsetM * kMaxLocalOffsetM,
                     "ToLocal: an offset of 10 km or more loses precision as a float");
    return Vec3(static_cast<float>(delta.x), static_cast<float>(delta.y),
                static_cast<float>(delta.z));
}

} // namespace Runic::Core
