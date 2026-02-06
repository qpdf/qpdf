#ifndef QPDFNAMEDDESTINATIONDOCUMENTHELPER_HH
#define QPDFNAMEDDESTINATIONDOCUMENTHELPER_HH

#include <qpdf/QPDFDocumentHelper.hh>
#include <qpdf/QPDFNamedDestinationObjectHelper.hh>
#include <functional>
#include <memory>
#include <string>

class QPDF_DLL QPDFNamedDestinationDocumentHelper: public QPDFDocumentHelper
{
  public:
    QPDF_DLL
    QPDFNamedDestinationDocumentHelper(QPDF& qpdf);
    virtual ~QPDFNamedDestinationDocumentHelper() override;

    enum class Kind {
        NAME,  // PDF 1.1. Origin: Dests entry in document catalog
        STRING // PDF 1.2. Origin: document's name dictionary
    };

    /**
     * Lookup a named destination by Name (PDF 1.1+) or String (PDF 1.2+)
     */
    QPDF_DLL
    QPDFNamedDestinationObjectHelper lookup_name(std::string const& name);
    QPDF_DLL
    QPDFNamedDestinationObjectHelper lookup_string(std::string const& name);

    // Use the appropriate lookup method based on the type of the argument.
    // Returns null if the argument is not a name or string, or if the lookup fails.
    QPDF_DLL
    QPDFNamedDestinationObjectHelper lookup(QPDFObjectHandle dest);
    // Convenience method: look up as name and then string, returning the first match.
    QPDF_DLL
    QPDFNamedDestinationObjectHelper lookup(std::string const& name);

    /**
     * Iterate over all named destinations in the document.
     */
    QPDF_DLL
    void for_each(
        std::function<
            void(std::string const& name, QPDFNamedDestinationObjectHelper const& dest, Kind kind)>
            callback);

    /**
     * Map an explicit destination to a 0-based page index.
     * Returns -1 if remote or if the page cannot be found.
     */
    QPDF_DLL
    int find_page_index(QPDFExplicitDestinationObjectHelper const& dest);

  private:
    class Members;
    std::unique_ptr<Members> m;
};

#endif // QPDFNAMEDDESTINATIONDOCUMENTHELPER_HH
