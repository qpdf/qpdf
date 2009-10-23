TARGETS_zlib-flate = \
	zlib-flate/$(OUTPUT_DIR)/$(call binname,zlib-flate)

$(TARGETS_zlib-flate): $(TARGETS_libqpdf)

INCLUDES_zlib-flate = include

SRCS_zlib-flate = zlib-flate/zlib-flate.cc

# -----

OBJS_zlib-flate = $(call src_to_obj,$(SRCS_zlib-flate))

ifeq ($(GENDEPS),1)
-include $(call obj_to_dep,$(OBJS_zlib-flate))
endif

$(OBJS_zlib-flate): zlib-flate/$(OUTPUT_DIR)/%.$(OBJ): zlib-flate/%.cc
	$(call compile,$<,$(INCLUDES_zlib-flate))

zlib-flate/$(OUTPUT_DIR)/$(call binname,zlib-flate): $(OBJS_zlib-flate)
	$(call makebin,$(OBJS_zlib-flate),$@,$(LDFLAGS) $(LDFLAGS_libqpdf),$(LIBS_libqpdf) $(LIBS))
