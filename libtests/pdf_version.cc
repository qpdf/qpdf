#include <qpdf/PDFVersion.hh>

#include <cassert>
#include <iostream>

int main()
{
    PDFVersion v1;
    assert(v1.getMajor() == 0);
    assert(v1.getMinor() == 0);
    assert(v1.getExtensionLevel() == 0);
    v1 = PDFVersion(1, 7, 8);
    assert(v1.getMajor() == 1);
    assert(v1.getMinor() == 7);
    assert(v1.getExtensionLevel() == 8);
    std::string version;
    int extension_level = -1;
    v1.getVersion(version, extension_level);
    assert(version == "1.7");
    assert(extension_level == 8);
    PDFVersion v2(1, 5);
    v2.getVersion(version, extension_level);
    assert(version == "1.5");
    assert(extension_level == 0);
    assert(v2 < v1);
    PDFVersion v3 = v1;
    assert(v3 == v1);
    v1.updateIfGreater(v2);
    assert(v3 == v1);
    assert(! (v3 == v2));
    assert(! (v2 == v1));
    v2.updateIfGreater(v1);
    assert(v2 == v1);
    v2.getVersion(version, extension_level);
    assert(version == "1.7");
    assert(extension_level == 8);
    assert(PDFVersion(1, 2) < PDFVersion(1, 3));
    assert(PDFVersion(1, 2) < PDFVersion(1, 2, 1));
    assert(PDFVersion(1, 2, 1) < PDFVersion(1, 2, 2));
    assert(! (PDFVersion(1, 2, 2) < PDFVersion(1, 2, 2)));
    assert(! (PDFVersion(1, 2, 3) < PDFVersion(1, 2, 2)));
    assert(PDFVersion(1, 1) < PDFVersion(1, 2, 2));
    assert(! (PDFVersion(1, 3) < PDFVersion(1, 2, 2)));
    assert(! (PDFVersion(2, 0) < PDFVersion(1, 9, 9)));
    assert(! (PDFVersion(2, 0) < PDFVersion(2, 0)));
    assert(PDFVersion(2, 0) == PDFVersion(2, 0));
    assert(PDFVersion(2, 0, 1) == PDFVersion(2, 0, 1));

    std::cout << "PDFVersion assertions passed" << std::endl;
    return 0;
}
