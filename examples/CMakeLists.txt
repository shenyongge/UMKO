message("current processor is ${CMAKE_SYSTEM_PROCESSOR}")
if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "arm")
    add_subdirectory(aarch64)
endif()

if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
    #add_subdirectory(x86_64)
endif()

add_subdirectory(comm)