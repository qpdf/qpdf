#include <qpdf/Buffer.hh>
#include <qpdf/BufferInputSource.hh>
#include <qpdf/QPDFTokenizer.hh>
#include <cstring>
#include <iostream>

static std::shared_ptr<Buffer>
get_buffer()
{
    size_t size = 3172;
    std::shared_ptr<Buffer> b(new Buffer(size));
    unsigned char* p = b->getBuffer();
    for (size_t i = 0; i < size; ++i) {
        p[i] = static_cast<unsigned char>(i & 0xff);
    }
    return b;
}

class Finder: public InputSource::Finder
{
  public:
    Finder(std::shared_ptr<InputSource> is, std::string const& after) :
        is(is),
        after(after)
    {
    }
    virtual ~Finder()
    {
    }
    virtual bool check();

  private:
    std::shared_ptr<InputSource> is;
    std::string after;
};

bool
Finder::check()
{
    QPDFTokenizer tokenizer;
    QPDFTokenizer::Token t = tokenizer.readToken(is, "finder", true);
    if (t == QPDFTokenizer::Token(QPDFTokenizer::tt_word, "potato")) {
        t = tokenizer.readToken(is, "finder", true);
        return (t == QPDFTokenizer::Token(QPDFTokenizer::tt_word, after));
    }
    return false;
}

void
check(char const* description, bool expected, bool actual)
{
    std::cout << description << ": " << ((actual == expected) ? "PASS" : "FAIL")
              << std::endl;
}

int
main()
{
    std::shared_ptr<Buffer> b1 = get_buffer();
    unsigned char* b = b1->getBuffer();
    // Straddle block boundaries
    memcpy(b + 1022, "potato", 6);
    // Overlap so that the first check() would advance past the start
    // of the next match
    memcpy(b + 2037, "potato potato salad ", 20);
    auto is = std::shared_ptr<InputSource>(
        new BufferInputSource("test buffer input source", b1.get()));
    Finder f1(is, "salad");
    check("find potato salad", true, is->findFirst("potato", 0, 0, f1));
    check(
        "barely find potato salad",
        true,
        is->findFirst("potato", 1100, 945, f1));
    check(
        "barely find potato salad",
        true,
        is->findFirst("potato", 2000, 45, f1));
    check(
        "potato salad is too late",
        false,
        is->findFirst("potato", 1100, 944, f1));
    check(
        "potato salad is too late",
        false,
        is->findFirst("potato", 2000, 44, f1));
    check(
        "potato salad not found", false, is->findFirst("potato", 2045, 0, f1));
    check("potato salad not found", false, is->findFirst("potato", 0, 1, f1));

    // Put one more right at EOF
    memcpy(b + b1->getSize() - 12, "potato salad", 12);
    check("potato salad at EOF", true, is->findFirst("potato", 3000, 0, f1));

    is->findFirst("potato", 0, 0, f1);
    check("findFirst found first", true, is->tell() == 2056);
    check(
        "findLast found potato salad", true, is->findLast("potato", 0, 0, f1));
    check("findLast found at EOF", true, is->tell() == 3172);

    // Make check() bump into EOF
    memcpy(b + b1->getSize() - 6, "potato", 6);
    check(
        "potato but not salad salad at EOF",
        false,
        is->findFirst("potato", 3000, 0, f1));
    check(
        "findLast found potato salad", true, is->findLast("potato", 0, 0, f1));
    check("findLast found first one", true, is->tell() == 2056);

    return 0;
}
