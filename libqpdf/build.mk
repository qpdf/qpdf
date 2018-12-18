TARGETS_libqpdf = libqpdf/$(OUTPUT_DIR)/$(call libname,qpdf)

INCLUDES_libqpdf = include libqpdf
LDFLAGS_libqpdf = -Llibqpdf/$(OUTPUT_DIR)
LIBS_libqpdf = -lqpdf

SRCS_libqpdf = \
	libqpdf/BitStream.cc \
	libqpdf/BitWriter.cc \
	libqpdf/Buffer.cc \
	libqpdf/BufferInputSource.cc \
	libqpdf/ClosedFileInputSource.cc \
	libqpdf/ContentNormalizer.cc \
	libqpdf/FileInputSource.cc \
	libqpdf/InputSource.cc \
	libqpdf/InsecureRandomDataProvider.cc \
	libqpdf/MD5.cc \
	libqpdf/OffsetInputSource.cc \
	libqpdf/Pipeline.cc \
	libqpdf/Pl_AES_PDF.cc \
	libqpdf/Pl_ASCII85Decoder.cc \
	libqpdf/Pl_ASCIIHexDecoder.cc \
	libqpdf/Pl_Buffer.cc \
	libqpdf/Pl_Concatenate.cc \
	libqpdf/Pl_Count.cc \
	libqpdf/Pl_DCT.cc \
	libqpdf/Pl_Discard.cc \
	libqpdf/Pl_Flate.cc \
	libqpdf/Pl_LZWDecoder.cc \
	libqpdf/Pl_MD5.cc \
	libqpdf/Pl_PNGFilter.cc \
	libqpdf/Pl_QPDFTokenizer.cc \
	libqpdf/Pl_RC4.cc \
	libqpdf/Pl_RunLength.cc \
	libqpdf/Pl_SHA2.cc \
	libqpdf/Pl_StdioFile.cc \
	libqpdf/Pl_TIFFPredictor.cc \
	libqpdf/QPDF.cc \
	libqpdf/QPDFAcroFormDocumentHelper.cc \
	libqpdf/QPDFAnnotationObjectHelper.cc \
	libqpdf/QPDFExc.cc \
	libqpdf/QPDFFormFieldObjectHelper.cc \
	libqpdf/QPDFNumberTreeObjectHelper.cc \
	libqpdf/QPDFObjGen.cc \
	libqpdf/QPDFObject.cc \
	libqpdf/QPDFObjectHandle.cc \
	libqpdf/QPDFPageDocumentHelper.cc \
	libqpdf/QPDFPageLabelDocumentHelper.cc \
	libqpdf/QPDFPageObjectHelper.cc \
	libqpdf/QPDFSystemError.cc \
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

$(TARGETS_libqpdf): $(OBJS_libqpdf)
	$(call makelib,$(OBJS_libqpdf),$@,$(LDFLAGS),$(LIBS),$(LT_CURRENT),$(LT_REVISION),$(LT_AGE))
