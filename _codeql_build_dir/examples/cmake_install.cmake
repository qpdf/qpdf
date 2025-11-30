# Install script for directory: /home/runner/work/qpdf/qpdf/examples

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "examples" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/doc/qpdf/examples" TYPE FILE FILES
    "/home/runner/work/qpdf/qpdf/examples/extend-c-api-impl.cc"
    "/home/runner/work/qpdf/qpdf/examples/extend-c-api.c"
    "/home/runner/work/qpdf/qpdf/examples/extend-c-api.h"
    "/home/runner/work/qpdf/qpdf/examples/pdf-attach-file.cc"
    "/home/runner/work/qpdf/qpdf/examples/pdf-bookmarks.cc"
    "/home/runner/work/qpdf/qpdf/examples/pdf-c-objects.c"
    "/home/runner/work/qpdf/qpdf/examples/pdf-count-strings.cc"
    "/home/runner/work/qpdf/qpdf/examples/pdf-create.cc"
    "/home/runner/work/qpdf/qpdf/examples/pdf-custom-filter.cc"
    "/home/runner/work/qpdf/qpdf/examples/pdf-double-page-size.cc"
    "/home/runner/work/qpdf/qpdf/examples/pdf-filter-tokens.cc"
    "/home/runner/work/qpdf/qpdf/examples/pdf-invert-images.cc"
    "/home/runner/work/qpdf/qpdf/examples/pdf-linearize.c"
    "/home/runner/work/qpdf/qpdf/examples/pdf-mod-info.cc"
    "/home/runner/work/qpdf/qpdf/examples/pdf-name-number-tree.cc"
    "/home/runner/work/qpdf/qpdf/examples/pdf-npages.cc"
    "/home/runner/work/qpdf/qpdf/examples/pdf-overlay-page.cc"
    "/home/runner/work/qpdf/qpdf/examples/pdf-parse-content.cc"
    "/home/runner/work/qpdf/qpdf/examples/pdf-set-form-values.cc"
    "/home/runner/work/qpdf/qpdf/examples/pdf-split-pages.cc"
    "/home/runner/work/qpdf/qpdf/examples/qpdf-job.cc"
    "/home/runner/work/qpdf/qpdf/examples/qpdfjob-c-save-attachment.c"
    "/home/runner/work/qpdf/qpdf/examples/qpdfjob-c.c"
    "/home/runner/work/qpdf/qpdf/examples/qpdfjob-remove-annotations.cc"
    "/home/runner/work/qpdf/qpdf/examples/qpdfjob-save-attachment.cc"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/home/runner/work/qpdf/qpdf/_codeql_build_dir/examples/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
