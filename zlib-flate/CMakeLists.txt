add_executable(zlib-flate zlib-flate.cc)
target_link_libraries(zlib-flate libqpdf)
set_target_properties(zlib-flate PROPERTIES INSTALL_RPATH "${Base_rpath}")

add_test(
  NAME zlib-flate
  COMMAND ${RUN_QTEST}
  --top ${qpdf_SOURCE_DIR}
  --bin $<TARGET_FILE_DIR:zlib-flate>
  --bin $<TARGET_FILE_DIR:libqpdf>  # for Windows to find DLL
  --code ${qpdf_SOURCE_DIR}/zlib-flate
  --color ${QTEST_COLOR}
  --show-on-failure ${SHOW_FAILED_TEST_OUTPUT})

install(TARGETS zlib-flate
  TYPE RUNTIME
  COMPONENT ${COMPONENT_CLI})
