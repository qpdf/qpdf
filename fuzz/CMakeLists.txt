# This directory contains support for Google's oss-fuzz project. See
# https://github.com/google/oss-fuzz/tree/master/projects/qpdf

# XXX Update oss-fuzz to handle cmake

set(FUZZERS
  qpdf_fuzzer
  ascii85_fuzzer
  dct_fuzzer
  flate_fuzzer
  hex_fuzzer
  lzw_fuzzer
  pngpredictor_fuzzer
  runlength_fuzzer
  tiffpredictor_fuzzer)

foreach(PROG ${FUZZERS})
  add_executable(${PROG} ${PROG}.cc)
  if(LIB_FUZZING_ENGINE)
    # When running from oss-fuzz, LIB_FUZZING_ENGINE points to a
    # static library that contains main.
    target_link_libraries(${PROG} ${LIB_FUZZING_ENGINE})
  else()
    # When not running from oss-fuzz, supply our own main.
    target_sources(${PROG} standalone_fuzz_target_runner.cc)
  endif()
  target_link_libraries(${PROG} libqpdf_object)
  if(OSS_FUZZ)
    # oss-fuzz wants static executables
    target_link_options(${PROG} -static)
  endif()
endforeach()

# Files from the test suite that are good for seeding the fuzzer.
# Update count for qpdf in @fuzzers qtest/fuzz.test if you change this list.
set(CORPUS_FROM_TEST
  stream-data.pdf
  lin5.pdf
  field-types.pdf
  image-streams-small.pdf
  need-appearances.pdf
  outlines-with-actions.pdf
  outlines-with-old-root-dests.pdf
  page-labels-and-outlines.pdf
  page-labels-num-tree.pdf
  dr-with-indirect-item.pdf
  fuzz-16214.pdf
  issue-99b.pdf
  issue-99.pdf
  issue-100.pdf
  issue-101.pdf
  issue-106.pdf
  issue-117.pdf
  issue-119.pdf
  issue-120.pdf
  issue-141a.pdf
  issue-141b.pdf
  issue-143.pdf
  issue-146.pdf
  issue-147.pdf
  issue-148.pdf
  issue-149.pdf
  issue-150.pdf
  issue-202.pdf
  issue-263.pdf
  issue-335a.pdf
  issue-335b.pdf)

# Any file that qpdf_fuzzer should be tested with can be named
# something.fuzz and dropped into qpdf_extra. Update count for qpdf in
# @fuzzers qtest/fuzz.test if you change this list.
set(CORPUS_OTHER
  15316.fuzz
  15387.fuzz
  15390.fuzz
  15442.fuzz
  15445.fuzz
  15983.fuzz
  16172.fuzz
  16301.fuzz
  16953.fuzz
  18241.fuzz
  18247.fuzz
  23172.fuzz
  23599.fuzz
  23642.fuzz
  23642-mod.fuzz
  26761.fuzz
  26994.fuzz
  27393.fuzz
  28262.fuzz
  30507.fuzz
  37740.fuzz)

set(CORPUS_DIR ${CMAKE_CURRENT_BINARY_DIR}/qpdf_corpus)
execute_process(COMMAND mkdir -p ${CORPUS_DIR})
function(copy_fuzz FROM)
  execute_process(
    COMMAND perl ${CMAKE_CURRENT_SOURCE_DIR}/get_sha1
    INPUT_FILE ${FROM}
    OUTPUT_VARIABLE SHA)
  set(OUT ${CORPUS_DIR}/${SHA})
  add_custom_command(
    OUTPUT ${OUT}
    COMMAND cp ${FROM} ${OUT})
  set(CORPUS_FILE ${OUT} PARENT_SCOPE)
endfunction()

list(APPEND CORPUS_FILES)
foreach(F ${CORPUS_FROM_TEST})
  copy_fuzz(${qpdf_SOURCE_DIR}/qpdf/qtest/qpdf/${F})
  list(APPEND CORPUS_FILES ${CORPUS_FILE})
endforeach()
foreach(F ${CORPUS_OTHER})
  copy_fuzz(${CMAKE_CURRENT_SOURCE_DIR}/qpdf_extra/${F})
  list(APPEND CORPUS_FILES ${CORPUS_FILE})
endforeach()
add_custom_target(fuzz_corpus ALL true
  DEPENDS ${CORPUS_FILES})

add_test(
  NAME fuzz
  COMMAND ${RUN_QTEST}
  --env QPDF_FUZZ_CORPUS=${CORPUS_DIR}
  --top ${qpdf_SOURCE_DIR}
  --bin $<TARGET_FILE_DIR:qpdf_fuzzer>
  --bin $<TARGET_FILE_DIR:qpdf>
  --code ${qpdf_SOURCE_DIR}/fuzz
  --color ${QTEST_COLOR}
  --show-on-failure ${SHOW_FAILED_TEST_OUTPUT})

if(NOT OUT)
  # oss-fuzz sets OUT
  set(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fuzz-install)
endif()
if(OSS_FUZZ)
  install(
    TARGETS ${FUZZERS}
    DESTINATION ${OUT}
    EXCLUDE_FROM_ALL
    COMPONENT fuzz)
  install(
    FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/pdf.dict
    ${CMAKE_CURRENT_SOURCE_DIR}/qpdf_fuzzer.options
    DESTINATION ${OUT}
    EXCLUDE_FROM_ALL
    COMPONENT fuzz)
endif()

## 	  if test -d fuzz/$(OUTPUT_DIR)/$${B}_seed_corpus; then \
## 	    (cd fuzz/$(OUTPUT_DIR)/$${B}_seed_corpus; zip -q -r $(OUT)/$${B}_seed_corpus.zip .); \
## 	  elif test -d fuzz/$${B}_seed_corpus; then \
## 	    (cd fuzz/$${B}_seed_corpus; zip -q -r $(OUT)/$${B}_seed_corpus.zip .); \
## 	  fi; \
## 	done
