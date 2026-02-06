#ifndef QPDFNAMEDDESTINATIONOBJECTHELPER_HH
#define QPDFNAMEDDESTINATIONOBJECTHELPER_HH

#include <qpdf/QPDFExplicitDestinationObjectHelper.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFObjectHelper.hh>

class QPDF_DLL QPDFNamedDestinationObjectHelper: public QPDFObjectHelper
{
  public:
    QPDF_DLL
    QPDFNamedDestinationObjectHelper(QPDFObjectHandle oh);
    virtual ~QPDFNamedDestinationObjectHelper() = default;

    /**
     * Performs a tolerant recursive resolution (following indirect references
     * and /D keys) to find the terminal destination array. Returns an
     * explicit helper wrapping the result.
     */
    QPDF_DLL
    QPDFExplicitDestinationObjectHelper unwrap() const;

    /**
     * Returns true if the structure follows the 1-step spec limit
     * (either an array or a dictionary with a /D array). Does *not* validate the grammar of the
     * destination array; it only checks that the structure is one of the two allowed forms.
     */
    QPDF_DLL
    bool strictly_compliant() const;

    QPDF_DLL
    bool
    is_null() const noexcept
    {
        return oh().isNull();
    }
};

#endif // QPDFNAMEDDESTINATIONOBJECTHELPER_HH
