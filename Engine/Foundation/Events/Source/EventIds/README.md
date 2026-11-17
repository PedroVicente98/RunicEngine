# Runic::Events › Source/EventIds

Stable event IDs (SimulationId plus a sequence) that stay the same on replay, so durable consumers can deduplicate.

- **Filled by:** EVT-1 (slice 4)
- **Status:** EVT-1 (slice 4) wrote `EventSequence.cpp`: a simulation's event sequence and an ID as text, behind `Public/EventId.hpp`

Private to `Runic::Events`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Events` (or `Runic::Events::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
