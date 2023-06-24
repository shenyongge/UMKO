aux_source_directory(. MAIN_SRC)

add_executable(umko ${MAIN_SRC} 
    $<TARGET_OBJECTS:reloc> 
    $<TARGET_OBJECTS:pub> 
    $<TARGET_OBJECTS:mod>)

target_link_options(umko PUBLIC  "-static") 
target_compile_options(umko PUBLIC  "-O2")
