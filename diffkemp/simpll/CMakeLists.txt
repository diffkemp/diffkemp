file(GLOB srcs *.cpp)
file(GLOB passes passes/*.cpp)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-fno-rtti")

exec_program(llvm-config ARGS --libs irreader passes support OUTPUT_VARIABLE llvm_libs)
add_executable(simpll ${srcs} ${passes})
target_link_libraries(simpll ${llvm_libs})
