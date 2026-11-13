# Runic::Combat › Source/HitResolution

Hit resolution: what a detected hit does. Each hit carries Game-defined fields and runs through Game-registered stages in order, which decide its outcome (Guard, Evasion, armor, damage in RunicGame); invulnerability is a restriction that ignores hits by tag. Damage types and formulas are Game rules, not Engine code.

- **Filled by:** CMB-5 (slice 18)
- **Status:** empty until CMB-5 (slice 18) starts

Private to `Runic::Combat`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Combat` (or `Runic::Combat::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
