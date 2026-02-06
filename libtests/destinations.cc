#include <qpdf/QPDF.hh>
#include <qpdf/QPDFNamedDestinationDocumentHelper.hh>
#include <iostream>

int
main(int argc, char* argv[])
{
    if (argc != 2) {
        return 2;
    }
    try {
        QPDF pdf;
        pdf.processFile(argv[1]);
        QPDFNamedDestinationDocumentHelper ddh(pdf);

        ddh.for_each([&ddh](
                         std::string const& name,
                         QPDFNamedDestinationObjectHelper const& ndest,
                         QPDFNamedDestinationDocumentHelper::Kind kind) {
            std::cout << name << " (kind = "
                      << (kind == QPDFNamedDestinationDocumentHelper::Kind::NAME ? "NAME"
                                                                                 : "STRING")
                      << ")";

            // 1. Unwrap to explicit destination
            auto edest = ndest.unwrap();

            // 2. Validate array grammar (The "Naked" check)
            auto array = edest.get_explicit_array();
            if (array.isNull()) {
                QPDFObjectHandle raw = ndest.getObjectHandle();
                std::cout << " is invalid. Raw object: " << raw.unparse() << " ==> "
                          << raw.unparseResolved() << "\n";
                return;
            }

            // 3. Process valid destination
            int idx = ddh.find_page_index(edest);
            bool remote = edest.is_remote();
            std::cout << ": find_page_index = " << idx
                      << ", is_remote = " << (remote ? "true" : "false")
                      << ", dest_array = " << array.unparse() << " ==> " << array.unparseResolved()
                      << "\n";
        });
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    return 0;
}
