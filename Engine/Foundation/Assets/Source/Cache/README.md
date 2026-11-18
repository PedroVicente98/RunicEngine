# Runic::Assets › Source/Cache

The asset cache shared by client and server.

- **Filled by:** ASSET-1 (slice 9)
- **Status:** ASSET-1 (slice 9) wrote `AssetCatalog.cpp` (the catalog's text and its problems with their lines) and `AssetCache.cpp` (load once, share, collect; one lock)

Private to `Runic::Assets`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Assets` (or `Runic::Assets::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
