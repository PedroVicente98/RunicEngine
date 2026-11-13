# Runic::Application › Source/EntryPoint

The Engine-owned `main()`: it reads the command line (UTF-8 on every platform), calls the `CreateApplication` each program defines, and hands the result to the ApplicationHost. Every argument stays available unchanged, because Godot's project manager relaunches the executable with its own arguments.

- **Filled by:** APP-1 (slice 1)
- **Status:** empty until APP-1 (slice 1) starts

Private to `Runic::Application`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Application` (or `Runic::Application::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
