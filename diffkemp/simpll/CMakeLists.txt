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
list(REMOVE_ITEM srcs SimpLL.cpp)
file(GLOB passes passes/*.cpp)
file(GLOB library library/*.cpp)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fpic -fvisibility=hidden")

exec_program(llvm-config ARGS --libs irreader passes support OUTPUT_VARIABLE llvm_libs)
exec_program(llvm-config ARGS --system-libs OUTPUT_VARIABLE system_libs)
string(STRIP ${system_libs} system_libs)

add_library(simpll-lib ${srcs} ${passes} ${library})
add_executable(simpll SimpLL.cpp)
set_target_properties(simpll PROPERTIES PREFIX "diffkemp-")
target_link_libraries(simpll simpll-lib ${llvm_libs})
if (NOT ${system_libs} STREQUAL "")
    target_link_libraries(simpll ${system_libs})
endif()

if(SIMPLL_REBUILD_BINDINGS)
add_custom_target(python-ffi ALL DEPENDS _simpll.c)
add_custom_command(OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/_simpll.c"
                   DEPENDS simpll-lib
                   WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                   COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/simpll_build.py")
endif()

install(TARGETS simpll simpll-lib
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
