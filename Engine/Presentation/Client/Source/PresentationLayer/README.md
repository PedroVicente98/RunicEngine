# Runic::Client › Source/PresentationLayer

The Presentation layer: interpolation, camera and render extraction into the RenderFrame each frame.

- **Filled by:** CLI-1 (slice 5)
- **Status:** CLI-1 (slice 5) wrote `PresentationLayer.cpp` (the camera, the extractors and the frame they write, one frame per update)

Private to `Runic::Client`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Client` (or `Runic::Client::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
