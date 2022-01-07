#include <qpdf/QPDFArgParser.hh>
#include <qpdf/QUtil.hh>
#include <iostream>
#include <cstring>
#include <cassert>

class ArgParser
{
  public:
    ArgParser(int argc, char* argv[]);
    void parseArgs();

    void test_exceptions();

  private:
    void handlePotato();
    void handleSalad(char* p);
    void handleMoo(char* p);
    void handleOink(char* p);
    void handleQuack(char* p);
    void startQuack();
    void getQuack(char* p);
    void endQuack();
    void finalChecks();

    void initOptions();
    void output(std::string const&);

    QPDFArgParser ap;
    int quacks;
};

ArgParser::ArgParser(int argc, char* argv[]) :
    ap(QPDFArgParser(argc, argv, "TEST_ARG_PARSER")),
    quacks(0)
{
    initOptions();
}

void
ArgParser::initOptions()
{
    auto b = [this](void (ArgParser::*f)()) {
        return QPDFArgParser::bindBare(f, this);
    };
    auto p = [this](void (ArgParser::*f)(char *)) {
        return QPDFArgParser::bindParam(f, this);
    };

    ap.addBare("potato", b(&ArgParser::handlePotato));
    ap.addRequiredParameter("salad", p(&ArgParser::handleSalad), "tossed");
    ap.addOptionalParameter("moo", p(&ArgParser::handleMoo));
    char const* choices[] = {"pig", "boar", "sow", 0};
    ap.addChoices("oink", p(&ArgParser::handleOink), true, choices);
    ap.selectHelpOptionTable();
    ap.addBare("version", [this](){ output("3.14159"); });
    ap.selectMainOptionTable();
    ap.addBare("quack", b(&ArgParser::startQuack));
    ap.registerOptionTable("quack", b(&ArgParser::endQuack));
    ap.addPositional(p(&ArgParser::getQuack));
    ap.addFinalCheck(b(&ArgParser::finalChecks));
    ap.selectMainOptionTable();
    ap.addBare("baaa", [this](){ this->ap.selectOptionTable("baaa"); });
    ap.registerOptionTable("baaa", nullptr);
    ap.addBare("ewe", [this](){ output("you"); });
    ap.addBare("ram", [this](){ output("ram"); });
    ap.selectMainOptionTable();
    ap.addBare("sheep", [this](){ this->ap.selectOptionTable("sheep"); });
    ap.registerOptionTable("sheep", nullptr);
    ap.copyFromOtherTable("ewe", "baaa");
}

void
ArgParser::output(std::string const& msg)
{
    if (! this->ap.isCompleting())
    {
        std::cout << msg << std::endl;
    }
}

void
ArgParser::handlePotato()
{
    output("got potato");
}

void
ArgParser::handleSalad(char* p)
{
    output(std::string("got salad=") + p);
}

void
ArgParser::handleMoo(char* p)
{
    output(std::string("got moo=") + (p ? p : "(none)"));
}

void
ArgParser::handleOink(char* p)
{
    output(std::string("got oink=") + p);
}

void
ArgParser::parseArgs()
{
    this->ap.parseArgs();
}

void
ArgParser::startQuack()
{
    this->ap.selectOptionTable("quack");
    if (this->ap.isCompleting())
    {
        if (this->ap.isCompleting() && (this->ap.argsLeft() == 0))
        {
            this->ap.insertCompletion("something");
            this->ap.insertCompletion("anything");
        }
        return;
    }
}

void
ArgParser::getQuack(char* p)
{
    ++this->quacks;
    if (this->ap.isCompleting() && (this->ap.argsLeft() == 0))
    {
        this->ap.insertCompletion(
            std::string("thing-") + QUtil::int_to_string(this->quacks));
        return;
    }
    output(std::string("got quack: ") + p);
}

void
ArgParser::endQuack()
{
    output("total quacks so far: " + QUtil::int_to_string(this->quacks));
}

void
ArgParser::finalChecks()
{
    output("total quacks: " + QUtil::int_to_string(this->quacks));
}

void
ArgParser::test_exceptions()
{
    try
    {
        ap.selectMainOptionTable();
        ap.addBare("potato", [](){});
        assert(false);
    }
    catch (std::exception& e)
    {
        std::cout << "duplicate handler: " << e.what() << std::endl;
    }
    try
    {
        ap.selectOptionTable("baaa");
        ap.addBare("ram", [](){});
        assert(false);
    }
    catch (std::exception& e)
    {
        std::cout << "duplicate handler: " << e.what() << std::endl;
    }
    try
    {
        ap.registerOptionTable("baaa", nullptr);
        assert(false);
    }
    catch (std::exception& e)
    {
        std::cout << "duplicate table: " << e.what() << std::endl;
    }
    try
    {
        ap.selectOptionTable("aardvark");
        assert(false);
    }
    catch (std::exception& e)
    {
        std::cout << "unknown table: " << e.what() << std::endl;
    }
    try
    {
        ap.copyFromOtherTable("one", "two");
        assert(false);
    }
    catch (std::exception& e)
    {
        std::cout << "copy from unknown table: " << e.what() << std::endl;
    }
    try
    {
        ap.copyFromOtherTable("two", "baaa");
        assert(false);
    }
    catch (std::exception& e)
    {
        std::cout << "copy unknown from other table: " << e.what() << std::endl;
    }
}

int main(int argc, char* argv[])
{
    ArgParser ap(argc, argv);
    if ((argc == 2) && (strcmp(argv[1], "exceptions") == 0))
    {
        ap.test_exceptions();
        return 0;
    }
    try
    {
        ap.parseArgs();
    }
    catch (QPDFArgParser::Usage& e)
    {
        std::cerr << "usage: " << e.what() << std::endl;
        exit(2);
    }
    catch (std::exception& e)
    {
        std::cerr << "exception: " << e.what() << std::endl;
        exit(3);
    }
    return 0;
}
