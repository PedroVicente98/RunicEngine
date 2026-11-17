# Runic::Client (Presentation): the client runtime. CLI-1 (slice 5) starts it with the Presentation layer, the
# extractor registry, the debug-shape extractor and the client camera. Contract: ARCHITECTURE.md.
runic_add_module(Client
    GROUP Presentation
    PUBLIC_DEPS Runic::Application Runic::Simulation)
