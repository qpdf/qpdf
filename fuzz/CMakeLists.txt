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
  # XXX Not yet
  add_executable(${PROG} ${PROG}.cc standalone_fuzz_target_runner.cc)
  target_link_libraries(${PROG} libqpdf_object)
endforeach()

## DEFAULT_FUZZ_RUNNER := standalone_fuzz_target_runner
## OBJ_DEFAULT_FUZZ := fuzz/$(OUTPUT_DIR)/$(DEFAULT_FUZZ_RUNNER).$(OBJ)
##
## # LIB_FUZZING_ENGINE is overridden by oss-fuzz
## LIB_FUZZING_ENGINE ?= $(OBJ_DEFAULT_FUZZ)
##
## # Depend on OBJ_DEFAULT_FUZZ to ensure that it is always compiled.
## # Don't depend on LIB_FUZZING_ENGINE, which we can't build. When used
## # by oss-fuzz, it will be there.
## $(BINS_fuzz): $(TARGETS_libqpdf) $(OBJ_DEFAULT_FUZZ)
##

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


## # -----
##
## CORPUS_EXTRA := $(foreach F,$(CORPUS_FROM_TEST),qpdf/qtest/qpdf/$F) \
## 	$(CORPUS_OTHER)
## CORPUS_DIR := fuzz/$(OUTPUT_DIR)/qpdf_fuzzer_seed_corpus
##
## .PHONY: fuzz_corpus
## fuzz_corpus:: fuzz/$(OUTPUT_DIR)/fuzz_corpus.stamp
## $(foreach F,$(CORPUS_EXTRA),$(eval \
##   SHA1_$(notdir $(F)) := $(shell perl fuzz/get_sha1 < $F)))
## $(foreach F,$(CORPUS_EXTRA),$(eval \
##   fuzz_corpus:: $(CORPUS_DIR)/$(SHA1_$(notdir $(F)))))
## $(foreach F,$(CORPUS_EXTRA),$(eval \
##   $(CORPUS_DIR)/$(SHA1_$(notdir $(F))): $(F) ; \
## 	mkdir -p $(CORPUS_DIR); \
## 	cp $(F) $(CORPUS_DIR)/$(SHA1_$(notdir $(F)))))
##
## fuzz/$(OUTPUT_DIR)/fuzz_corpus.stamp: $(CORPUS_EXTRA)
## 	mkdir -p $(CORPUS_DIR)
## 	touch $@
##
## ifeq ($(suffix $(LIB_FUZZING_ENGINE)),.$(OBJ))
##   FUZZ_as_obj := $(LIB_FUZZING_ENGINE)
##   FUZZ_as_lib :=
## else
##   FUZZ_as_obj :=
##   FUZZ_as_lib := $(LIB_FUZZING_ENGINE)
## endif
##
## $(foreach B,$(FUZZERS),$(eval \
##   fuzz/$(OUTPUT_DIR)/$(call binname,$(B)): $(OBJS_$(B)) ; \
## 	$(call makebin,$(OBJS_$(B)) $(FUZZ_as_obj),$$@,$(LDFLAGS_libqpdf) $(LDFLAGS),$(FUZZ_as_lib) $(LIBS_libqpdf) $(LIBS))))
##
## ifeq ($(OSS_FUZZ),1)
##
## # Build fuzzers linked with static libraries and installed into a
## # location provided by oss-fuzz. This is specifically to support the
## # oss-fuzz project. These rules won't on systems that don't allow main
## # to be in a library or don't name their libraries libsomething.a.
##
## STATIC_BINS_fuzz := $(foreach B,$(FUZZERS),fuzz/$(OUTPUT_DIR)/static/$(call binname,$(B)))
## $(STATIC_BINS_fuzz): $(TARGETS_libqpdf) $(OBJ_DEFAULT_FUZZ)
##
## # OUT is provided in the oss-fuzz environment
## OUT ?= $(CURDIR)/fuzz/$(OUTPUT_DIR)/fuzz-install
##
## # These are not fully static, but they statically link with qpdf and
## # our external dependencies other than system libraries.
## $(foreach B,$(FUZZERS),$(eval \
##   fuzz/$(OUTPUT_DIR)/static/$(call binname,$(B)): $(OBJS_$(B)) ; \
## 	$(call makebin,$(OBJS_$(B)),$$@,$(LDFLAGS_libqpdf) $(LDFLAGS),$(LIB_FUZZING_ENGINE) $(patsubst -l%,-l:lib%.a,$(LIBS_libqpdf) $(LIBS)))))
##
## # The install_fuzz target is used by build.sh in oss-fuzz's qpdf project.
## install_fuzz: $(STATIC_BINS_fuzz)
## 	mkdir -p $(OUT)
## 	cp fuzz/pdf.dict $(STATIC_BINS_fuzz) $(OUT)/
## 	for B in $(FUZZERS); do \
## 	  if test -f fuzz/$${B}.options; then \
## 	    cp fuzz/$${B}.options $(OUT)/$${B}.options; \
## 	  fi; \
## 	  if test -d fuzz/$(OUTPUT_DIR)/$${B}_seed_corpus; then \
## 	    (cd fuzz/$(OUTPUT_DIR)/$${B}_seed_corpus; zip -q -r $(OUT)/$${B}_seed_corpus.zip .); \
## 	  elif test -d fuzz/$${B}_seed_corpus; then \
## 	    (cd fuzz/$${B}_seed_corpus; zip -q -r $(OUT)/$${B}_seed_corpus.zip .); \
## 	  fi; \
## 	done
##
## endif # OSS_FUZZ
