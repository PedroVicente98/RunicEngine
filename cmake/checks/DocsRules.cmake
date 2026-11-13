# DocsCheck rules for RunicEngine: a code change in Engine/<Group>/<Module>/ needs that module's ARCHITECTURE.md (its
# contract and package status; the module's first package writes it). Engine/Programs/ holds executables, not
# modules, and has no module contract.
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
    endif()
    set(${out_module} "${module}" PARENT_SCOPE)
    set(${out_contracts} "${contracts}" PARENT_SCOPE)
endfunction()
