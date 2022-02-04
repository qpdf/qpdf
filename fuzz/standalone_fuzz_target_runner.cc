#include <qpdf/QUtil.hh>
#include <iostream>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(unsigned char const* data, size_t size);

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++)
    {
        PointerHolder<char> file_buf;
        size_t size = 0;
        QUtil::read_file_into_memory(argv[i], file_buf, size);
        LLVMFuzzerTestOneInput(
            reinterpret_cast<unsigned char*>(file_buf.get()), size);
        std::cout << argv[i] << " successful" << std::endl;
    }
    return 0;
}
