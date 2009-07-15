
#include <qpdf/QEXC.hh>
#include <iostream>
#include <errno.h>
#include <stdlib.h>

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
    }
}

int main(int argc, char* argv[])
{
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
    catch (std::exception& e)
    {
	std::cerr << "uncaught exception: " << e.what() << std::endl;
	exit(3);
    }
    catch (...)
    {
	exit(4);
    }
    return 0;
}
