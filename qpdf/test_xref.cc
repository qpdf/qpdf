#include <qpdf/QPDF.hh>

#include <cstdlib>
#include <iostream>
#include <map>

int
main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cerr << "usage: test_xref INPUT.pdf" << std::endl;
        std::exit(2);
    }

    try {
        QPDF qpdf;
        qpdf.processFile(argv[1]);

        for (auto const& iter: qpdf.getXRefTable()) {
            std::cout << iter.first.getObj() << "/" << iter.first.getGen()
                      << ", ";
            switch (iter.second.getType()) {
            case 0:
                std::cout << "free entry" << std::endl;
                break;
            case 1:
                std::cout << "uncompressed, offset = "
                          << iter.second.getOffset() << " (0x" << std::hex
                          << iter.second.getOffset() << std::dec << ")"
                          << std::endl;
                break;
            case 2:
                std::cout << "compressed, stream number = "
                          << iter.second.getObjStreamNumber()
                          << ", stream index = "
                          << iter.second.getObjStreamIndex() << std::endl;
                break;
            default:
                std::cerr << "unknown" << std::endl;
                std::exit(2);
            }
        }
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        std::exit(2);
    }

    return 0;
}
