# Reads a catalog file in script mode (cmake -P) by replacing the catalog functions with recorders.
#
#   include(CatalogReader.cmake)
#   runic_read_catalog(<file>)        defines, for every row <Name> (bare name, namespace dropped):
#       CATALOG_NAMES                 the row names, in file order
#       CATALOG_<Name>_NS             RUNIC_MODULE_NAMESPACE when the row was declared (default Runic)
#       CATALOG_<Name>_GROUP, _ALLOWS, _THIRD_PARTY, _PUBLIC_EXCEPTIONS, _ALLOWS_ANY_MODULE, _HEADLESS
include_guard(GLOBAL)

macro(runic_catalog_module _name)
    cmake_parse_arguments(_ROW "ALLOWS_ANY_MODULE" "GROUP;HEADLESS" "ALLOWS;THIRD_PARTY;PUBLIC_EXCEPTIONS" ${ARGN})
    list(APPEND CATALOG_NAMES "${_name}")
    if(DEFINED RUNIC_MODULE_NAMESPACE AND NOT RUNIC_MODULE_NAMESPACE STREQUAL "")
        set(CATALOG_${_name}_NS "${RUNIC_MODULE_NAMESPACE}")
    else()
        set(CATALOG_${_name}_NS "Runic")
    endif()
    set(CATALOG_${_name}_GROUP "${_ROW_GROUP}")
    set(CATALOG_${_name}_ALLOWS "${_ROW_ALLOWS}")
    set(CATALOG_${_name}_THIRD_PARTY "${_ROW_THIRD_PARTY}")
    set(CATALOG_${_name}_PUBLIC_EXCEPTIONS "${_ROW_PUBLIC_EXCEPTIONS}")
    set(CATALOG_${_name}_ALLOWS_ANY_MODULE "${_ROW_ALLOWS_ANY_MODULE}")
    set(CATALOG_${_name}_HEADLESS "${_ROW_HEADLESS}")
endmacro()

macro(runic_catalog_interface _name)
    # Interface targets are not table rows; nothing to record.
endmacro()

macro(runic_read_catalog _file)
    if(NOT EXISTS "${_file}")
        message(FATAL_ERROR "RUNIC: catalog file '${_file}' does not exist")
    endif()
    set(CATALOG_NAMES "")
    include("${_file}")
endmacro()
