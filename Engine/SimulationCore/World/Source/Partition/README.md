# Runic::World › Source/Partition

The world registry (what each WorldId is: one seamless world with its coordinate space, origin and partition; instances are InstanceIds of a world), Regions, Sectors and sparse 3D Cell keys. The partition is stable; which simulation owns what is decided elsewhere.

- **Filled by:** WORLD-1 (slice 4)
- **Status:** empty until WORLD-1 (slice 4) starts

Private to `Runic::World`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::World` (or `Runic::World::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
