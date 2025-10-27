#include <qpdf/Buffer.hh>
#include <qpdf/BufferInputSource.hh>
#include <qpdf/ClosedFileInputSource.hh>
#include <qpdf/Constants.h>
#include <qpdf/DLL.h>
#include <qpdf/FileInputSource.hh>
#include <qpdf/InputSource.hh>
#include <qpdf/JSON.hh>
#include <qpdf/ObjectHandle.hh>
#include <qpdf/PDFVersion.hh>
#include <qpdf/Pipeline.hh>
#include <qpdf/Pl_Buffer.hh>
#include <qpdf/Pl_Concatenate.hh>
#include <qpdf/Pl_Count.hh>
#include <qpdf/Pl_DCT.hh>
#include <qpdf/Pl_Discard.hh>
#include <qpdf/Pl_Flate.hh>
#include <qpdf/Pl_Function.hh>
#include <qpdf/Pl_OStream.hh>
#include <qpdf/Pl_QPDFTokenizer.hh>
#include <qpdf/Pl_RunLength.hh>
#include <qpdf/Pl_StdioFile.hh>
#include <qpdf/Pl_String.hh>
#include <qpdf/PointerHolder.hh>
#include <qpdf/QIntC.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFAcroFormDocumentHelper.hh>
#include <qpdf/QPDFAnnotationObjectHelper.hh>
#include <qpdf/QPDFCryptoImpl.hh>
#include <qpdf/QPDFCryptoProvider.hh>
#include <qpdf/QPDFDocumentHelper.hh>
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
// do not include obsolete QPDFObject.hh
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFObjectHelper.hh>
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
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/RandomDataProvider.hh>
#include <qpdf/Types.h>
#include <qpdf/qpdf-c.h>
#include <qpdf/qpdfjob-c.h>
#include <qpdf/qpdflogger-c.h>

#include <iostream>

int
main()
{
    std::cout << QPDF::QPDFVersion() << '\n';
    return 0;
}
