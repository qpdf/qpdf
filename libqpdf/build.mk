TARGETS_libqpdf = libqpdf/$(OUTPUT_DIR)/$(call libname,qpdf)

ifeq ($(MAINTAINER_MODE), 1)
ifeq ($(shell if ./generate_auto_job --check; then echo 0; else echo 1; fi), 1)
_ := $(shell ./generate_auto_job --generate)
endif
endif

INCLUDES_libqpdf = include libqpdf
LDFLAGS_libqpdf = -Llibqpdf/$(OUTPUT_DIR)
LIBS_libqpdf = -lqpdf

CRYPTO_NATIVE = \
	libqpdf/AES_PDF_native.cc \
	libqpdf/MD5_native.cc \
	libqpdf/QPDFCrypto_native.cc \
	libqpdf/RC4_native.cc \
	libqpdf/SHA2_native.cc \
	libqpdf/rijndael.cc \
	libqpdf/sha2.c \
	libqpdf/sha2big.c

CRYPTO_OPENSSL = \
	libqpdf/QPDFCrypto_openssl.cc

CRYPTO_GNUTLS = \
	libqpdf/QPDFCrypto_gnutls.cc

SRCS_libqpdf = \
	libqpdf/BitStream.cc \
	libqpdf/BitWriter.cc \
	libqpdf/Buffer.cc \
	libqpdf/BufferInputSource.cc \
	libqpdf/ClosedFileInputSource.cc \
	libqpdf/ContentNormalizer.cc \
	libqpdf/CryptoRandomDataProvider.cc \
	libqpdf/FileInputSource.cc \
	libqpdf/InputSource.cc \
	libqpdf/InsecureRandomDataProvider.cc \
	libqpdf/JSON.cc \
	libqpdf/JSONHandler.cc \
	libqpdf/MD5.cc \
	libqpdf/NNTree.cc \
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
	libqpdf/QPDFArgParser.cc \
	libqpdf/QPDFCryptoProvider.cc \
	libqpdf/QPDFEFStreamObjectHelper.cc \
	libqpdf/QPDFEmbeddedFileDocumentHelper.cc \
	libqpdf/QPDFExc.cc \
	libqpdf/QPDFFileSpecObjectHelper.cc \
	libqpdf/QPDFFormFieldObjectHelper.cc \
	libqpdf/QPDFJob.cc \
	libqpdf/QPDFJob_argv.cc \
	libqpdf/QPDFJob_config.cc \
	libqpdf/QPDFMatrix.cc \
	libqpdf/QPDFNameTreeObjectHelper.cc \
	libqpdf/QPDFNumberTreeObjectHelper.cc \
	libqpdf/QPDFObjGen.cc \
	libqpdf/QPDFObject.cc \
	libqpdf/QPDFObjectHandle.cc \
	libqpdf/QPDFOutlineDocumentHelper.cc \
	libqpdf/QPDFOutlineObjectHelper.cc \
	libqpdf/QPDFPageDocumentHelper.cc \
	libqpdf/QPDFPageLabelDocumentHelper.cc \
	libqpdf/QPDFPageObjectHelper.cc \
	libqpdf/QPDFStreamFilter.cc \
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
	libqpdf/ResourceFinder.cc \
	libqpdf/SecureRandomDataProvider.cc \
	libqpdf/SF_FlateLzwDecode.cc \
	libqpdf/SparseOHArray.cc \
	libqpdf/qpdf-c.cc

ifeq ($(USE_CRYPTO_NATIVE), 1)
SRCS_libqpdf += $(CRYPTO_NATIVE)
endif

ifeq ($(USE_CRYPTO_OPENSSL), 1)
SRCS_libqpdf += $(CRYPTO_OPENSSL)
endif

ifeq ($(USE_CRYPTO_GNUTLS), 1)
SRCS_libqpdf += $(CRYPTO_GNUTLS)
endif

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
