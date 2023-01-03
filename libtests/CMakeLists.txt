set(TEST_PROGRAMS
  cxx11
  aes
  arg_parser
  ascii85
  base64
  bits
  buffer
  closed_file_input_source
  concatenate
  dct_compress
  dct_uncompress
  flate
  hex
  input_source
  json
  json_handler
  json_parse
  logger
  lzw
  main_from_wmain
  main_from_wmain_12
  matrix
  md5
  nntree
  numrange
  pdf_version
  pl_function
  pointer_holder
  predictors
  qintc
  qutil
  qutil_12
  random
  rc4
  runlength
  sha2
  sparse_array)
set(TEST_C_PROGRAMS
  logger_c)

foreach(PROG ${TEST_PROGRAMS})
  add_executable(${PROG} ${PROG}.cc)
  target_link_libraries(${PROG} libqpdf_object)
endforeach()
foreach(PROG ${TEST_C_PROGRAMS})
  add_executable(${PROG} ${PROG}.c)
  target_link_libraries(${PROG} libqpdf_object)
  set_property(TARGET ${PROG} PROPERTY LINKER_LANGUAGE CXX)
endforeach()

# Since libtests link with the object library and don't use the DLL,
# we don't need to (and shouldn't) add the libqpdf target directory to
# the path for libtests.
add_test(
  NAME libtests
  COMMAND ${RUN_QTEST}
  --top ${qpdf_SOURCE_DIR}
  --bin $<TARGET_FILE_DIR:qutil>
  --bin $<TARGET_FILE_DIR:qpdf>
  --code ${qpdf_SOURCE_DIR}/libtests
  --color ${QTEST_COLOR}
  --show-on-failure ${SHOW_FAILED_TEST_OUTPUT}
  --tc "${qpdf_SOURCE_DIR}/libtests/*.cc"
  --tc "${qpdf_SOURCE_DIR}/libqpdf/*.cc"
  --tc "${qpdf_SOURCE_DIR}/libqpdf/qpdf/bits_functions.hh")
