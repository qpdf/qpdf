set(EXAMPLE_CXX_PROGRAMS
  pdf-attach-file
  pdf-bookmarks
  pdf-count-strings
  pdf-create
  pdf-custom-filter
  pdf-double-page-size
  pdf-filter-tokens
  pdf-invert-images
  pdf-mod-info
  pdf-name-number-tree
  pdf-npages
  pdf-overlay-page
  pdf-parse-content
  pdf-set-form-values
  pdf-split-pages
  qpdf-job)
set(EXAMPLE_C_PROGRAMS
  pdf-c-objects
  pdf-linearize
  qpdfjob-c)

foreach(PROG ${EXAMPLE_CXX_PROGRAMS})
  add_executable(${PROG} ${PROG}.cc)
  target_link_libraries(${PROG} libqpdf)
endforeach()
foreach(PROG ${EXAMPLE_C_PROGRAMS})
  add_executable(${PROG} ${PROG}.c)
  target_link_libraries(${PROG} libqpdf)
  set_property(TARGET ${PROG} PROPERTY LINKER_LANGUAGE CXX)
endforeach()
target_include_directories(pdf-create PRIVATE ${JPEG_INCLUDE})

add_test(
  NAME examples
  COMMAND ${RUN_QTEST}
  --top ${qpdf_SOURCE_DIR}
  --bin $<TARGET_FILE_DIR:pdf-create>
  --bin $<TARGET_FILE_DIR:qpdf>
  --bin $<TARGET_FILE_DIR:libqpdf>  # for Windows to find DLL
  --code ${qpdf_SOURCE_DIR}/examples
  --color ${QTEST_COLOR}
  --show-on-failure ${SHOW_FAILED_TEST_OUTPUT}
  --tc "${qpdf_SOURCE_DIR}/examples/*.cc"
  --tc "${qpdf_SOURCE_DIR}/examples/*.c")

file(GLOB EXAMPLES_SRC "*.c" "*.cc")
install(FILES ${EXAMPLES_SRC}
  DESTINATION ${CMAKE_INSTALL_DOCDIR}/examples
  COMPONENT ${COMPONENT_DOC})
