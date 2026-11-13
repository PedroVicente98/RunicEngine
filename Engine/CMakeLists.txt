# Engine modules live at Engine/<Group>/<Module>/ (Public/, Source/<Subsystem>/, Tests/, ARCHITECTURE.md,
# CMakeLists.txt). Every module and subsystem folder exists from the start with a README (decided 2026-11-12).
#
# Modules from the catalog (BUILD-2, decided 2026-11-12): every module of cmake/RunicModuleCatalog.cmake whose folder
# has a CMakeLists.txt joins the build, so a module's first package adds only its own CMakeLists.txt and never edits
# this file. A row whose Headless value is "no" needs presentation; "tool" and "never-shipped" need the tools.
# RUNIC_ENGINE_MODULES limits a consumer's build to what it links (RunicPlatform). Interface targets join from
# <module>/<Target>/CMakeLists.txt; each group with a configured module gets its umbrella target (Runic::GameplayAPI).
runic_add_catalog_modules(ROOT "${CMAKE_CURRENT_SOURCE_DIR}" LAYOUT GROUP
    PRESENTATION ${RUNIC_ENGINE_ENABLE_PRESENTATION}
    TOOLS ${RUNIC_ENGINE_BUILD_TOOLS}
    MODULES ${RUNIC_ENGINE_MODULES})

# Engine/Programs/<Program>/ holds the executables the Engine owns. They are composition roots, not modules: no
# catalog row, no Public/ folder. Each joins the build when its folder has a CMakeLists.txt, which the package that
# shows its result writes (decided 2026-11-12: APP-1 creates RunicServer, GDB-1 creates RunicClient). RunicServer and
# RunicBot need RUNIC_ENGINE_BUILD_PROGRAMS, RunicClient also presentation, the RunicEditor the tools.
macro(_runic_add_program program)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Programs/${program}/CMakeLists.txt")
        add_subdirectory(Programs/${program})
    endif()
endmacro()
if(RUNIC_ENGINE_BUILD_PROGRAMS)
    _runic_add_program(RunicServer)
    _runic_add_program(RunicBot)
    if(RUNIC_ENGINE_ENABLE_PRESENTATION)
        _runic_add_program(RunicClient)
    endif()
endif()
if(RUNIC_ENGINE_BUILD_TOOLS)
    _runic_add_program(RunicEditor)
endif()
