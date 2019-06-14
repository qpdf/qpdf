# This directory contains support for Google's oss-fuzz project. See
# https://github.com/google/oss-fuzz/tree/master/projects/qpdf

FUZZERS = \
    qpdf_fuzzer \
    qpdf_read_memory_fuzzer

DEFAULT_FUZZ_RUNNER := standalone_fuzz_target_runner
OBJ_DEFAULT_FUZZ := fuzz/$(OUTPUT_DIR)/$(DEFAULT_FUZZ_RUNNER).$(OBJ)

BINS_fuzz = $(foreach B,$(FUZZERS),fuzz/$(OUTPUT_DIR)/$(call binname,$(B)))
TARGETS_fuzz = $(OBJ_DEFAULT_FUZZ) $(BINS_fuzz)

INCLUDES_fuzz = include

# LIB_FUZZING_ENGINE is overridden by oss-fuzz
LIB_FUZZING_ENGINE ?= $(OBJ_DEFAULT_FUZZ)

# Depend on OBJ_DEFAULT_FUZZ to ensure that it is always compiled.
# Don't depend on LIB_FUZZING_ENGINE, which we can't build. When used
# by oss-fuzz, it will be there.
$(BINS_fuzz): $(TARGETS_libqpdf) $(OBJ_DEFAULT_FUZZ)

# -----

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
	  cp fuzz/options $(OUT)/$${B}.options; \
	  if test -d fuzz/$${B}_seed_corpus; then \
	    (cd fuzz/$${B}_seed_corpus; zip -q -r $(OUT)/$${B}_seed_corpus.zip .); \
	  fi; \
	done

endif # OSS_FUZZ
