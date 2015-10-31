TARGETS_libqpdf = libqpdf/$(OUTPUT_DIR)/$(call libname,qpdf)

INCLUDES_libqpdf = include libqpdf
LDFLAGS_libqpdf = -Llibqpdf/$(OUTPUT_DIR)
LIBS_libqpdf = -lqpdf

SRCS_libqpdf = \
	libqpdf/BitStream.cc \
	libqpdf/BitWriter.cc \
	libqpdf/Buffer.cc \
	libqpdf/BufferInputSource.cc \
	libqpdf/FileInputSource.cc \
	libqpdf/InputSource.cc \
	libqpdf/InsecureRandomDataProvider.cc \
	libqpdf/MD5.cc \
	libqpdf/OffsetInputSource.cc \
	libqpdf/PCRE.cc \
	libqpdf/Pipeline.cc \
	libqpdf/Pl_AES_PDF.cc \
	libqpdf/Pl_ASCII85Decoder.cc \
	libqpdf/Pl_ASCIIHexDecoder.cc \
	libqpdf/Pl_Buffer.cc \
	libqpdf/Pl_Concatenate.cc \
	libqpdf/Pl_Count.cc \
	libqpdf/Pl_Discard.cc \
	libqpdf/Pl_Flate.cc \
	libqpdf/Pl_LZWDecoder.cc \
	libqpdf/Pl_MD5.cc \
	libqpdf/Pl_PNGFilter.cc \
	libqpdf/Pl_QPDFTokenizer.cc \
	libqpdf/Pl_RC4.cc \
	libqpdf/Pl_SHA2.cc \
	libqpdf/Pl_StdioFile.cc \
	libqpdf/QPDF.cc \
	libqpdf/QPDFExc.cc \
	libqpdf/QPDFObjGen.cc \
	libqpdf/QPDFObject.cc \
	libqpdf/QPDFObjectHandle.cc \
	libqpdf/QPDFTokenizer.cc \
	libqpdf/QPDFWriter.cc \
	libqpdf/QPDFXRefEntry.cc \
	libqpdf/QPDF_Array.cc \
	libqpdf/QPDF_Bool.cc \
	libqpdf/QPDF_Dictionary.cc \
	libqpdf/QPDF_InlineImage.cc \
	libqpdf/QPDF_Integer.cc \
	libqpdf/QPDF_Name.cc \
	libqpdf/QPDF_Null.cc \
	libqpdf/QPDF_Operator.cc \
	libqpdf/QPDF_Real.cc \
	libqpdf/QPDF_Reserved.cc \
	libqpdf/QPDF_Stream.cc \
	libqpdf/QPDF_String.cc \
	libqpdf/QPDF_encryption.cc \
	libqpdf/QPDF_linearization.cc \
	libqpdf/QPDF_optimization.cc \
	libqpdf/QPDF_pages.cc \
	libqpdf/QTC.cc \
	libqpdf/QUtil.cc \
	libqpdf/RC4.cc \
	libqpdf/SecureRandomDataProvider.cc \
	libqpdf/qpdf-c.cc \
	libqpdf/rijndael.cc \
	libqpdf/sha2.c \
	libqpdf/sha2big.c

# -----

CCSRCS_libqpdf = $(filter %.cc,$(SRCS_libqpdf))
CSRCS_libqpdf = $(filter %.c,$(SRCS_libqpdf))

CCOBJS_libqpdf = $(call src_to_lobj,$(CCSRCS_libqpdf))
COBJS_libqpdf = $(call c_src_to_lobj,$(CSRCS_libqpdf))
OBJS_libqpdf = $(CCOBJS_libqpdf) $(COBJS_libqpdf)

ifeq ($(GENDEPS),1)
-include $(call lobj_to_dep,$(OBJS_libqpdf))
endif

$(CCOBJS_libqpdf): libqpdf/$(OUTPUT_DIR)/%.$(LOBJ): libqpdf/%.cc
	$(call libcompile,$<,$(INCLUDES_libqpdf))
$(COBJS_libqpdf): libqpdf/$(OUTPUT_DIR)/%.$(LOBJ): libqpdf/%.c
	$(call c_libcompile,$<,$(INCLUDES_libqpdf))

# Last three arguments to makelib are CURRENT,REVISION,AGE.
#
# * If any interfaces have been removed or changed, we are not binary
#   compatible.  Increment CURRENT, and set AGE and REVISION to 0.
#   Also update libqpdf.map, changing the numeric portion to match
#   CURRENT.
#
# * Otherwise, if any interfaces have been added since the last
#   public release, then increment CURRENT and AGE, and set REVISION
#   to 0.
#
# * Otherwise, increment REVISION

$(TARGETS_libqpdf): $(OBJS_libqpdf)
	$(call makelib,$(OBJS_libqpdf),$@,$(LDFLAGS),$(LIBS),16,0,3)
