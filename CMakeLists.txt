cmake_minimum_required(VERSION 3.7)

find_package(LLVM REQUIRED CONFIG)
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

add_subdirectory(diffkemp/simpll)
add_subdirectory(diffkemp/llreve/reve)
