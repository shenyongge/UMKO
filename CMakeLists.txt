cmake_minimum_required (VERSION 3.0)
project (umko)


include_directories(3rdpart/ELFIO include)

add_subdirectory(arch)
add_subdirectory(module)
add_subdirectory(pub)
add_subdirectory(main)
add_subdirectory(examples)
