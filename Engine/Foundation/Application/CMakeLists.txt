# Runic::Application (Foundation): the entry point, Application, ApplicationHost (Run and RunFrame), Layer and
# LayerStack, loop policies and application events. Contract: ARCHITECTURE.md. APP-1 (slice 1).
runic_add_module(Application
    GROUP Foundation
    PUBLIC_DEPS Runic::Core
    HEADLESS)

if(WIN32)
    target_link_libraries(runic_application PRIVATE shell32)  # CommandLineToArgvW: the UTF-8 command line
endif()
