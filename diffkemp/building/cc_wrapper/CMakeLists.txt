# Define the sources for the new C++ compiler wrapper
set(CC_WRAPPER_SOURCES
    "${CMAKE_SOURCE_DIR}/diffkemp/building/cc_wrapper/cc_wrapper.cpp"
    "${CMAKE_SOURCE_DIR}/diffkemp/building/cc_wrapper/cc_wrapper_utils.cpp"
    "${CMAKE_SOURCE_DIR}/diffkemp/building/cc_wrapper/cc_wrapper_utils.h"
)

add_executable(cc_wrapper-c ${CC_WRAPPER_SOURCES})
target_link_libraries(cc_wrapper-c
    PRIVATE
    stdc++
    stdc++fs
    gcc_s
)
set_target_properties(cc_wrapper-c
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/diffkemp/building/cc_wrapper"
)
install(TARGETS cc_wrapper-c
        RENAME diffkemp-cc-wrapper
        DESTINATION ${CMAKE_INSTALL_BINDIR}
)
