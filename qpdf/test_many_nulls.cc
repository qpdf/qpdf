#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QUtil.hh>
#include <cstdlib>
#include <iostream>

int
main(int argc, char* argv[])
{
    auto whoami = QUtil::getWhoami(argv[0]);
    if (argc != 2) {
        std::cerr << "Usage: " << whoami << " outfile.pdf" << std::endl;
        exit(2);
    }
    char const* outfile = argv[1];

    // Create a file with lots of arrays containing very large numbers
    // of nulls. Prior to qpdf 9.0.0, qpdf had a lot of trouble with
    // this kind of file. This program is used to generate a file that
    // can be used in the test suite and performance benchmarking.
    QPDF q;
    q.emptyPDF();
    auto null = QPDFObjectHandle::newNull();
    auto top = "[]"_qpdf;
    for (int i = 0; i < 20; ++i) {
        auto inner = "[]"_qpdf;
        for (int j = 0; j < 20000; ++j) {
            inner.appendItem(null);
        }
        top.appendItem(inner);
    }
    q.getTrailer().replaceKey("/Nulls", q.makeIndirectObject(top));
    auto page = "<< /Type /Page /MediaBox [0 0 612 792] >>"_qpdf;
    page = q.makeIndirectObject(page);
    q.getRoot().getKey("/Pages").getKey("/Kids").appendItem(page);
    QPDFWriter w(q, outfile);
    w.setObjectStreamMode(qpdf_o_generate);
    w.setDeterministicID(true);
    w.write();
    return 0;
}
