
#include <qpdf/Pl_PNGFilter.hh>
#include <qpdf/Pl_StdioFile.hh>

#include <iostream>
#include <errno.h>
#include <assert.h>

FILE* safe_fopen(char const* filename, char const* mode)
{
    FILE* result = fopen(filename, mode);
    if (result == 0)
    {
	std::cerr << "fopen " << filename << " failed: " << strerror(errno)
		  << std::endl;
    }
    return result;
}

void run(char const* filename, bool encode, unsigned int columns)
{
    // Decode the file
    FILE* in = safe_fopen(filename, "rb");
    FILE* o1 = safe_fopen("out", "wb");
    Pipeline* out = new Pl_StdioFile("out", o1);
    Pipeline* pl = new Pl_PNGFilter(
	"png", out,
	encode ? Pl_PNGFilter::a_encode : Pl_PNGFilter::a_decode,
	columns, 0 /* not used */);
    assert((2 * (columns + 1)) < 1024);
    unsigned char buf[1024];
    size_t len;
    while (true)
    {
	len = fread(buf, 1, (2 * columns) + 1, in);
	if (len == 0)
	{
	    break;
	}
	pl->write(buf, len);
	len = fread(buf, 1, 1, in);
	if (len == 0)
	{
	    break;
	}
	pl->write(buf, len);
	len = fread(buf, 1, 1, in);
	if (len == 0)
	{
	    break;
	}
	pl->write(buf, len);
    }

    pl->finish();
    delete pl;
    delete out;
    fclose(o1);
    fclose(in);

    std::cout << "done" << std::endl;
}

int main(int argc, char* argv[])
{
    if (argc != 4)
    {
	std::cerr << "Usage: pipeline {en,de}code filename columns" << std::endl;
	exit(2);
    }
    bool encode = (strcmp(argv[1], "encode") == 0);
    char* filename = argv[2];
    int columns = atoi(argv[3]);

    try
    {
	run(filename, encode, columns);
    }
    catch (QEXC::General& e)
    {
	std::cout << e.unparse() << std::endl;
    }
    return 0;
}
