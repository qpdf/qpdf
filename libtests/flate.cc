
#include <qpdf/Pl_Flate.hh>
#include <qpdf/Pl_StdioFile.hh>
#include <qpdf/Pl_Count.hh>
#include <qpdf/QUtil.hh>

#include <iostream>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

void run(char const* filename)
{
    std::string n1 = std::string(filename) + ".1";
    std::string n2 = std::string(filename) + ".2";
    std::string n3 = std::string(filename) + ".3";

    FILE* o1 = QUtil::safe_fopen(n1.c_str(), "wb");
    FILE* o2 = QUtil::safe_fopen(n2.c_str(), "wb");
    FILE* o3 = QUtil::safe_fopen(n3.c_str(), "wb");
    Pipeline* out1 = new Pl_StdioFile("o1", o1);
    Pipeline* out2 = new Pl_StdioFile("o2", o2);
    Pipeline* out3 = new Pl_StdioFile("o3", o3);

    // Compress the file
    Pipeline* def1 = new Pl_Flate("def1", out1, Pl_Flate::a_deflate);

    // Decompress the file
    Pipeline* inf2 = new Pl_Flate("inf2", out2, Pl_Flate::a_inflate);

    // Count bytes written to o3
    Pl_Count* count3 = new Pl_Count("count3", out3);

    // Do both simultaneously
    Pipeline* inf3 = new Pl_Flate("inf3", count3, Pl_Flate::a_inflate);
    Pipeline* def3 = new Pl_Flate("def3", inf3, Pl_Flate::a_deflate);

    FILE* in1 = QUtil::safe_fopen(filename, "rb");
    unsigned char buf[1024];
    size_t len;
    while ((len = fread(buf, 1, sizeof(buf), in1)) > 0)
    {
	// Write to the compression pipeline
	def1->write(buf, len);

	// Write to the both pipeline
	def3->write(buf, len);
    }
    fclose(in1);

    def1->finish();
    delete def1;
    delete out1;
    fclose(o1);

    def3->finish();

    std::cout << "bytes written to o3: " << count3->getCount() << std::endl;


    delete def3;
    delete inf3;
    delete count3;
    delete out3;
    fclose(o3);

    // Now read the compressed data and write to the output uncompress pipeline
    FILE* in2 = QUtil::safe_fopen(n1.c_str(), "rb");
    while ((len = fread(buf, 1, sizeof(buf), in2)) > 0)
    {
	inf2->write(buf, len);
    }
    fclose(in2);

    inf2->finish();
    delete inf2;
    delete out2;
    fclose(o2);

    // At this point, filename, filename.2, and filename.3 should have
    // identical contents.  filename.1 should be a compressed version.

    std::cout << "done" << std::endl;
}

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
	std::cerr << "Usage: pipeline filename" << std::endl;
	exit(2);
    }
    char* filename = argv[1];

    try
    {
	run(filename);
    }
    catch (std::exception& e)
    {
	std::cout << e.what() << std::endl;
    }
    return 0;
}
