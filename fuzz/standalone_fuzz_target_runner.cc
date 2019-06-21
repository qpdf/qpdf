#include <qpdf/QUtil.hh>
#include <qpdf/PointerHolder.hh>
#include <qpdf/QIntC.hh>
#include <iostream>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(unsigned char const* data, size_t size);

static void read_file_into_memory(
    char const* filename,
    PointerHolder<unsigned char>& file_buf, size_t& size)
{
    FILE* f = QUtil::safe_fopen(filename, "rb");
    fseek(f, 0, SEEK_END);
    size = QIntC::to_size(QUtil::tell(f));
    fseek(f, 0, SEEK_SET);
    file_buf = PointerHolder<unsigned char>(true, new unsigned char[size]);
    unsigned char* buf_p = file_buf.getPointer();
    size_t bytes_read = 0;
    size_t len = 0;
    while ((len = fread(buf_p + bytes_read, 1, size - bytes_read, f)) > 0)
    {
        bytes_read += len;
    }
    if (bytes_read != size)
    {
        throw std::runtime_error(
            std::string("failure reading file ") + filename +
            " into memory: read " +
            QUtil::uint_to_string(bytes_read) + "; wanted " +
            QUtil::uint_to_string(size));
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++)
    {
        PointerHolder<unsigned char> file_buf;
        size_t size = 0;
        read_file_into_memory(argv[i], file_buf, size);
        LLVMFuzzerTestOneInput(file_buf.getPointer(), size);
        std::cout << argv[i] << " successful" << std::endl;
    }
    return 0;
}
