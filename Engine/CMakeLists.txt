# Engine modules live at Engine/<Group>/<Module>/ (Public/, Source/, Tests/, ARCHITECTURE.md, CMakeLists.txt).
# A module folder is created by its module's first work package; the catalog (cmake/RunicModuleCatalog.cmake)
# must already hold its row. Slice 0 has the two modules its spike needs.
# Engine/Programs/<Program>/ holds the executables the Engine owns (the RunicEditor; the RunicBot later). They are
# composition roots, not modules: no catalog row, no Public/ folder.
if(RUNIC_ENGINE_ENABLE_PRESENTATION)
    add_subdirectory(Presentation/GodotBridge)
endif()
if(RUNIC_ENGINE_BUILD_TOOLS)
    add_subdirectory(Tools/GodotTools)
    add_subdirectory(Programs/RunicEditor)
endif()
