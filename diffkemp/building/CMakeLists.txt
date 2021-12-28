find_program(RPYTHON rpython)
set(CC_WRAPPER_SOURCE "${CMAKE_SOURCE_DIR}/diffkemp/building/cc_wrapper.py")
add_custom_target(cc-wrapper-source ALL DEPENDS ${CC_WRAPPER_SOURCE})
install(PROGRAMS ${CC_WRAPPER_SOURCE} RENAME diffkemp-cc-wrapper.py DESTINATION ${CMAKE_INSTALL_BINDIR})
if (NOT RPYTHON)
  message(WARNING "RPython not found, not building compiler wrapper")
else ()
  set(CC_WRAPPER "${CMAKE_BINARY_DIR}/cc_wrapper-c")
  add_custom_target(cc-wrapper ALL DEPENDS ${CC_WRAPPER})
  add_custom_command(OUTPUT ${CC_WRAPPER}
                     DEPENDS ${CC_WRAPPER_SOURCE}
                     COMMAND ${RPYTHON} ${CC_WRAPPER_SOURCE} "--output=${CC_WRAPPER}"
                     WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  install(PROGRAMS ${CC_WRAPPER} RENAME diffkemp-cc-wrapper DESTINATION ${CMAKE_INSTALL_BINDIR})
endif ()
