#pragma once

#include <cstdint>

#include "PlainIds.hpp"
#include "TickDomain.hpp"
#include "Time.hpp"

// What a system is given when it runs: which tick this is and how long it lasts. A system reads
// time from here and nowhere else; nothing in a simulation reads the wall clock (RunicDocs ›
// Simulation core clock and threads).

namespace Runic::Simulation {

struct TickContext {
    TickDomain domain = TickDomain::Hz60; // the domain of the system that runs
    Core::TickId masterTick;              // the 60 Hz step this tick runs in
    std::uint64_t domainTick = 0;         // the tick's number in its domain, from 0
    float dt = 0.0f;                      // the domain's fixed step in seconds
    Core::Duration time{};                // simulation time at this tick: exact, from its number
};

} // namespace Runic::Simulation
