# Runic::Core › Public/RenderData

Public RenderData headers (namespace `Runic::RenderData`): plain, header-only structs shared by the Client's extractors, GodotBridge and the debug tools, included as `#include "RenderData/<Name>.hpp"`. No Flecs, Jolt or Godot types, nothing that allocates on its own; positions are 32-bit relative to the frame's one 64-bit origin.

- **Created by:** CORE-3 (slice 2), a slice 5 addition (shape instances), CORE-4 (slice 22), CORE-5 (slice 25a), CORE-6 (slice 42a), CORE-7 (slice 44+)
- **Status:** CORE-3 (slice 2): `Color.hpp`, `Camera.hpp`, `DebugGeometry.hpp`, `DebugDraw.hpp`, `RenderFrame.hpp`,
  `RenderFrameExchange.hpp`; slice 5 (an addition GDB-3 needs, awaiting the owner): `ShapeInstances.hpp`; the contract is the module's [ARCHITECTURE.md](../../ARCHITECTURE.md)
- This folder is a public subfolder of the module's include root.

Layout and rules: [the module's README](../../README.md).
