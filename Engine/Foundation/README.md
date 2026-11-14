# Foundation

The base every other module builds on: plain value types, the entry point and the layer stack, typed event buffers, content IDs and asset loading. Every module here is headless.

| Module | Target | First package | Headless |
| --- | --- | --- | --- |
| [Core](Core/README.md) | `Runic::Core` | CORE-1 (slice 1) | yes |
| [Application](Application/README.md) | `Runic::Application` | APP-1 (slice 1) | yes |

## Planned modules

No folder yet: a module's folder appears with its first package, which takes the module's READMEs from the `layout`
branch (`git checkout origin/layout -- Engine/Foundation/<Module>/README.md`; decided 2026-11-14).

| Module | Target | First package | Headless |
| --- | --- | --- | --- |
| Events | `Runic::Events` | EVT-1 (slice 4) | yes |
| Content | `Runic::Content` | CNT-1 (slice 9) | yes |
| Assets | `Runic::Assets` | ASSET-1 (slice 9) | yes |

A group is a folder (and at most an umbrella target): it has no code of its own. Every module lives at
`Engine/Foundation/<Module>/` with the same shape; see [the Engine README](../README.md).
