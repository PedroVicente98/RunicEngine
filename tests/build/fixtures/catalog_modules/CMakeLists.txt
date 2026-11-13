cmake_minimum_required(VERSION 3.24)
project(FixtureCatalogModules LANGUAGES CXX)
include("${CMAKE_CURRENT_LIST_DIR}/../FixturePrologue.cmake")
# Modules/<Group>/<Module>/: Alpha (with the interface target Port), Beta and Idle (Foundation, headless; Idle has no
# CMakeLists.txt), View (Presentation, Headless no), Forge (Tools, Headless tool); Programs/Demo is not a group.
# FIXTURE_MODULES uses "," between names; set it empty for none.
foreach(option PRESENTATION TOOLS)
    if(NOT DEFINED FIXTURE_${option})
        set(FIXTURE_${option} OFF)
    endif()
endforeach()
if(NOT DEFINED FIXTURE_MODULES)
    set(FIXTURE_MODULES ALL)
endif()
string(REPLACE "," ";" FIXTURE_MODULES "${FIXTURE_MODULES}")
if(NOT FIXTURE_ROOT)
    set(FIXTURE_ROOT Modules)
endif()

runic_catalog_module(Alpha GROUP Foundation HEADLESS yes)
runic_catalog_module(Beta GROUP Foundation ALLOWS Fixture::Alpha HEADLESS yes)
runic_catalog_module(Idle GROUP Foundation HEADLESS yes)
runic_catalog_module(View GROUP Presentation ALLOWS Fixture::Beta HEADLESS no)
runic_catalog_module(Forge GROUP Tools ALLOWS Fixture::Alpha HEADLESS tool)
runic_catalog_interface(Port FROM Alpha)

runic_add_catalog_modules(ROOT ${FIXTURE_ROOT} LAYOUT GROUP
    PRESENTATION ${FIXTURE_PRESENTATION}
    TOOLS ${FIXTURE_TOOLS}
    MODULES ${FIXTURE_MODULES})

set(found "")
foreach(name Alpha Beta Idle View Forge Port Foundation Presentation Tools)
    if(TARGET Fixture::${name})
        list(APPEND found ${name})
    endif()
endforeach()
list(JOIN found " " found)
message(STATUS "FIXTURE: targets [${found}]")
