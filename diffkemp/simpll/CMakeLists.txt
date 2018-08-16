file(GLOB srcs *.cpp)
file(GLOB passes passes/*.cpp)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-fno-rtti")

add_executable(simpll ${srcs} ${passes})
llvm_map_components_to_libnames(llvm_libs
  irreader
  passes
  support)
target_link_libraries(simpll ${llvm_libs})
