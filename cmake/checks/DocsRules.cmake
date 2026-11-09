# DocsCheck rules for RunicEngine: Engine/<Group>/<Module>/... needs Engine/<Group>/<Module>/ARCHITECTURE.md, or
# docs/modules/<Module>.md while the module folder has no ARCHITECTURE.md yet. Engine/Programs/ holds executables,
# not modules, and has no module contract.
function(runic_docs_rule path out_module out_contracts)
    set(module "")
    set(contracts "")
    # Two separate if()s: a second MATCHES in the same condition would overwrite CMAKE_MATCH_<n>.
    if(path MATCHES "^Engine/Programs/")
        # executables: no module contract
    elseif(path MATCHES "^Engine/([A-Za-z]+)/([A-Za-z]+)/")
        set(module "${CMAKE_MATCH_2}")
        set(folder "Engine/${CMAKE_MATCH_1}/${CMAKE_MATCH_2}")
        list(APPEND contracts "${folder}/ARCHITECTURE.md")
        if(NOT EXISTS "${ROOT}/${folder}/ARCHITECTURE.md")
            list(APPEND contracts "docs/modules/${module}.md")
        endif()
    endif()
    set(${out_module} "${module}" PARENT_SCOPE)
    set(${out_contracts} "${contracts}" PARENT_SCOPE)
endfunction()
