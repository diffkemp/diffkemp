cmake_policy(SET CMP0048 NEW)
cmake_minimum_required(VERSION 3.5)
project(diffkemp
    VERSION 0.4.0)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
add_definitions(${LLVM_DEFINITIONS})
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
set(CMAKE_CXX_STANDARD 17)

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

execute_process(COMMAND
  ${CMAKE_CURRENT_SOURCE_DIR}/tools/simpll_clean.sh
  ${CMAKE_CURRENT_SOURCE_DIR})

option(BUILD_VIEWER "Install packages for result viewer and create production build" OFF)
if (${BUILD_VIEWER})
  find_program(NPM npm REQUIRED)
  message(STATUS "Found npm ${NPM}")
  message(STATUS "Installing packages for result viewer")
  execute_process(
    COMMAND ${NPM} install
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/view
  )
  message(STATUS "Creating production build of result viewer")
  execute_process(
    COMMAND ${NPM} run build
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/view
  )
  install(DIRECTORY view/build/ DESTINATION /var/lib/diffkemp/view
    # Setting 644 for files.
    FILE_PERMISSIONS OWNER_READ OWNER_WRITE
                     GROUP_READ
                     WORLD_READ
    # Setting 711 for directories to make it possible 
    # to access files/folders in the directory. 
    DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                          GROUP_EXECUTE
                          WORLD_EXECUTE
    # Do not install files describing results of comparison.
    PATTERN "diffkemp-out.yaml" EXCLUDE
    PATTERN "src" EXCLUDE
    PATTERN "diffs" EXCLUDE
  ) # install - copy `build` dir
  # Creating `src`, `diffs` directories
  # and setting the permissions to 777 to make able to copy there files and remove them.
  install(DIRECTORY DESTINATION /var/lib/diffkemp/view/src
    DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                          GROUP_READ GROUP_WRITE GROUP_EXECUTE
                          WORLD_READ WORLD_WRITE WORLD_EXECUTE
  ) # install src dir
  install(DIRECTORY DESTINATION /var/lib/diffkemp/view/diffs
    DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                          GROUP_READ GROUP_WRITE GROUP_EXECUTE
                          WORLD_READ WORLD_WRITE WORLD_EXECUTE
  ) # install diffs dir
  # Creating diffkemp-out.yaml
  # and setting its permission to 666 to make able edit its content by `diffkemp view`.
  install(CODE "file(TOUCH /var/lib/diffkemp/view/diffkemp-out.yaml)")
  install(CODE "file(CHMOD /var/lib/diffkemp/view/diffkemp-out.yaml
                           PERMISSIONS OWNER_READ OWNER_WRITE
                                       GROUP_READ GROUP_WRITE
                                       WORLD_READ WORLD_WRITE
  )") # install - file chmod
endif () # ${BUILD_VIEWER}

add_subdirectory(tests/unit_tests/simpll)
