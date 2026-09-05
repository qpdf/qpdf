#include <qpdf/QPDF.hh>
#include <qpdf/QPDFNamedDestinationDocumentHelper.hh>
#include <qpdf/QPDFNamedDestinationObjectHelper.hh>
#include <iostream>
#include <optional>

using Kind = QPDFNamedDestinationDocumentHelper::Kind;

void
ndest_info(
    std::string const& name,
    QPDFNamedDestinationDocumentHelper const& ddh,
    QPDFNamedDestinationObjectHelper const& ndest,
    std::optional<Kind> kind = std::nullopt)
{
    auto kind_str = kind.has_value() ? (kind == Kind::NAME ? "NAME" : "STRING") : "NONE";
    QPDFObjectHandle raw = ndest.getObjectHandle();
    std::cout << std::boolalpha;
    std::cout << "Named destination: " << name << "\n  kind = " << kind_str
              << "\n  ndest.isNull() = " << ndest.isNull()
              << "\n  ndest.hasValidWrapper() = " << ndest.hasValidWrapper()
              << "\n  raw = " << raw.unparse() << " ==> " << raw.unparseResolved() << "\n";

    // 1. Unwrap to explicit destination
    auto edest = ndest.unwrap();
    auto array = edest.getExplicitArray();

    // 3. Process valid destination
    std::cout << "  Explicit destination edest := ndest.unwrap()"
              << "\n    edest.getExplicitArray() = " << array.unparse() << " ==> "
              << array.unparseResolved()
              << "\n    edest.isStrictlyCompliant = " << edest.isStrictlyCompliant()
              << "\n    findPageIndex(edest) = " << ddh.findPageIndex(edest)
              << "\n    edest.isRemote() = " << edest.isRemote() << "\n\n";
}

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

        auto callback = [&ddh](
                            std::string const& name,
                            QPDFNamedDestinationObjectHelper const& ndest,
                            Kind kind) { ndest_info(name, ddh, ndest, kind); };

        std::cout << "=== 1. Iteration over all named destinations ===\n\n";
        ddh.forEach(callback);
        std::cout << "\n=== 2. Lookup specific named destinations ===\n\n";
        auto lookAndSee =
            [&ddh, &callback](std::string const& name, std::optional<Kind> filter = std::nullopt) {
                QPDFNamedDestinationObjectHelper ndest;
                if (!filter.has_value()) {
                    ndest = ddh.lookup(name);
                } else if (*filter == Kind::NAME) {
                    ndest = ddh.lookupName(name);
                } else {
                    ndest = ddh.lookupString(name);
                }
                ndest_info(name, ddh, ndest, filter);
            };
        lookAndSee("Foo", Kind::STRING);
        lookAndSee("Foo", Kind::NAME);
        lookAndSee("/Foo", Kind::NAME);
        lookAndSee("TrulyModern", Kind::STRING);
        lookAndSee("/TrulyModern", Kind::NAME);
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    return 0;
}
