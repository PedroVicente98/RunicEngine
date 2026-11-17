# Runic::Client › Source/InputMapping

Input mapping, actions and the command builder over the AppEvents the GodotLayer forwards (modifiers, chords, press and release, layouts and presets the Game defines).

- **Filled by:** CLI-2 (slice 7)
- **Status:** CLI-2 (slice 7) wrote `InputMap.cpp` (actions, presets, bindings with modifiers and chords where the more specific binding wins, presses and releases per frame, the movement helpers) and `InputLayer.cpp` (the Input layer and its command builders)

Private to `Runic::Client`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Client` (or `Runic::Client::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
