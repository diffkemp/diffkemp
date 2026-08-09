set(WRAPPER_DIR "${CMAKE_SOURCE_DIR}/diffkemp/building/cc_wrapper")

enable_testing()

add_executable(ccWrapperTests
CcWrapperUtilsTest.cpp
"${WRAPPER_DIR}/cc_wrapper_utils.cpp"
)

target_include_directories(ccWrapperTests PRIVATE ${WRAPPER_DIR})

target_link_libraries(ccWrapperTests gtest gtest_main
                      pthread stdc++ gcc_s)

include(GoogleTest)
gtest_discover_tests(ccWrapperTests)
