# This directory contains support for Google's oss-fuzz project. See
# https://github.com/google/oss-fuzz/tree/master/projects/qpdf

set(FUZZERS
  qpdf_fuzzer
  qpdf_crypt_fuzzer
  qpdf_crypt_insecure_fuzzer
  qpdf_lin_fuzzer
  qpdf_pages_fuzzer
  qpdf_outlines_fuzzer
  ascii85_fuzzer
  dct_fuzzer
  flate_fuzzer
  hex_fuzzer
  json_fuzzer
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
  target_include_directories(
    standalone_fuzzer PRIVATE ${qpdf_SOURCE_DIR}/include)
  set(LIB_FUZZING_ENGINE standalone_fuzzer)
endif()
set(FUZZ_OUT $ENV{OUT})
if(NOT FUZZ_OUT)
  set(FUZZ_OUT ${CMAKE_CURRENT_BINARY_DIR}/fuzz-install)
endif()

if(OSS_FUZZ)
  # We need to link jpeg and zlib statically for oss-fuzz. Construct
  # our own object library without the external dependencies and add
  # what we need.
  add_library(libqpdf_fuzz STATIC $<TARGET_OBJECTS:libqpdf_object>)
  target_link_libraries(libqpdf_fuzz INTERFACE libjpeg.a libz.a)
  target_include_directories(libqpdf_fuzz
    PUBLIC
    ${JPEG_INCLUDE}
    ${qpdf_SOURCE_DIR}/include
    ${qpdf_SOURCE_DIR}/libqpdf)
else()
  add_library(libqpdf_fuzz ALIAS libqpdf_object)
endif()

foreach(PROG ${FUZZERS})
  add_executable(${PROG} ${PROG}.cc)
  target_link_libraries(${PROG} ${LIB_FUZZING_ENGINE})
  target_link_libraries(${PROG} libqpdf_fuzz)
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
  17630.fuzz
  17630a.fuzz
  17630b.fuzz
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
  37740.fuzz
  57639.fuzz
  65681.fuzz
  65773.fuzz
  65777.fuzz
  68374.fuzz
  68377.fuzz
  68668.fuzz
  68915.fuzz
  69857.fuzz
  69913.fuzz
  69969.fuzz
  69977.fuzz
  69977a.fuzz
  69977b.fuzz
  69977c.fuzz
  69977d.fuzz
  69977e.fuzz
  70055.fuzz
  70245.fuzz
  70306.fuzz
  70306a.fuzz
  70306b.fuzz
  71624.fuzz
)

set(CORPUS_DIR ${CMAKE_CURRENT_BINARY_DIR}/qpdf_corpus)
file(MAKE_DIRECTORY ${CORPUS_DIR})
function(copy_fuzz FROM)
  file(SHA1 ${FROM} SHA)
  set(OUT ${CORPUS_DIR}/${SHA})
  add_custom_command(
    OUTPUT ${OUT}
    COMMAND ${COPY_COMMAND} $<SHELL_PATH:${FROM}> $<SHELL_PATH:${OUT}>)
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
add_custom_target(qpdf_corpus ALL
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
    if((F STRGREATER qpdf_) AND (F STRLESS qpdg))
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
