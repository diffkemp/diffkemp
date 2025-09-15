# Create symlinks to a correct version of FunctionComparator based on LLVM version
execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
  llvm-lib/${LLVM_VERSION_MAJOR}/FunctionComparator.h
  ${CMAKE_CURRENT_SOURCE_DIR}/FunctionComparator.h)
execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
  llvm-lib/${LLVM_VERSION_MAJOR}/FunctionComparator.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/FunctionComparator.cpp)

# Add color coding to compiler diagnostics
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options (-fcolor-diagnostics)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options (-fdiagnostics-color=always)
endif()

file(GLOB srcs *.cpp)
get_filename_component(simpll_main_path ${CMAKE_CURRENT_SOURCE_DIR}/SimpLL.cpp ABSOLUTE)
list(REMOVE_ITEM srcs ${simpll_main_path})
file(GLOB passes passes/*.cpp)
file(GLOB library library/*.cpp)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fpic -Wall -Wextra")

if (${SANITIZE_ADDRESS})
  add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address -fno-omit-frame-pointer)
endif()

execute_process(COMMAND llvm-config --libs irreader passes support
                OUTPUT_VARIABLE llvm_libs
                OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND llvm-config --system-libs
                OUTPUT_VARIABLE system_libs
                OUTPUT_STRIP_TRAILING_WHITESPACE)

# Try to find system-wide Z3 (e.g. local build)
find_package(Z3 CONFIG)
if (NOT ${Z3_FOUND})
    # Use our FindZ3.cmake (e.g. nix build)
    find_package(Z3 REQUIRED MODULE)
endif()

add_library(simpll-lib ${srcs} ${passes} ${library})
target_include_directories(simpll-lib PRIVATE ${Z3_CXX_INCLUDE_DIRS})
target_link_libraries(simpll-lib PRIVATE ${Z3_LIBRARIES} -lstdc++ -lgcc_s)
target_compile_options(simpll-lib PRIVATE ${Z3_COMPONENT_CXX_FLAGS})
add_executable(simpll SimpLL.cpp)
set_target_properties(simpll PROPERTIES PREFIX "diffkemp-")
target_link_libraries(simpll simpll-lib ${llvm_libs})
if (NOT ${system_libs} STREQUAL "")
    target_link_libraries(simpll ${system_libs})
endif()

if (NOT DEFINED $ENV{SIMPLL_BUILD_DIR})
  message(STATUS "Setting SIMPLL_BUILD_DIR to ${CMAKE_BINARY_DIR}")
  set(simpll_build_dir ${CMAKE_BINARY_DIR})
else ()
  set(simpll_build_dir $ENV{SIMPLL_BUILD_DIR})
endif ()

add_custom_target(python-ffi ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/_simpll.c")
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/_simpll.c"
  DEPENDS simpll-lib "simpll_build.py"
  COMMENT "Building CFFI python bindings for SimpLL"
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/simpll_build.py .
  && ${CMAKE_COMMAND} -E env PATH=$ENV{PATH} "SIMPLL_BUILD_DIR=${simpll_build_dir}" python3 simpll_build.py)

if(SIMPLL_INSTALL_BINARY)
install(TARGETS simpll
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
install(TARGETS simpll-lib
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
