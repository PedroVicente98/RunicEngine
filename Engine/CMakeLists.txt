# Engine modules live at Engine/<Group>/<Module>/ (Public/, Source/<Subsystem>/, Tests/, ARCHITECTURE.md,
# CMakeLists.txt). Every module and subsystem folder exists from the start with a README (decided 2026-11-12); a
# module joins the build when its first work package adds its CMakeLists.txt and the add_subdirectory line below
# (BUILD-2 will add modules from the catalog automatically, keyed on that CMakeLists.txt). The catalog
# (cmake/RunicModuleCatalog.cmake) must already hold the module's row. Slice 0 has code in two modules.
# Engine/Programs/<Program>/ holds the executables the Engine owns (the RunicEditor; the RunicBot later). They are
# composition roots, not modules: no catalog row, no Public/ folder.
if(RUNIC_ENGINE_ENABLE_PRESENTATION)
    add_subdirectory(Presentation/GodotBridge)
endif()
if(RUNIC_ENGINE_BUILD_TOOLS)
    add_subdirectory(Tools/GodotTools)
    add_subdirectory(Programs/RunicEditor)
endif()
