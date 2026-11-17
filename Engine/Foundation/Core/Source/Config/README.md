# Runic::Core › Source/Config

Process configuration: typed settings read once at start from defaults, a configuration file, the environment and the command line, in that order of precedence (worker count, ports, paths, development flags); an unknown key or a bad value is reported with its source.

- **Filled by:** CORE-8 (slice 3)
- **Status:** CORE-8 (slice 3) wrote `ProcessConfig.cpp`: the declared settings, the file, environment and command-line parsers and their problem reports behind `Public/ProcessConfig.hpp`

Private to `Runic::Core`: other modules reach this code only through the module's public headers. Files here use namespace `Runic::Core` (or `Runic::Core::Detail` for helpers several files of the module share); a helper used by one file sits in an unnamed namespace inside that file. Layout and rules: [the module's README](../../README.md).
