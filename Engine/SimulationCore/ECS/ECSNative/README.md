# Runic::ECSNative (a target of Runic::ECS)

Headers of the opt-in `Runic::ECSNative` target: the only door to the native Flecs world. Using it is discouraged and the responsibility of whoever uses it; the configure-time dependency check lists every module that links it. A need that more than one module shares goes into the Runic API instead. These headers sit outside `Public/` because they name Flecs types.

- **Created by:** ECS-1 (slice 4)
- **Status:** empty until ECS-1 (slice 4) starts
- This folder is the target's own include root, the way `Public/` is the module's: its headers are included by their path under this folder.

Layout and rules: [the module's README](../README.md).
