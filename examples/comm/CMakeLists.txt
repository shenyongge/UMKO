
add_library(app00 OBJECT app00.cpp)

#target_compile_options(app00 PUBLIC -fstack-protector-strong -fno-pic  -mcmodel=large )
target_compile_options(app00 PUBLIC  -fno-stack-protector )