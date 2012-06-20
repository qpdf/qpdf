#include <qpdf/Pl_Flate.hh>
#include <qpdf/Pl_StdioFile.hh>
#include <qpdf/QUtil.hh>

#include <stdio.h>
#include <string.h>
#include <iostream>
#include <stdlib.h>
#include <fcntl.h>

static char const* whoami = 0;

void usage()
{
    std::cerr << "Usage: " << whoami << " { -uncompress | -compress }"
	      << std::endl;
    exit(2);
}

int main(int argc, char* argv[])
{
    if ((whoami = strrchr(argv[0], '/')) == NULL)
    {
	whoami = argv[0];
    }
    else
    {
	++whoami;
    }
    // For libtool's sake....
    if (strncmp(whoami, "lt-", 3) == 0)
    {
	whoami += 3;
    }

    if ((argc == 2) && (strcmp(argv[1], "--version") == 0))
    {
	std::cout << whoami << " version 1.0" << std::endl;
	exit(0);
    }

    if (argc != 2)
    {
	usage();
    }

    Pl_Flate::action_e action = Pl_Flate::a_inflate;

    if ((strcmp(argv[1], "-uncompress") == 0))
    {
	// okay
    }
    else if ((strcmp(argv[1], "-compress") == 0))
    {
	action = Pl_Flate::a_deflate;
    }
    else
    {
	usage();
    }

    QUtil::binary_stdout();
    QUtil::binary_stdin();
    Pl_StdioFile* out = new Pl_StdioFile("stdout", stdout);
    Pl_Flate* flate = new Pl_Flate("flate", out, action);

    try
    {
	unsigned char buf[10000];
	bool done = false;
	while (! done)
	{
	    size_t len = fread(buf, 1, sizeof(buf), stdin);
	    if (len <= 0)
	    {
		done = true;
	    }
	    else
	    {
		flate->write(buf, len);
	    }
	}
	flate->finish();
	delete flate;
	delete out;
    }
    catch (std::exception& e)
    {
	std::cerr << e.what() << std::endl;
	exit(2);
    }

    return 0;
}
