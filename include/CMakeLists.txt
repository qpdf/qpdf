set(qpdf_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)

# While globs are not considered best practice, it makes sense for
# installation o header files. When compiling, you specify an entire
# directory, so not doing the same at installation time creates a high
# risk that forgetting to explicitly add a header to an installation
# list would not be detected in CI or at any time until an end user
# tries to build code.
install(DIRECTORY qpdf
  TYPE INCLUDE
  COMPONENT ${COMPONENT_DEV}
  FILES_MATCHING PATTERN "*.h" PATTERN "*.hh")
