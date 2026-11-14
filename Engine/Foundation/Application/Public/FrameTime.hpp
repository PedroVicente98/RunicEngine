#pragma once

#include <cstdint>

#include "Time.hpp"

namespace Runic::Application {

// The time of one frame, on Core's integer clock (never accumulated floats). A fixed loop gives
// exact values: frame n starts at TickRate::TimeOfTick(n). A loop paced by the top layer measures
// them on the monotonic clock.
struct FrameTime {
    std::uint64_t frame = 0;      // frames completed before this one
    Core::Duration delta{0};      // since the previous frame (0 for the first)
    Core::Duration sinceStart{0}; // since the first frame
};

} // namespace Runic::Application
