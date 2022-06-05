// See "ABI checks" in README-maintainer and comments in check_abi.

#include <iostream>

#include <qpdf/Buffer.hh>
#include <qpdf/BufferInputSource.hh>
#include <qpdf/ClosedFileInputSource.hh>
#include <qpdf/FileInputSource.hh>
#include <qpdf/InputSource.hh>
#include <qpdf/JSON.hh>
#include <qpdf/PDFVersion.hh>
#include <qpdf/Pipeline.hh>
#include <qpdf/Pl_Buffer.hh>
#include <qpdf/Pl_Concatenate.hh>
#include <qpdf/Pl_Count.hh>
#include <qpdf/Pl_DCT.hh>
#include <qpdf/Pl_Discard.hh>
#include <qpdf/Pl_Flate.hh>
#include <qpdf/Pl_OStream.hh>
#include <qpdf/Pl_QPDFTokenizer.hh>
#include <qpdf/Pl_RunLength.hh>
#include <qpdf/Pl_StdioFile.hh>
#include <qpdf/Pl_String.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFAcroFormDocumentHelper.hh>
#include <qpdf/QPDFAnnotationObjectHelper.hh>
#include <qpdf/QPDFCryptoProvider.hh>
#include <qpdf/QPDFEFStreamObjectHelper.hh>
#include <qpdf/QPDFEmbeddedFileDocumentHelper.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/QPDFFileSpecObjectHelper.hh>
#include <qpdf/QPDFFormFieldObjectHelper.hh>
#include <qpdf/QPDFJob.hh>
#include <qpdf/QPDFLogger.hh>
#include <qpdf/QPDFMatrix.hh>
#include <qpdf/QPDFNameTreeObjectHelper.hh>
#include <qpdf/QPDFNumberTreeObjectHelper.hh>
#include <qpdf/QPDFObjGen.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFOutlineDocumentHelper.hh>
#include <qpdf/QPDFOutlineObjectHelper.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageLabelDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFStreamFilter.hh>
#include <qpdf/QPDFSystemError.hh>
#include <qpdf/QPDFTokenizer.hh>
#include <qpdf/QPDFUsage.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QPDFXRefEntry.hh>

#define ignore_class(cls)
#define print_size(cls) std::cout << #cls << " " << sizeof(cls) << std::endl

// These are not classes
// -------
ignore_class(QUtil);
ignore_class(QTC);

int
main()
{
    // Print the size of every class in the public API. This file is
    // read by the check_abi script at the top of the repository as
    // part of the binary compatibility checks performed before each
    // release.
    print_size(Buffer);
    print_size(BufferInputSource);
    print_size(ClosedFileInputSource);
    print_size(FileInputSource);
    print_size(InputSource);
    print_size(JSON);
    print_size(PDFVersion);
    print_size(Pipeline);
    print_size(Pl_Buffer);
    print_size(Pl_Concatenate);
    print_size(Pl_Count);
    print_size(Pl_DCT);
    print_size(Pl_Discard);
    print_size(Pl_Flate);
    print_size(Pl_OStream);
    print_size(Pl_QPDFTokenizer);
    print_size(Pl_RunLength);
    print_size(Pl_StdioFile);
    print_size(Pl_String);
    print_size(QPDF);
    print_size(QPDFAcroFormDocumentHelper);
    print_size(QPDFAnnotationObjectHelper);
    print_size(QPDFCryptoProvider);
    print_size(QPDFEFStreamObjectHelper);
    print_size(QPDFEmbeddedFileDocumentHelper);
    print_size(QPDFExc);
    print_size(QPDFFileSpecObjectHelper);
    print_size(QPDFFormFieldObjectHelper);
    print_size(QPDFJob);
    print_size(QPDFJob::AttConfig);
    print_size(QPDFJob::Config);
    print_size(QPDFJob::CopyAttConfig);
    print_size(QPDFJob::EncConfig);
    print_size(QPDFJob::PagesConfig);
    print_size(QPDFJob::UOConfig);
    print_size(QPDFLogger);
    print_size(QPDFMatrix);
    print_size(QPDFNameTreeObjectHelper);
    print_size(QPDFNameTreeObjectHelper::iterator);
    print_size(QPDFNumberTreeObjectHelper);
    print_size(QPDFNumberTreeObjectHelper::iterator);
    print_size(QPDFObjGen);
    print_size(QPDFObjectHandle);
    print_size(QPDFObjectHandle::ParserCallbacks);
    print_size(QPDFObjectHandle::QPDFArrayItems);
    print_size(QPDFObjectHandle::QPDFArrayItems::iterator);
    print_size(QPDFObjectHandle::QPDFDictItems);
    print_size(QPDFObjectHandle::QPDFDictItems::iterator);
    print_size(QPDFObjectHandle::StreamDataProvider);
    print_size(QPDFObjectHandle::TokenFilter);
    print_size(QPDFOutlineDocumentHelper);
    print_size(QPDFOutlineObjectHelper);
    print_size(QPDFPageDocumentHelper);
    print_size(QPDFPageLabelDocumentHelper);
    print_size(QPDFPageObjectHelper);
    print_size(QPDFStreamFilter);
    print_size(QPDFSystemError);
    print_size(QPDFTokenizer);
    print_size(QPDFTokenizer::Token);
    print_size(QPDFUsage);
    print_size(QPDFWriter);
    print_size(QPDFXRefEntry);
    return 0;
}
