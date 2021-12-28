cmake_policy(SET CMP0048 NEW)
cmake_minimum_required(VERSION 3.5)
project(diffkemp
    VERSION 0.3.0)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
add_definitions(${LLVM_DEFINITIONS})
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
set(CMAKE_CXX_STANDARD 14)

include(GNUInstallDirs)

option(SANITIZE_ADDRESS "Enable address sanitizer" OFF)

add_subdirectory(diffkemp/simpll)
add_subdirectory(diffkemp/building)

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

add_subdirectory(tests/unit_tests/simpll)
