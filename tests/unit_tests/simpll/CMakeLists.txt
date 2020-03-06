# Download and unpack googletest at configure time
configure_file(CMakeLists.txt.in googletest-download/CMakeLists.txt)
execute_process(COMMAND "${CMAKE_COMMAND}" -G "${CMAKE_GENERATOR}" .
                WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/googletest-download")
execute_process(COMMAND "${CMAKE_COMMAND}" --build .
                WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/googletest-download")

# Add googletest directly to our build. This adds the following targets:
# gtest, gtest_main, gmock and gmock_main
add_subdirectory("${CMAKE_CURRENT_BINARY_DIR}/googletest-src"
                 "${CMAKE_CURRENT_BINARY_DIR}/googletest-build")

# Disable RTTI to link with SimpLL correctly.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")

enable_testing()

include_directories(${CMAKE_SOURCE_DIR}/diffkemp/simpll)
add_executable(runTests SimpLLTest.cpp DifferentialFunctionComparatorTest.cpp)
set_target_properties(runTests
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
exec_program(llvm-config ARGS --libs irreader passes support OUTPUT_VARIABLE llvm_libs)
target_link_libraries(runTests gtest simpll-lib ${llvm_libs})
