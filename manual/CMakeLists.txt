# There is a FindLATEX module, but it does some things we don't care
# about and doesn't do everything we need.
find_program(LATEX latex)
find_program(PDFLATEX pdflatex)
find_program(LATEXMK latexmk)
find_program(SPHINX sphinx-build)

# *** NOTE: Never check BUILD_DOC_PDF if BUILD_DOC is not set. See
# *** comments in top-level CMakeLists.txt.

if(BUILD_DOC)
  if(SPHINX STREQUAL SPHINX-NOTFOUND)
    message(FATAL_ERROR "sphinx-build is required for building documentation")
  endif()
  if(BUILD_DOC_PDF AND
      ((LATEX STREQUAL LATEX-NOTFOUND) OR
        (LATEXMK STREQUAL LATEXMK-NOTFOUND) OR
        (PDFLATEX STREQUAL PDFLATEX-NOTFOUND)))
    message(FATAL_ERROR
      "latex, latexmk, and pdflatex are required to build PDF documentation")
  endif()
endif()

set(MANUAL_SRC ${qpdf_SOURCE_DIR}/manual)
foreach(F fix-qdf.1 zlib-flate.1)
  configure_file(
    ${MANUAL_SRC}/${F}.in
    ${CMAKE_CURRENT_BINARY_DIR}/${F}
    NEWLINE_STYLE UNIX)
endforeach()

SET(MANUAL_DEPS
  conf.py
  _ext/qpdf.py
  acknowledgement.rst
  cli.rst
  design.rst
  download.rst
  encryption.rst
  index.rst
  installation.rst
  json.rst
  contributing.rst
  library.rst
  license.rst
  linearization.rst
  object-streams.rst
  overview.rst
  packaging.rst
  qdf.rst
  qpdf-job.rst
  release-notes.rst
  weak-crypto.rst)

# Prevent targets that run ${SPHINX} from running in parallel by using
# dependencies. This avoids clashes in temporary files that cause the
# build to fail with the error "_pickle.UnpicklingError: pickle data
# was truncated". It would be better if we could use order-only
# dependencies like gnu make to make it possible to build the targets
# independently.
set(DOC_HTML_OUTPUT html/index.html)
set(DOC_SINGLEHTML_OUTPUT singlehtml/index.html)
set(DOC_PDF_OUTPUT latex/qpdf.pdf)
if(BUILD_DOC)
  add_custom_command(OUTPUT ${DOC_HTML_OUTPUT}
    COMMAND ${SPHINX} -M html ${MANUAL_SRC} ${CMAKE_CURRENT_BINARY_DIR} -W
    COMMAND touch ${DOC_HTML_OUTPUT}
    DEPENDS ${MANUAL_DEPS})
  add_custom_command(OUTPUT ${DOC_SINGLEHTML_OUTPUT}
    COMMAND ${SPHINX} -M singlehtml ${MANUAL_SRC} ${CMAKE_CURRENT_BINARY_DIR} -W
    COMMAND touch ${DOC_SINGLEHTML_OUTPUT}
    DEPENDS ${MANUAL_DEPS})
  add_custom_command(OUTPUT ${DOC_PDF_OUTPUT}
    COMMAND ${SPHINX} -M latexpdf ${MANUAL_SRC} ${CMAKE_CURRENT_BINARY_DIR} -W
    COMMAND touch ${DOC_PDF_OUTPUT}
    DEPENDS ${MANUAL_DEPS})
  add_custom_target(doc_html DEPENDS ${DOC_HTML_OUTPUT})
  add_custom_target(doc_singlehtml DEPENDS ${DOC_SINGLEHTML_OUTPUT})
  add_dependencies(doc_singlehtml doc_html)
  add_custom_target(doc_pdf DEPENDS ${DOC_PDF_OUTPUT})
  add_custom_target(doc ALL)
  if(BUILD_DOC_PDF)
    add_dependencies(doc doc_pdf)
    if(BUILD_DOC_HTML)
      add_dependencies(doc_pdf doc_singlehtml)
    endif()
  elseif(BUILD_DOC_HTML)
    add_dependencies(doc doc_singlehtml)
  endif()
  if(GENERATE_AUTO_JOB)
    add_dependencies(doc auto_job_files)
  endif()

  if(BUILD_DOC_DIST)
    set(DOC_DIST_HTML doc-dist/manual-html)
    set(DOC_DIST_SINGLEHTML doc-dist/manual-single-page-html)
    set(DOC_DIST_PDF doc-dist/qpdf-manual.pdf)
    add_custom_command(
      OUTPUT
      ${DOC_DIST_HTML}
      ${DOC_DIST_SINGLEHTML}
      ${DOC_DIST_PDF}
      COMMAND rm -rf doc-dist
      COMMAND mkdir -p doc-dist
      COMMAND cp -r html ${DOC_DIST_HTML}
      COMMAND cp -r singlehtml ${DOC_DIST_SINGLEHTML}
      COMMAND cp -r ${DOC_PDF_OUTPUT} ${DOC_DIST_PDF}
      DEPENDS ${DOC_HTML_OUTPUT} ${DOC_SINGLEHTML_OUTPUT} ${DOC_PDF_OUTPUT})
    add_custom_target(doc_dist ALL
      DEPENDS ${DOC_DIST_HTML} ${DOC_DIST_SINGLEHTML} ${DOC_DIST_PDF})
    add_dependencies(doc_dist doc)
  endif()
endif()

# INSTALL_MANUAL is not dependent on building doc -- we sometimes drop in
# pre-built doc when creating distributions.
if(INSTALL_MANUAL)
  install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doc-dist/
    TYPE DOC
    COMPONENT ${COMPONENT_DOC})
else()
  install(FILES ${qpdf_SOURCE_DIR}/README-doc.txt
    TYPE DOC
    COMPONENT ${COMPONENT_DOC})
endif()

if(NOT WIN32)
  # There's no reason to install manual pages in a Windows
  # environment, especially when all they do is refer people to the
  # manual.
  install(FILES
    ${qpdf_SOURCE_DIR}/manual/qpdf.1
    ${CMAKE_CURRENT_BINARY_DIR}/fix-qdf.1
    ${CMAKE_CURRENT_BINARY_DIR}/zlib-flate.1
    DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
    COMPONENT ${COMPONENT_DOC})
endif()
