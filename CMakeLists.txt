cmake_minimum_required(VERSION 3.7)

find_package(LLVM 5.0 REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

add_subdirectory(diffkemp/simpll)

option(BUILD_LLREVE "Build the LLReve tool for semantic comparison" OFF)
if (${BUILD_LLREVE})
  if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/llreve)
    execute_process(COMMAND
      git clone
        -b diffkemp
        https://github.com/viktormalik/llreve.git
        ${CMAKE_CURRENT_SOURCE_DIR}/llreve)
  endif ()
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/llreve/reve)
endif ()
