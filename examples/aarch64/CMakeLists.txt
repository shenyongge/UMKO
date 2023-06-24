
add_library(app10 OBJECT app10.c mylibc.c)
target_compile_options(app10 PUBLIC  "-fno-stack-protector")