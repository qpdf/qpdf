#include <qpdf/QUtil.hh>
#include <iostream>

static void
test_numrange(char const* range)
{
    if (range == 0) {
        std::cout << "null" << std::endl;
    } else {
        std::vector<int> result = QUtil::parse_numrange(range, 15);
        std::cout << "numeric range " << range << " ->";
        for (std::vector<int>::iterator iter = result.begin();
             iter != result.end();
             ++iter) {
            std::cout << " " << *iter;
        }
        std::cout << std::endl;
    }
}

int
main(int argc, char* argv[])
{
    try {
        test_numrange(argv[1]);
    } catch (std::exception& e) {
        std::cout << e.what() << std::endl;
        return 2;
    }

    return 0;
}
