BINS_examples = \
	pdf-bookmarks \
	pdf-count-strings \
	pdf-create \
	pdf-custom-filter \
	pdf-double-page-size \
	pdf-filter-tokens \
	pdf-invert-images \
	pdf-mod-info \
	pdf-name-number-tree \
	pdf-npages \
	pdf-overlay-page \
	pdf-parse-content \
	pdf-set-form-values \
	pdf-split-pages
CBINS_examples = \
	pdf-c-objects \
	pdf-linearize

TARGETS_examples = $(foreach B,$(BINS_examples) $(CBINS_examples),examples/$(OUTPUT_DIR)/$(call binname,$(B)))

$(TARGETS_examples): $(TARGETS_qpdf)

INCLUDES_examples = include

TC_SRCS_examples = $(wildcard examples/*.cc)

# -----

$(foreach B,$(BINS_examples),$(eval \
  OBJS_$(B) = $(call src_to_obj,examples/$(B).cc)))

$(foreach B,$(CBINS_examples),$(eval \
  OBJS_$(B) = $(call c_src_to_obj,examples/$(B).c)))

ifeq ($(GENDEPS),1)
-include $(foreach B,$(BINS_examples) $(CBINS_examples),$(call obj_to_dep,$(OBJS_$(B))))
endif

$(foreach B,$(BINS_examples),$(eval \
  $(OBJS_$(B)): examples/$(OUTPUT_DIR)/%.$(OBJ): examples/$(B).cc ; \
	$(call compile,examples/$(B).cc,$(INCLUDES_examples))))

$(foreach B,$(CBINS_examples),$(eval \
  $(OBJS_$(B)): examples/$(OUTPUT_DIR)/%.$(OBJ): examples/$(B).c ; \
	$(call c_compile,examples/$(B).c,$(INCLUDES_examples))))

$(foreach B,$(BINS_examples) $(CBINS_examples),$(eval \
  examples/$(OUTPUT_DIR)/$(call binname,$(B)): $(OBJS_$(B)) ; \
	$(call makebin,$(OBJS_$(B)),$$@,$(LDFLAGS_libqpdf) $(LDFLAGS),$(LIBS_libqpdf) $(LIBS))))
