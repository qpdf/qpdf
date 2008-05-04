
#include <qpdf/QEXC.hh>
#include <iostream>
#include <errno.h>
#include <stdlib.h>

void do_terminate()
{
    try
    {
	throw;
    }
    catch (std::exception& e)
    {
	std::cerr << "uncaught exception: " << e.what() << std::endl;
	exit(3);
    }
    exit(4);
}

void f(int n)
{
    switch (n)
    {
      case 0:
	throw QEXC::General("general exception");
	break;

      case 1:
	throw QEXC::Internal("internal error");
	break;

      case 2:
	throw QEXC::System("doing something", EINVAL);
	break;

      case 3:
	{
	    int a = -1;
	    new char[a];
	}
	break;
    }
}

int main(int argc, char* argv[])
{
    std::set_terminate(do_terminate);
    if (argc != 2)
    {
	std::cerr << "usage: qexc n" << std::endl;
	exit(2);
    }

    try
    {
	f(atoi(argv[1]));
    }
    catch (QEXC::General& e)
    {
	std::cerr << "exception: " << e.unparse() << std::endl;
	std::cerr << "what: " << e.what() << std::endl;
	exit(2);
    }
    return 0;
}
