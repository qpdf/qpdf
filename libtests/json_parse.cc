#include <qpdf/FileInputSource.hh>
#include <qpdf/JSON.hh>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>

namespace
{
    class Reactor: public JSON::Reactor
    {
      public:
        virtual ~Reactor() = default;
        void dictionaryStart() override;
        void arrayStart() override;
        void containerEnd(JSON const& value) override;
        void topLevelScalar() override;
        bool
        dictionaryItem(std::string const& key, JSON const& value) override;
        bool arrayItem(JSON const& value) override;

      private:
        void printItem(JSON const&);
    };
} // namespace

void
Reactor::dictionaryStart()
{
    std::cout << "dictionary start" << std::endl;
}

void
Reactor::arrayStart()
{
    std::cout << "array start" << std::endl;
}

void
Reactor::containerEnd(JSON const& value)
{
    std::cout << "container end: ";
    printItem(value);
}

void
Reactor::topLevelScalar()
{
    std::cout << "top-level scalar" << std::endl;
}

bool
Reactor::dictionaryItem(std::string const& key, JSON const& value)
{
    std::cout << "dictionary item: " << key << " -> ";
    printItem(value);
    if (key == "keep") {
        return false;
    }
    return true;
}

bool
Reactor::arrayItem(JSON const& value)
{
    std::cout << "array item: ";
    printItem(value);
    std::string n;
    if (value.getString(n) && n == "keep") {
        return false;
    }
    return true;
}

void
Reactor::printItem(JSON const& j)
{
    std::cout << "[" << j.getStart() << ", " << j.getEnd()
              << "): " << j.unparse() << std::endl;
}

static void
usage()
{
    std::cerr << "Usage: json_parse file [--react]" << std::endl;
    exit(2);
}

int
main(int argc, char* argv[])
{
    if ((argc < 2) || (argc > 3)) {
        usage();
        return 2;
    }
    char const* filename = argv[1];
    std::shared_ptr<Reactor> reactor;
    if (argc == 3) {
        if (strcmp(argv[2], "--react") == 0) {
            reactor = std::make_shared<Reactor>();
        } else {
            usage();
        }
    }
    try {
        FileInputSource is(filename);
        std::cout << JSON::parse(is, reactor.get()).unparse() << std::endl;
    } catch (std::exception& e) {
        std::cerr << "exception: " << filename << ": " << e.what() << std::endl;
        return 2;
    }
    return 0;
}
