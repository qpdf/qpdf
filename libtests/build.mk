BINS_libtests = \
	aes \
	ascii85 \
	bits \
	buffer \
	concatenate \
	flate \
	hex \
	lzw \
	md5 \
	pcre \
	png_filter \
	pointer_holder \
	qutil \
	random \
	rc4 \
	sha2

TARGETS_libtests = $(foreach B,$(BINS_libtests),libtests/$(OUTPUT_DIR)/$(call binname,$(B)))

$(TARGETS_libtests): $(TARGETS_libqpdf)

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
	$(call makebin,$(OBJS_$(B)),$$@,$(LDFLAGS) $(LDFLAGS_libqpdf),$(LIBS) $(LIBS_libqpdf))))
