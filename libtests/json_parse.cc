#include <qpdf/JSON.hh>
#include <qpdf/QUtil.hh>
#include <iostream>

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: json_parse file" << std::endl;
        return 2;
    }
    char const* filename = argv[1];
    try
    {
        PointerHolder<char> buf;
        size_t size;
        QUtil::read_file_into_memory(filename, buf, size);
        std::string s(buf.getPointer(), size);
        std::cout << JSON::parse(s).unparse() << std::endl;
    }
    catch (std::exception& e)
    {
        std::cerr << "exception: " << filename<< ": " << e.what() << std::endl;
        return 2;
    }
    return 0;
}
