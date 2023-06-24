
aux_source_directory(. RELOC_SRC)
add_library(reloc OBJECT ${RELOC_SRC})

#target_link_libraries(tutorial PRIVATE elfio::elfio)
