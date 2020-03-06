# Create symlinks to a correct version of FunctionComparator based on LLVM version
execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
  llvm-lib/${LLVM_VERSION_MAJOR}/FunctionComparator.h
  ${CMAKE_CURRENT_SOURCE_DIR}/FunctionComparator.h)
execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
  llvm-lib/${LLVM_VERSION_MAJOR}/FunctionComparator.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/FunctionComparator.cpp)

file(GLOB srcs *.cpp)
list(REMOVE_ITEM srcs SimpLL.cpp)
file(GLOB passes passes/*.cpp)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")

exec_program(llvm-config ARGS --libs irreader passes support OUTPUT_VARIABLE llvm_libs)
add_library(simpll-lib ${srcs} ${passes})
add_executable(simpll SimpLL.cpp)
set_target_properties(simpll PROPERTIES PREFIX "diffkemp-")
target_link_libraries(simpll simpll-lib ${llvm_libs})

install(TARGETS simpll RUNTIME DESTINATION bin)
