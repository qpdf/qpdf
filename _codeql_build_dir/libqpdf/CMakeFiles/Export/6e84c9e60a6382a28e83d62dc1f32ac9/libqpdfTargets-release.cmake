#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "qpdf::libqpdf" for configuration "Release"
set_property(TARGET qpdf::libqpdf APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(qpdf::libqpdf PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libqpdf.so.30.3.0"
  IMPORTED_SONAME_RELEASE "libqpdf.so.30"
  )

list(APPEND _cmake_import_check_targets qpdf::libqpdf )
list(APPEND _cmake_import_check_files_for_qpdf::libqpdf "${_IMPORT_PREFIX}/lib/libqpdf.so.30.3.0" )

# Import target "qpdf::libqpdf_static" for configuration "Release"
set_property(TARGET qpdf::libqpdf_static APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(qpdf::libqpdf_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libqpdf.a"
  )

list(APPEND _cmake_import_check_targets qpdf::libqpdf_static )
list(APPEND _cmake_import_check_files_for_qpdf::libqpdf_static "${_IMPORT_PREFIX}/lib/libqpdf.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
