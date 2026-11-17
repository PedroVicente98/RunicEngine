# Runic::Events (Foundation): typed event buffers and readers with stable event IDs. EVT-1 (slice 4) starts it.
# Contract: ARCHITECTURE.md.
runic_add_module(Events
    GROUP Foundation
    PUBLIC_DEPS Runic::Core
    HEADLESS)
