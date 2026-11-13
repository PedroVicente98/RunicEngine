# Runic::Combat › Source/HitDetection

Hit detection: combat volumes and projectile sweeps over Physics queries; every hit as an ordered list along the hit timeline (time of contact, then distance) with deterministic tie-breaking; filters (relation, tags, already hit, line of sight); per-ability hit limits applied as data. It answers who is hit and in what order; how many hits an attack takes and when a projectile stops are Game data; hit resolution then decides what each hit does.

- **Filled by:** CMB-2 (slice 16)
- **Status:** empty until CMB-2 (slice 16) starts

Private to `Runic::Combat`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Combat` (or `Runic::Combat::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
