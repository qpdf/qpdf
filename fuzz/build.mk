# This directory contains support for Google's oss-fuzz project. See
# https://github.com/google/oss-fuzz/tree/master/projects/qpdf

FUZZERS = \
    qpdf_fuzzer \
    ascii85_fuzzer \
    dct_fuzzer \
    flate_fuzzer \
    hex_fuzzer \
    lzw_fuzzer \
    pngpredictor_fuzzer \
    runlength_fuzzer \
    tiffpredictor_fuzzer

DEFAULT_FUZZ_RUNNER := standalone_fuzz_target_runner
OBJ_DEFAULT_FUZZ := fuzz/$(OUTPUT_DIR)/$(DEFAULT_FUZZ_RUNNER).$(OBJ)

BINS_fuzz = $(foreach B,$(FUZZERS),fuzz/$(OUTPUT_DIR)/$(call binname,$(B)))
TARGETS_fuzz = $(OBJ_DEFAULT_FUZZ) $(BINS_fuzz) fuzz_corpus

# Fuzzers test private classes too, so we need libqpdf in the include path
INCLUDES_fuzz = include libqpdf

# LIB_FUZZING_ENGINE is overridden by oss-fuzz
LIB_FUZZING_ENGINE ?= $(OBJ_DEFAULT_FUZZ)

# Depend on OBJ_DEFAULT_FUZZ to ensure that it is always compiled.
# Don't depend on LIB_FUZZING_ENGINE, which we can't build. When used
# by oss-fuzz, it will be there.
$(BINS_fuzz): $(TARGETS_libqpdf) $(OBJ_DEFAULT_FUZZ)

# Files from the test suite that are good for seeding the fuzzer.
# Update $qpdf_n_test_files in qtest/fuzz.test if you change this list.
CORPUS_FROM_TEST = \
	stream-data.pdf \
	lin5.pdf \
	field-types.pdf \
	image-streams-small.pdf \
	need-appearances.pdf \
	outlines-with-actions.pdf \
	outlines-with-old-root-dests.pdf \
	page-labels-and-outlines.pdf \
	page-labels-num-tree.pdf \
	issue-99b.pdf \
	issue-99.pdf \
	issue-100.pdf \
	issue-101.pdf \
	issue-106.pdf \
	issue-117.pdf \
	issue-119.pdf \
	issue-120.pdf \
	issue-141a.pdf \
	issue-141b.pdf \
	issue-143.pdf \
	issue-146.pdf \
	issue-147.pdf \
	issue-148.pdf \
	issue-149.pdf \
	issue-150.pdf \
	issue-202.pdf \
	issue-263.pdf \
	issue-335a.pdf \
	issue-335b.pdf

