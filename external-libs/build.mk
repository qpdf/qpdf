TARGETS_external-libs = external-libs/$(OUTPUT_DIR)/$(call libname,external)
INCLUDES_external-libs = external-libs/zlib external-libs/pcre

SRCS_external-libs_zlib = \
	external-libs/zlib/adler32.c \
	external-libs/zlib/compress.c \
	external-libs/zlib/crc32.c \
	external-libs/zlib/gzio.c \
	external-libs/zlib/uncompr.c \
	external-libs/zlib/deflate.c \
	external-libs/zlib/trees.c \
	external-libs/zlib/zutil.c \
	external-libs/zlib/inflate.c \
	external-libs/zlib/infback.c \
	external-libs/zlib/inftrees.c \
	external-libs/zlib/inffast.c \

SRCS_external-libs_pcre = \
	external-libs/pcre/maketables.c \
	external-libs/pcre/get.c \
	external-libs/pcre/study.c \
	external-libs/pcre/pcre.c

SRCS_external-libs = $(SRCS_external-libs_zlib) $(SRCS_external-libs_pcre)

external-libs/$(OUTPUT_DIR)/pcre.$(LOBJ): external-libs/$(OUTPUT_DIR)/chartables.c

external-libs/$(OUTPUT_DIR)/chartables.c: external-libs/$(OUTPUT_DIR)/$(call binname,dftables)
	external-libs/$(OUTPUT_DIR)/$(call binname,dftables) \
		external-libs/$(OUTPUT_DIR)/chartables.c

# -----

OBJS_external-libs_zlib = $(call c_src_to_lobj,$(subst zlib/,,$(SRCS_external-libs_zlib)))
OBJS_external-libs_pcre = $(call c_src_to_lobj,$(subst pcre/,,$(SRCS_external-libs_pcre)))

OBJS_external-libs = $(OBJS_external-libs_zlib) $(OBJS_external-libs_pcre)

ifeq ($(GENDEPS),1)
-include $(call lobj_to_dep,$(OBJS_external-libs))
endif

$(OBJS_external-libs_zlib): external-libs/$(OUTPUT_DIR)/%.$(LOBJ): external-libs/zlib/%.c
	$(call c_libcompile,$<,$(INCLUDES_external-libs))

$(OBJS_external-libs_pcre): external-libs/$(OUTPUT_DIR)/%.$(LOBJ): external-libs/pcre/%.c
	$(call c_libcompile,$<,$(INCLUDES_external-libs) external-libs/$(OUTPUT_DIR))

$(TARGETS_external-libs): $(OBJS_external-libs)
	$(call makeslib,$(OBJS_external-libs),$(TARGETS_external-libs))

OBJS_dftables = $(call c_src_to_obj,external-libs/pcre/dftables.c)

$(OBJS_dftables): external-libs/pcre/dftables.c
	$(call c_compile,$<,)

external-libs/$(OUTPUT_DIR)/$(call binname,dftables): LIBS=
external-libs/$(OUTPUT_DIR)/$(call binname,dftables): $(OBJS_dftables)
	$(call makebin,$(OBJS_dftables),$@)
