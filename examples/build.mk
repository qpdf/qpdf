BINS_examples = pdf-bookmarks pdf-mod-info pdf-npages

TARGETS_examples = $(foreach B,$(BINS_examples),examples/$(OUTPUT_DIR)/$(B))

$(TARGETS_examples): $(TARGETS_qpdf)

INCLUDES_examples = include

TC_SRCS_examples = $(wildcard examples/*.cc)

# -----

$(foreach B,$(BINS_examples),$(eval \
  OBJS_$(B) = $(call src_to_obj,examples/$(B).cc)))

ifeq ($(GENDEPS),1)
-include $(foreach B,$(BINS_examples),$(call obj_to_dep,$(OBJS_$(B))))
endif

$(foreach B,$(BINS_examples),$(eval \
  $(OBJS_$(B)): examples/$(OUTPUT_DIR)/%.o: examples/$(B).cc ; \
	$(call compile,examples/$(B).cc,$(INCLUDES_examples))))

$(foreach B,$(BINS_examples),$(eval \
  examples/$(OUTPUT_DIR)/$(B): $(OBJS_$(B)) ; \
	$(call makebin,$(OBJS_$(B)),$$@)))
