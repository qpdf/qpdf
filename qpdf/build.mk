BINS_qpdf = qpdf test_driver pdf_from_scratch test_large_file
CBINS_qpdf = qpdf-ctest

TARGETS_qpdf = $(foreach B,$(BINS_qpdf) $(CBINS_qpdf),qpdf/$(OUTPUT_DIR)/$(call binname,$(B)))

$(TARGETS_qpdf): $(TARGETS_libqpdf)

INCLUDES_qpdf = include

TC_SRCS_qpdf = $(wildcard libqpdf/*.cc) $(wildcard qpdf/*.cc)

# -----

$(foreach B,$(BINS_qpdf),$(eval \
  OBJS_$(B) = $(call src_to_obj,qpdf/$(B).cc)))
$(foreach B,$(CBINS_qpdf),$(eval \
  OBJS_$(B) = $(call c_src_to_obj,qpdf/$(B).c)))

ifeq ($(GENDEPS),1)
-include $(foreach B,$(BINS_qpdf) $(CBINS_qpdf),$(call obj_to_dep,$(OBJS_$(B))))
endif

$(foreach B,$(BINS_qpdf),$(eval \
  $(OBJS_$(B)): qpdf/$(OUTPUT_DIR)/%.$(OBJ): qpdf/$(B).cc ; \
	$(call compile,qpdf/$(B).cc,$(INCLUDES_qpdf))))

$(foreach B,$(CBINS_qpdf),$(eval \
  $(OBJS_$(B)): qpdf/$(OUTPUT_DIR)/%.$(OBJ): qpdf/$(B).c ; \
	$(call c_compile,qpdf/$(B).c,$(INCLUDES_qpdf))))

$(foreach B,$(BINS_qpdf) $(CBINS_qpdf),$(eval \
  qpdf/$(OUTPUT_DIR)/$(call binname,$(B)): $(OBJS_$(B)) ; \
	$(call makebin,$(OBJS_$(B)),$$@,$(LDFLAGS) $(LDFLAGS_libqpdf),$(LIBS_libqpdf) $(LIBS))))
