# Runic::Assets › Source/Handles

Asset handles over the AssetId value type from Core.

- **Filled by:** ASSET-1 (slice 9)
- **Status:** ASSET-1 (slice 9) wrote `AssetKind.cpp` (the kinds by name); the handle itself is the template in `Public/AssetHandle.hpp`

Private to `Runic::Assets`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Assets` (or `Runic::Assets::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
