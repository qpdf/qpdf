BINS_qpdf = qpdf test_driver

TARGETS_qpdf = $(foreach B,$(BINS_qpdf),qpdf/$(OUTPUT_DIR)/$(B))

$(TARGETS_qpdf): $(TARGETS_libqpdf)

INCLUDES_qpdf = include

TC_SRCS_qpdf = $(wildcard libqpdf/*.cc) $(wildcard qpdf/*.cc)

# -----

$(foreach B,$(BINS_qpdf),$(eval \
  OBJS_$(B) = $(call src_to_obj,qpdf/$(B).cc)))

ifeq ($(GENDEPS),1)
-include $(foreach B,$(BINS_qpdf),$(call obj_to_dep,$(OBJS_$(B))))
endif

$(foreach B,$(BINS_qpdf),$(eval \
  $(OBJS_$(B)): qpdf/$(OUTPUT_DIR)/%.o: qpdf/$(B).cc ; \
	$(call compile,qpdf/$(B).cc,$(INCLUDES_qpdf))))

$(foreach B,$(BINS_qpdf),$(eval \
  qpdf/$(OUTPUT_DIR)/$(B): $(OBJS_$(B)) ; \
	$(call makebin,$(OBJS_$(B)),$$@)))
