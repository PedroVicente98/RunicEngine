# Runic::Combat › Source/Abilities

The ability toolkit: timelines of named phases from data, phase lengths set by a Game hook, phase events, cancel requests, and hit timing per skill (ServerTime, CappedRewind).

- **Filled by:** CMB-1 (slice 16)
- **Status:** empty until CMB-1 (slice 16) starts

Private to `Runic::Combat`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Combat` (or `Runic::Combat::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