# Any file that qpdf_fuzzer should be tested with can be named
# something.fuzz and dropped into this directory.
CORPUS_OTHER = $(wildcard fuzz/qpdf_extra/*.fuzz)

# -----

CORPUS_EXTRA := $(foreach F,$(CORPUS_FROM_TEST),qpdf/qtest/qpdf/$F) \
	$(CORPUS_OTHER)
CORPUS_DIR := fuzz/$(OUTPUT_DIR)/qpdf_fuzzer_seed_corpus

.PHONY: fuzz_corpus
fuzz_corpus:: fuzz/$(OUTPUT_DIR)/fuzz_corpus.stamp
$(foreach F,$(CORPUS_EXTRA),$(eval \
  SHA1_$(notdir $(F)) := $(shell perl fuzz/get_sha1 < $F)))
$(foreach F,$(CORPUS_EXTRA),$(eval \
  fuzz_corpus:: $(CORPUS_DIR)/$(SHA1_$(notdir $(F)))))
$(foreach F,$(CORPUS_EXTRA),$(eval \
  $(CORPUS_DIR)/$(SHA1_$(notdir $(F))): $(F) ; \
	mkdir -p $(CORPUS_DIR); \
	cp $(F) $(CORPUS_DIR)/$(SHA1_$(notdir $(F)))))

fuzz/$(OUTPUT_DIR)/fuzz_corpus.stamp: fuzz/original-corpus.tar.gz $(CORPUS_EXTRA)
	mkdir -p $(CORPUS_DIR)
	(cd $(CORPUS_DIR); tar xzf ../../original-corpus.tar.gz)
	touch $@

$(foreach B,$(FUZZERS),$(eval \
  OBJS_$(B) = $(call src_to_obj,fuzz/$(B).cc)))

ifeq ($(GENDEPS),1)
-include $(foreach B,$(FUZZERS),$(call obj_to_dep,$(OBJS_$(B))))
endif

$(foreach B,$(DEFAULT_FUZZ_RUNNER),$(eval \
  fuzz/$(OUTPUT_DIR)/%.$(OBJ): fuzz/$(B).cc ; \
	$(call compile,fuzz/$(B).cc,$(INCLUDES_fuzz))))

$(foreach B,$(FUZZERS),$(eval \
  $(OBJS_$(B)): fuzz/$(OUTPUT_DIR)/%.$(OBJ): fuzz/$(B).cc ; \
	$(call compile,fuzz/$(B).cc,$(INCLUDES_fuzz))))

ifeq ($(suffix $(LIB_FUZZING_ENGINE)),.$(OBJ))
  FUZZ_as_obj := $(LIB_FUZZING_ENGINE)
  FUZZ_as_lib :=
else
  FUZZ_as_obj :=
  FUZZ_as_lib := $(LIB_FUZZING_ENGINE)
endif

$(foreach B,$(FUZZERS),$(eval \
  fuzz/$(OUTPUT_DIR)/$(call binname,$(B)): $(OBJS_$(B)) ; \
	$(call makebin,$(OBJS_$(B)) $(FUZZ_as_obj),$$@,$(LDFLAGS_libqpdf) $(LDFLAGS),$(FUZZ_as_lib) $(LIBS_libqpdf) $(LIBS))))

ifeq ($(OSS_FUZZ),1)

# Build fuzzers linked with static libraries and installed into a
# location provided by oss-fuzz. This is specifically to support the
# oss-fuzz project. These rules won't on systems that don't allow main
# to be in a library or don't name their libraries libsomething.a.

STATIC_BINS_fuzz := $(foreach B,$(FUZZERS),fuzz/$(OUTPUT_DIR)/static/$(call binname,$(B)))
$(STATIC_BINS_fuzz): $(TARGETS_libqpdf) $(OBJ_DEFAULT_FUZZ)

# OUT is provided in the oss-fuzz environment
OUT ?= $(CURDIR)/fuzz/$(OUTPUT_DIR)/fuzz-install

# These are not fully static, but they statically link with qpdf and
# our external dependencies other than system libraries.
$(foreach B,$(FUZZERS),$(eval \
  fuzz/$(OUTPUT_DIR)/static/$(call binname,$(B)): $(OBJS_$(B)) ; \
	$(call makebin,$(OBJS_$(B)),$$@,$(LDFLAGS_libqpdf) $(LDFLAGS),$(LIB_FUZZING_ENGINE) $(patsubst -l%,-l:lib%.a,$(LIBS_libqpdf) $(LIBS)))))

# The install_fuzz target is used by build.sh in oss-fuzz's qpdf project.
install_fuzz: $(STATIC_BINS_fuzz)
	mkdir -p $(OUT)
	cp fuzz/pdf.dict $(STATIC_BINS_fuzz) $(OUT)/
	for B in $(FUZZERS); do \
	  if test -f fuzz/$${B}.options; then \
	    cp fuzz/$${B}.options $(OUT)/$${B}.options; \
	  fi; \
	  if test -d fuzz/$(OUTPUT_DIR)/$${B}_seed_corpus; then \
	    (cd fuzz/$(OUTPUT_DIR)/$${B}_seed_corpus; zip -q -r $(OUT)/$${B}_seed_corpus.zip .); \
	  elif test -d fuzz/$${B}_seed_corpus; then \
	    (cd fuzz/$${B}_seed_corpus; zip -q -r $(OUT)/$${B}_seed_corpus.zip .); \
	  fi; \
	done

endif # OSS_FUZZ
