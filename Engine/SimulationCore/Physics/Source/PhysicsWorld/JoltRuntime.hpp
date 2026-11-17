#pragma once

// Jolt's process-wide setup (its allocator, factory and registered types), shared by every
// PhysicsWorld: the first world sets it up, the last one tears it down. Private to Runic::Physics.

namespace Runic::Physics::Detail {

class JoltRuntime {
  public:
    JoltRuntime();
    ~JoltRuntime();
    JoltRuntime(const JoltRuntime &) = delete;
    JoltRuntime &operator=(const JoltRuntime &) = delete;
};

} // namespace Runic::Physics::Detail
