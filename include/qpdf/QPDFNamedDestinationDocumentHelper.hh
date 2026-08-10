#ifndef QPDFNAMEDDESTINATIONDOCUMENTHELPER_HH
#define QPDFNAMEDDESTINATIONDOCUMENTHELPER_HH

#include <qpdf/QPDFDocumentHelper.hh>
#include <qpdf/QPDFExplicitDestinationObjectHelper.hh>
#include <qpdf/QPDFNamedDestinationObjectHelper.hh>

#include <qpdf/DLL.h>

class QPDFNamedDestinationDocumentHelper: public QPDFDocumentHelper
{
  public:
    QPDF_DLL
    QPDFNamedDestinationDocumentHelper();

    QPDF_DLL
    QPDFNamedDestinationDocumentHelper(QPDF&);

    ~QPDFNamedDestinationDocumentHelper() override = default;

    enum class Kind {
        NAME,  // PDF 1.1. Origin: Dests entry in document catalog
        STRING // PDF 1.2. Origin: document's name dictionary
    };

    // Lookup a named destination by Name (PDF 1.1+) or String (PDF 1.2+)
    QPDF_DLL
    QPDFNamedDestinationObjectHelper lookupName(std::string const& name);
    QPDF_DLL
    QPDFNamedDestinationObjectHelper lookupString(std::string const& name);

    // Use the appropriate lookup method based on the type of the argument.
    // Returns null if the argument is not a name or string, or if the lookup fails.
    QPDF_DLL
    QPDFNamedDestinationObjectHelper lookup(QPDFObjectHandle dest);
    // Convenience method: look up as name and then string, returning the first match.
    QPDF_DLL
    QPDFNamedDestinationObjectHelper lookup(std::string const& name);

    // Iterate over all named destinations in the document.
    //
    // Note: if `name` appears in both /Dests (as a Name) and /Names/Dests
    // (as a string), then the callback will fire twice.
    QPDF_DLL
    void forEach(
        std::function<
            void(std::string const& name, QPDFNamedDestinationObjectHelper const& dest, Kind kind)>
            callback);

    // Map an explicit destination to a 0-based page index.
    // Returns -1 if remote or if the page cannot be found.
    QPDF_DLL
    int findPageIndex(QPDFExplicitDestinationObjectHelper const& dest) const;

  private:
    class Members;

    std::shared_ptr<Members> m;
};

#endif
