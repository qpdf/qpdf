# This directory contains support for Google's oss-fuzz project. See
# https://github.com/google/oss-fuzz/tree/master/projects/qpdf

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

# The oss-fuzz project provides LIB_FUZZING_ENGINE and OUT environment
# variables. For local testing, provide values if not set.
set(LIB_FUZZING_ENGINE $ENV{LIB_FUZZING_ENGINE})
if(NOT LIB_FUZZING_ENGINE)
  # When running from oss-fuzz, LIB_FUZZING_ENGINE points to a
  # static library that contains main.
  add_library(standalone_fuzzer STATIC standalone_fuzz_target_runner.cc)
  set(LIB_FUZZING_ENGINE standalone_fuzzer)
endif()
set(FUZZ_OUT $ENV{OUT})
if(NOT FUZZ_OUT)
  set(FUZZ_OUT ${CMAKE_CURRENT_BINARY_DIR}/fuzz-install)
endif()

foreach(PROG ${FUZZERS})
  add_executable(${PROG} ${PROG}.cc)
  target_link_libraries(${PROG} ${LIB_FUZZING_ENGINE})
  target_link_libraries(${PROG} libqpdf_object)
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
add_custom_target(qpdf_corpus ALL true
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

if(OSS_FUZZ)
  list(APPEND SEED_CORPUS_ZIPS)
  foreach(F ${FUZZERS})
    if(F STREQUAL qpdf_fuzzer)
      set(SEED_DIR ${CORPUS_DIR})
    else()
      set(SEED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${F}_seed_corpus)
    endif()
    set(SEED_ZIP ${CMAKE_CURRENT_BINARY_DIR}/${F}_seed_corpus.zip)
    add_custom_command(OUTPUT ${SEED_ZIP}
      COMMAND zip -q -r ${SEED_ZIP} .
      WORKING_DIRECTORY ${SEED_DIR})
    list(APPEND SEED_CORPUS_ZIPS ${SEED_ZIP})
  endforeach()
  add_custom_target(seed_corpus_zips ALL DEPENDS ${SEED_CORPUS_ZIPS})
  add_dependencies(seed_corpus_zips qpdf_corpus)
  add_custom_target(fuzzers)
  add_dependencies(fuzzers ${FUZZERS} seed_corpus_zips)
  install(
    TARGETS ${FUZZERS}
    DESTINATION ${FUZZ_OUT}
    EXCLUDE_FROM_ALL
    COMPONENT fuzz)
  install(
    FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/pdf.dict
    ${CMAKE_CURRENT_SOURCE_DIR}/qpdf_fuzzer.options
    ${SEED_CORPUS_ZIPS}
    DESTINATION ${FUZZ_OUT}
    EXCLUDE_FROM_ALL
    COMPONENT fuzz)
endif()
