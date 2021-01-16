BINS_libtests = \
	cxx11 \
	aes \
	ascii85 \
	bits \
	buffer \
	closed_file_input_source \
	concatenate \
	dct_compress \
	dct_uncompress \
	flate \
	hex \
	input_source \
	json \
	lzw \
	main_from_wmain \
	matrix \
	md5 \
	nntree \
	numrange \
	pointer_holder \
	predictors \
	qintc \
	qutil \
	random \
	rc4 \
	runlength \
	sha2 \
	sparse_array

TARGETS_libtests = $(foreach B,$(BINS_libtests),libtests/$(OUTPUT_DIR)/$(call binname,$(B)))

$(TARGETS_libtests): $(TARGETS_libqpdf) $(TARGETS_qpdf)

INCLUDES_libtests = include libqpdf

TC_SRCS_libtests = $(wildcard libqpdf/*.cc) $(wildcard libtests/*.cc) \
	libqpdf/bits.icc

# -----

$(foreach B,$(BINS_libtests),$(eval \
  OBJS_$(B) = $(call src_to_obj,libtests/$(B).cc)))

ifeq ($(GENDEPS),1)
-include $(foreach B,$(BINS_libtests),$(call obj_to_dep,$(OBJS_$(B))))
endif

$(foreach B,$(BINS_libtests),$(eval \
  $(OBJS_$(B)): libtests/$(OUTPUT_DIR)/%.$(OBJ): libtests/$(B).cc ; \
	$(call compile,libtests/$(B).cc,$(INCLUDES_libtests))))

$(foreach B,$(BINS_libtests),$(eval \
  libtests/$(OUTPUT_DIR)/$(call binname,$(B)): $(OBJS_$(B)) ; \
	$(call makebin,$(OBJS_$(B)),$$@,$(LDFLAGS_libqpdf) $(LDFLAGS),$(LIBS_libqpdf) $(LIBS))))
