# This directory is called compare-for-test rather than
# qpdf-test-compare to make shell completion easier.
add_executable(qpdf-test-compare qpdf-test-compare.cc)
target_link_libraries(qpdf-test-compare libqpdf)

add_test(
  NAME compare-for-test
  COMMAND ${RUN_QTEST}
  --top ${qpdf_SOURCE_DIR}
  --bin $<TARGET_FILE_DIR:qpdf-test-compare>
  --bin $<TARGET_FILE_DIR:libqpdf>  # for Windows to find DLL
  --code ${qpdf_SOURCE_DIR}/compare-for-test
  --color ${QTEST_COLOR}
  --show-on-failure ${SHOW_FAILED_TEST_OUTPUT}
  --tc "${qpdf_SOURCE_DIR}/compare-for-test/*.cc")
