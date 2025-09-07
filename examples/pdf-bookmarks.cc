#include <qpdf/QIntC.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFOutlineDocumentHelper.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>
#include <cstdlib>
#include <cstring>
#include <iostream>

// This program demonstrates extraction of bookmarks using the qpdf outlines API. Note that all the
// information shown by this program can also be obtained from a PDF file using qpdf's --json
// option.
//
// Ignore calls to QTC::TC - they are for qpdf CI testing only.

static char const* whoami = nullptr;
static enum { st_none, st_numbers, st_lines } style = st_none;
static bool show_open = false;
static bool show_targets = false;
static std::map<QPDFObjGen, int> page_map;

void
usage()
{
    std::cerr << "Usage: " << whoami << " [options] file.pdf [password]\n"
              << "Options:\n"
              << "  --numbers        give bookmarks outline-style numbers\n"
              << "  --lines          draw lines to show bookmark hierarchy\n"
              << "  --show-open      indicate whether a bookmark is initially open\n"
              << "  --show-targets   show target if possible\n";
    exit(2);
}

void
print_lines(std::vector<int>& numbers)
{
    for (unsigned int i = 0; i < numbers.size() - 1; ++i) {
        if (numbers.at(i)) {
            std::cout << "| ";
        } else {
            std::cout << "  ";
        }
    }
}

void
generate_page_map(QPDF& qpdf)
{
    auto& dh = QPDFPageDocumentHelper::get(qpdf);
    int n = 0;
    for (auto const& page: dh.getAllPages()) {
        page_map[page] = ++n;
    }
}

void
show_bookmark_details(QPDFOutlineObjectHelper outline, std::vector<int> numbers)
{
    // No default so gcc will warn on missing tag
    switch (style) {
    case st_none:
        break;

    case st_numbers:
        for (auto const& number: numbers) {
            std::cout << number << ".";
        }
        std::cout << " ";
        break;

    case st_lines:
        print_lines(numbers);
        std::cout << "|\n";
        print_lines(numbers);
        std::cout << "+-+ ";
        break;
    }

    if (show_open) {
        int count = outline.getCount();
        if (count) {
            if (count > 0) {
                // hierarchy is open at this point
                std::cout << "(v) ";
            } else {
                std::cout << "(>) ";
            }
        } else {
            std::cout << "( ) ";
        }
    }

    if (show_targets) {
        std::string target = "unknown";
        QPDFObjectHandle dest_page = outline.getDestPage();
        if (!dest_page.isNull()) {
            if (page_map.contains(dest_page)) {
                target = std::to_string(page_map[dest_page]);
            }
        }
        std::cout << "[ -> " << target << " ] ";
    }

    std::cout << outline.getTitle() << '\n';
}

void
extract_bookmarks(std::vector<QPDFOutlineObjectHelper> outlines, std::vector<int>& numbers)
{
    // For style == st_numbers, numbers.at(n) contains the numerical label for the outline, so we
    // count up from 1.
    // For style == st_lines, numbers.at(n) == 0 indicates the last outline at level n, and we don't
    // otherwise care what the value is, so we count up to zero.
    numbers.push_back((style == st_lines) ? -QIntC::to_int(outlines.size()) : 0);
    for (auto& outline: outlines) {
        ++(numbers.back());
        show_bookmark_details(outline, numbers);
        extract_bookmarks(outline.getKids(), numbers);
    }
    numbers.pop_back();
}

int
main(int argc, char* argv[])
{
    whoami = QUtil::getWhoami(argv[0]);

    if ((argc == 2) && (strcmp(argv[1], "--version") == 0)) {
        std::cout << whoami << " version 1.5\n";
        exit(0);
    }

    int arg;
    for (arg = 1; arg < argc; ++arg) {
        if (argv[arg][0] == '-') {
            if (strcmp(argv[arg], "--numbers") == 0) {
                style = st_numbers;
            } else if (strcmp(argv[arg], "--lines") == 0) {
                style = st_lines;
            } else if (strcmp(argv[arg], "--show-open") == 0) {
                show_open = true;
            } else if (strcmp(argv[arg], "--show-targets") == 0) {
                show_targets = true;
            } else {
                usage();
            }
        } else {
            break;
        }
    }

    if (arg >= argc) {
        usage();
    }

    char const* filename = argv[arg++];
    char const* password = "";

    if (arg < argc) {
        password = argv[arg++];
    }
    if (arg != argc) {
        usage();
    }

    try {
        QPDF qpdf;
        qpdf.processFile(filename, password);

        auto& odh = QPDFOutlineDocumentHelper::get(qpdf);
        if (odh.hasOutlines()) {
            std::vector<int> numbers;
            if (show_targets) {
                generate_page_map(qpdf);
            }
            extract_bookmarks(odh.getTopLevelOutlines(), numbers);
        } else {
            std::cout << filename << " has no bookmarks\n";
        }
    } catch (std::exception& e) {
        std::cerr << whoami << " processing file " << filename << ": " << e.what() << '\n';
        exit(2);
    }

    return 0;
}
