# Runic::Core › Source/Ids

The plain ID types every module and process shares: GlobalEntityId, GlobalPlayerId, SimulationId, WorldId (one seamless world: every place reachable without a loading screen), InstanceId (one copy of a world), TickId, AuthorityEpoch, and value types other modules give meaning to (AssetId, WaterBodyId, FieldId, WidgetId). Core defines the types; the Game's data gives the values. Local Flecs entity IDs never leave a process; these do.

- **Filled by:** CORE-1 (slice 1)
- **Status:** header-only since CORE-1: the IDs live in `Public/PlainIds.hpp`, so this folder stays empty

Private to `Runic::Core`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Core` (or `Runic::Core::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
