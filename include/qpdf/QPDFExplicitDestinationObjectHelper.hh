#ifndef QPDFEXPLICITDESTINATIONOBJECTHELPER_HH
#define QPDFEXPLICITDESTINATIONOBJECTHELPER_HH

#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFObjectHelper.hh>

class QPDF_DLL QPDFExplicitDestinationObjectHelper: public QPDFObjectHelper
{
  public:
    QPDF_DLL
    QPDFExplicitDestinationObjectHelper(QPDFObjectHandle oh);
    virtual ~QPDFExplicitDestinationObjectHelper() = default;

    /**
     * Returns the underlying array, or a null handle if the structure is not an array. Note that
     * this does not validate the grammar of the destination array; it only checks that the object
     * is an array.
     */
    QPDF_DLL
    QPDFObjectHandle getExplicitArray() const;

    /**
     * Returns true if the destination is remote. A remote destination is one where the first
     * element of the destination array is a non-negative integer page number.
     */
    QPDF_DLL
    bool isRemote() const;

    /**
     * Returns true if the structure follows the grammar in ISO 32000-1, Table 151.
     */
    QPDF_DLL bool isStrictlyCompliant() const;
};

#endif // QPDFEXPLICITDESTINATIONOBJECTHELPER_HH
