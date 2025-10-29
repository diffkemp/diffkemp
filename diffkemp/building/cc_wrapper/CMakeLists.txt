# Define the sources for the new C++ compiler wrapper
set(CC_WRAPPER_SOURCES
    "cc_wrapper.cpp"
    "cc_wrapper_utils.cpp"
)

add_executable(diffkemp-cc-wrapper ${CC_WRAPPER_SOURCES})
target_link_libraries(diffkemp-cc-wrapper
    PRIVATE
    stdc++
    stdc++fs
    gcc_s
)
install(TARGETS diffkemp-cc-wrapper
        DESTINATION ${CMAKE_INSTALL_BINDIR}
)
