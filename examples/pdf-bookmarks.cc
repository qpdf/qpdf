#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <qpdf/QPDF.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/QTC.hh>

static char const* whoami = 0;
static enum { st_none, st_numbers, st_lines } style = st_none;
static bool show_open = false;
static bool show_targets = false;
static std::map<QPDFObjGen, int> page_map;

void usage()
{
    std::cerr << "Usage: " << whoami << " [options] file.pdf [password]"
	      << std::endl
	      << "Options:" << std::endl
	      << "  -numbers        give bookmarks outline-style numbers"
	      << std::endl
	      << "  -lines          draw lines to show bookmark hierarchy"
	      << std::endl
	      << "  -show-open      indicate whether a bookmark is initially open"
	      << std::endl
	      << "  -show-targets   show target if possible"
	      << std::endl;
    exit(2);
}

void print_lines(std::vector<int>& numbers)
{
    for (unsigned int i = 0; i < numbers.size() - 1; ++i)
    {
	if (numbers.at(i))
	{
	    std::cout << "| ";
	}
	else
	{
	    std::cout << "  ";
	}
    }
}

void generate_page_map(QPDF& qpdf)
{
    std::vector<QPDFObjectHandle> pages = qpdf.getAllPages();
    int n = 0;
    for (std::vector<QPDFObjectHandle>::iterator iter = pages.begin();
	 iter != pages.end(); ++iter)
    {
	QPDFObjectHandle& oh = *iter;
	page_map[oh.getObjGen()] = ++n;
    }
}

void extract_bookmarks(QPDFObjectHandle outlines, std::vector<int>& numbers)
{
    if (outlines.hasKey("/Title"))
    {
	// No default so gcc will warn on missing tag
	switch (style)
	{
	  case st_none:
	    QTC::TC("examples", "pdf-bookmarks none");
	    break;

	  case st_numbers:
	    QTC::TC("examples", "pdf-bookmarks numbers");
	    for (std::vector<int>::iterator iter = numbers.begin();
		 iter != numbers.end(); ++iter)
	    {
		std::cout << *iter << ".";
	    }
	    std::cout << " ";
	    break;

	  case st_lines:
	    QTC::TC("examples", "pdf-bookmarks lines");
	    print_lines(numbers);
	    std::cout << "|" << std::endl;
	    print_lines(numbers);
	    std::cout << "+-+ ";
	    break;
	}

	if (show_open)
	{
	    if (outlines.hasKey("/Count"))
	    {
		QTC::TC("examples", "pdf-bookmarks has count");
		int count = outlines.getKey("/Count").getIntValue();
		if (count > 0)
		{
		    // hierarchy is open at this point
		    QTC::TC("examples", "pdf-bookmarks open");
		    std::cout << "(v) ";
		}
		else
		{
		    QTC::TC("examples", "pdf-bookmarks closed");
		    std::cout << "(>) ";
		}
	    }
	    else
	    {
		QTC::TC("examples", "pdf-bookmarks no count");
		std::cout << "( ) ";
	    }
	}

	if (show_targets)
	{
	    QTC::TC("examples", "pdf-bookmarks targets");
	    std::string target = "unknown";
	    // Only explicit destinations supported for now
	    if (outlines.hasKey("/Dest"))
	    {
		QTC::TC("examples", "pdf-bookmarks dest");
		QPDFObjectHandle dest = outlines.getKey("/Dest");
		if ((dest.isArray()) && (dest.getArrayNItems() > 0))
		{
		    QPDFObjectHandle first = dest.getArrayItem(0);
		    QPDFObjGen og = first.getObjGen();
		    if (page_map.count(og))
		    {
			target = QUtil::int_to_string(page_map[og]);
		    }
		}

		std::cout << "[ -> " << target << " ] ";
	    }
	}

	std::cout << outlines.getKey("/Title").getUTF8Value() << std::endl;
    }

    if (outlines.hasKey("/First"))
    {
	numbers.push_back(0);
	QPDFObjectHandle child = outlines.getKey("/First");
	while (1)
	{
	    ++(numbers.back());
	    bool has_next = child.hasKey("/Next");
	    if ((style == st_lines) && (! has_next))
	    {
		numbers.back() = 0;
	    }
	    extract_bookmarks(child, numbers);
	    if (has_next)
	    {
		child = child.getKey("/Next");
	    }
	    else
	    {
		break;
	    }
	}
	numbers.pop_back();
    }
}

int main(int argc, char* argv[])
{
    whoami = QUtil::getWhoami(argv[0]);

    // For libtool's sake....
    if (strncmp(whoami, "lt-", 3) == 0)
    {
	whoami += 3;
    }

    if ((argc == 2) && (strcmp(argv[1], "--version") == 0))
    {
	std::cout << whoami << " version 1.5" << std::endl;
	exit(0);
    }

    int arg;
    for (arg = 1; arg < argc; ++arg)
    {
	if (argv[arg][0] == '-')
	{
	    if (strcmp(argv[arg], "-numbers") == 0)
	    {
		style = st_numbers;
	    }
	    else if (strcmp(argv[arg], "-lines") == 0)
	    {
		style = st_lines;
	    }
	    else if (strcmp(argv[arg], "-show-open") == 0)
	    {
		show_open = true;
	    }
	    else if (strcmp(argv[arg], "-show-targets") == 0)
	    {
		show_targets = true;
	    }
	    else
	    {
		usage();
	    }
	}
	else
	{
	    break;
	}
    }

    if (arg >= argc)
    {
	usage();
    }

    char const* filename = argv[arg++];
    char const* password = "";

    if (arg < argc)
    {
	password = argv[arg++];
    }
    if (arg != argc)
    {
	usage();
    }

    try
    {
	QPDF qpdf;
	qpdf.processFile(filename, password);

	QPDFObjectHandle root = qpdf.getRoot();
	if (root.hasKey("/Outlines"))
	{
	    std::vector<int> numbers;
	    if (show_targets)
	    {
		generate_page_map(qpdf);
	    }
	    extract_bookmarks(root.getKey("/Outlines"), numbers);
	}
	else
	{
	    std::cout << filename << " has no bookmarks" << std::endl;
	}
    }
    catch (std::exception &e)
    {
	std::cerr << whoami << " processing file " << filename << ": "
		  << e.what() << std::endl;
	exit(2);
    }

    return 0;
}
