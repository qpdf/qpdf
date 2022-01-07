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

    ap.addHelpFooter("For more help, read the manual.\n");
    ap.addHelpTopic(
        "quack", "Quack Options",
        "Just put stuff after quack to get a count at the end.\n");
    ap.addHelpTopic(
        "baaa", "Baaa Options",
        "Ewe can do sheepish things.\n"
        "For example, ewe can add more ram to your computer.\n");
    ap.addOptionHelp("--ewe", "baaa",
                     "just for ewe", "You are not a ewe.\n");
    ap.addOptionHelp("--ram", "baaa", "curly horns", "");
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
    auto err = [](char const* msg, std::function<void()> fn) {
        try
        {
            fn();
            assert(msg == nullptr);
        }
        catch (std::exception& e)
        {
            std::cout << msg << ": " << e.what() << std::endl;
        }
    };

    err("duplicate handler", [this]() {
        ap.selectMainOptionTable();
        ap.addBare("potato", [](){});
    });
    err("duplicate handler", [this]() {
        ap.selectOptionTable("baaa");
        ap.addBare("ram", [](){});
    });
    err("duplicate table", [this]() {
        ap.registerOptionTable("baaa", nullptr);
    });
    err("unknown table", [this]() {
        ap.selectOptionTable("aardvark");
    });
    err("copy from unknown table", [this]() {
        ap.copyFromOtherTable("one", "two");
    });
    err("copy unknown from other table", [this]() {
        ap.copyFromOtherTable("two", "baaa");
    });
    err("add existing help topic", [this]() {
        ap.addHelpTopic("baaa", "potato", "salad");
    });
    err("add reserved help topic", [this]() {
        ap.addHelpTopic("all", "potato", "salad");
    });
    err("add to unknown topic", [this]() {
        ap.addOptionHelp("--new", "oops", "potato", "salad");
    });
    err("bad option for help", [this]() {
        ap.addOptionHelp("nodash", "baaa", "potato", "salad");
    });
    err("bad topic for help", [this]() {
        ap.addHelpTopic("--dashes", "potato", "salad");
    });
    err("duplicate option help", [this]() {
        ap.addOptionHelp("--ewe", "baaa", "potato", "salad");
    });
    err("invalid choice handler to unknown", [this]() {
        ap.addInvalidChoiceHandler(
            "elephant", [](char*){});
    });
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
