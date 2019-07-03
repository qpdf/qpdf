#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFOutlineDocumentHelper.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/QTC.hh>

// This program demonstrates extraction of bookmarks using the qpdf
// outlines API. Note that all the information shown by this program
// can also be obtained from a PDF file using qpdf's --json option.

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
    QPDFPageDocumentHelper dh(qpdf);
    std::vector<QPDFPageObjectHelper> pages = dh.getAllPages();
    int n = 0;
    for (std::vector<QPDFPageObjectHelper>::iterator iter = pages.begin();
	 iter != pages.end(); ++iter)
    {
	QPDFObjectHandle oh = (*iter).getObjectHandle();
	page_map[oh.getObjGen()] = ++n;
    }
}

void show_bookmark_details(QPDFOutlineObjectHelper outline,
                           std::vector<int> numbers)
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
        int count = outline.getCount();
        if (count)
        {
            QTC::TC("examples", "pdf-bookmarks has count");
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
        QPDFObjectHandle dest_page = outline.getDestPage();
        if (! dest_page.isNull())
        {
            QTC::TC("examples", "pdf-bookmarks dest");
            QPDFObjGen og = dest_page.getObjGen();
            if (page_map.count(og))
            {
                target = QUtil::int_to_string(page_map[og]);
            }
        }
        std::cout << "[ -> " << target << " ] ";
    }

    std::cout << outline.getTitle() << std::endl;
}

void extract_bookmarks(std::vector<QPDFOutlineObjectHelper> outlines,
                       std::vector<int>& numbers)
{
    numbers.push_back(0);
    for (std::vector<QPDFOutlineObjectHelper>::iterator iter = outlines.begin();
         iter != outlines.end(); ++iter)
    {
        ++(numbers.back());
        show_bookmark_details(*iter, numbers);
        std::vector<QPDFOutlineObjectHelper>::iterator next = iter;
        ++next;
        bool has_next = (next != outlines.end());
        if ((style == st_lines) && (! has_next))
        {
            numbers.back() = 0;
        }
        extract_bookmarks((*iter).getKids(), numbers);
    }
    numbers.pop_back();
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

        QPDFOutlineDocumentHelper odh(qpdf);
	if (odh.hasOutlines())
	{
	    std::vector<int> numbers;
	    if (show_targets)
	    {
		generate_page_map(qpdf);
	    }
	    extract_bookmarks(odh.getTopLevelOutlines(), numbers);
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
