//
// This example illustrates the use of QPDFObjectHandle::TokenFilter
// with addContentTokenFilter. Please see comments inline for details.
// See also pdf-count-strings.cc for a use of
// QPDFObjectHandle::TokenFilter with filterContents.
//

#include <algorithm>
#include <deque>
#include <iostream>
#include <stdlib.h>
#include <string.h>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QUtil.hh>

static char const* whoami = nullptr;

void
usage()
{
    std::cerr << "Usage: " << whoami << " infile outfile" << std::endl
              << "Applies token filters to infile and writes outfile"
              << std::endl;
    exit(2);
}

// The StringReverser class is a trivial example of using a token
// filter. This class only overrides the pure virtual handleToken
// function and preserves the default handleEOF function.
class StringReverser: public QPDFObjectHandle::TokenFilter
{
  public:
    virtual ~StringReverser() = default;
    virtual void handleToken(QPDFTokenizer::Token const&);
};

void
StringReverser::handleToken(QPDFTokenizer::Token const& token)
{
    // For string tokens, reverse the characters. For other tokens,
    // just pass them through. Notice that we construct a new string
    // token and write that, thus allowing the library to handle any
    // subtleties about properly encoding unprintable characters. This
    // function doesn't handle multibyte characters at all. It's not
    // intended to be an example of the correct way to reverse
    // strings. It's just intended to give a simple example of a
    // pretty minimal filter and to show an example of writing a
    // constructed token.
    if (token.getType() == QPDFTokenizer::tt_string) {
        std::string value = token.getValue();
        std::reverse(value.begin(), value.end());
        writeToken(QPDFTokenizer::Token(QPDFTokenizer::tt_string, value));
    } else {
        writeToken(token);
    }
}

// The ColorToGray filter finds all "rg" operators in the content
// stream and replaces them with "g" operators, thus mapping color to
// grayscale. Note that it only applies to content streams, not
// images, so this will not replace color images with grayscale
// images.
class ColorToGray: public QPDFObjectHandle::TokenFilter
{
  public:
    virtual ~ColorToGray() = default;
    virtual void handleToken(QPDFTokenizer::Token const&);
    virtual void handleEOF();

  private:
    bool isNumeric(QPDFTokenizer::token_type_e);
    bool isIgnorable(QPDFTokenizer::token_type_e);
    double numericValue(QPDFTokenizer::Token const&);

    std::deque<QPDFTokenizer::Token> all_stack;
    std::deque<QPDFTokenizer::Token> stack;
};

bool
ColorToGray::isNumeric(QPDFTokenizer::token_type_e token_type)
{
    return (
        (token_type == QPDFTokenizer::tt_integer) ||
        (token_type == QPDFTokenizer::tt_real));
}

bool
ColorToGray::isIgnorable(QPDFTokenizer::token_type_e token_type)
{
    return (
        (token_type == QPDFTokenizer::tt_space) ||
        (token_type == QPDFTokenizer::tt_comment));
}

double
ColorToGray::numericValue(QPDFTokenizer::Token const& token)
{
    return QPDFObjectHandle::parse(token.getValue()).getNumericValue();
}

void
ColorToGray::handleToken(QPDFTokenizer::Token const& token)
{
    // Track the number of non-ignorable tokens we've seen. If we see
    // an "rg" following three numbers, convert it to a grayscale
    // value. Keep writing tokens to the output as we can.

    // There are several things to notice here. We keep two stacks:
    // one of "meaningful" tokens, and one of all tokens. This way we
    // can preserve whitespace or comments that we encounter in the
    // stream and there preserve layout. As we receive tokens, we keep
    // the last four meaningful tokens. If we see three numbers
    // followed by rg, we use the three numbers to calculate a gray
    // value that is perceptually similar to the color value and then
    // write the "g" operator to the output, discarding any spaces or
    // comments encountered embedded in the "rg" operator.

    // The stack and all_stack members are updated in such a way that
    // they always contain exactly the same non-ignorable tokens. The
    // stack member contains the tokens that would be left if you
    // removed all space and comment tokens from all_stack.

    // On each new token, flush out any space or comment tokens. Store
    // the incoming token. If we just got an rg preceded by the right
    // kinds of operands, replace the command. Flush any additional
    // accumulated tokens to keep the stack only four tokens deep.

    while ((!this->all_stack.empty()) &&
           isIgnorable(this->all_stack.at(0).getType())) {
        writeToken(this->all_stack.at(0));
        this->all_stack.pop_front();
    }
    this->all_stack.push_back(token);
    QPDFTokenizer::token_type_e token_type = token.getType();
    if (!isIgnorable(token_type)) {
        this->stack.push_back(token);
        if ((this->stack.size() == 4) && token.isWord("rg") &&
            (isNumeric(this->stack.at(0).getType())) &&
            (isNumeric(this->stack.at(1).getType())) &&
            (isNumeric(this->stack.at(2).getType()))) {
            double r = numericValue(this->stack.at(0));
            double g = numericValue(this->stack.at(1));
            double b = numericValue(this->stack.at(2));
            double gray = ((0.3 * r) + (0.59 * b) + (0.11 * g));
            if (gray > 1.0) {
                gray = 1.0;
            }
            if (gray < 0.0) {
                gray = 0.0;
            }
            write(QUtil::double_to_string(gray, 3));
            write(" g");
            this->stack.clear();
            this->all_stack.clear();
        }
    }
    if (this->stack.size() == 4) {
        writeToken(this->all_stack.at(0));
        this->all_stack.pop_front();
        this->stack.pop_front();
    }
}

void
ColorToGray::handleEOF()
{
    // Flush out any remaining accumulated tokens.
    while (!this->all_stack.empty()) {
        writeToken(this->all_stack.at(0));
        this->all_stack.pop_front();
    }
}

int
main(int argc, char* argv[])
{
    whoami = QUtil::getWhoami(argv[0]);

    if (argc != 3) {
        usage();
    }
    char const* infilename = argv[1];
    char const* outfilename = argv[2];

    try {
        QPDF pdf;
        pdf.processFile(infilename);
        for (auto& page: QPDFPageDocumentHelper(pdf).getAllPages()) {
            // Attach two token filters to each page of this file.
            // When the file is written, or when the pages' contents
            // are retrieved in any other way, the filters will be
            // applied. See comments on the filters for additional
            // details.
            page.addContentTokenFilter(
                std::shared_ptr<QPDFObjectHandle::TokenFilter>(
                    new StringReverser));
            page.addContentTokenFilter(
                std::shared_ptr<QPDFObjectHandle::TokenFilter>(
                    new ColorToGray));
        }

        QPDFWriter w(pdf, outfilename);
        w.setStaticID(true); // for testing only
        w.write();
    } catch (std::exception& e) {
        std::cerr << whoami << ": " << e.what() << std::endl;
        exit(2);
    }

    return 0;
}
