#include <qpdf/assert_test.h>

#include <qpdf/Pl_String.hh>
#include <qpdf/QPDFLogger.hh>
#include <stdexcept>

static void
test1()
{
    // Standard behavior

    auto logger = QPDFLogger::defaultLogger();

    logger->info("info to stdout\n");
    logger->warn("warn to stderr\n");
    logger->error("error to stderr\n");
    assert(logger->getSave(true) == nullptr);
    try {
        logger->getSave();
        assert(false);
    } catch (std::logic_error& e) {
        *(logger->getInfo()) << "getSave exception: " << e.what() << "\n";
    }
    try {
        logger->saveToStandardOutput(true);
        assert(false);
    } catch (std::logic_error& e) {
        *(logger->getInfo()) << "saveToStandardOutput exception: " << e.what() << "\n";
    }
    logger->setWarn(logger->discard());
    logger->warn("warning not seen\n");
    logger->setWarn(nullptr);
    logger->warn("restored warning to stderr\n");
}

static void
test2()
{
    // First call saveToStandardOutput. Then use info, which then to
    // go stderr.
    auto l = QPDFLogger::create();
    l->saveToStandardOutput(true);
    l->info(std::string("info to stderr\n"));
    *(l->getSave()) << "save to stdout\n";
    l->setInfo(nullptr);
    l->info("info still to stderr\n");
    l->setSave(nullptr, false);
    l->setInfo(nullptr);
    l->info("info back to stdout\n");
}

static void
test3()
{
    // Error/warning
    auto l = QPDFLogger::create();

    // Warning follows error when error is set explicitly.
    std::string errors;
    auto pl_error = std::make_shared<Pl_String>("errors", nullptr, errors);
    l->setError(pl_error);
    l->warn("warn follows error\n");
    assert(errors == "warn follows error\n");
    l->error("error too\n");
    assert(errors == "warn follows error\nerror too\n");

    // Set warnings -- now they're separate
    std::string warnings;
    auto pl_warn = std::make_shared<Pl_String>("warnings", nullptr, warnings);
    l->setWarn(pl_warn);
    l->warn(std::string("warning now separate\n"));
    l->error(std::string("new error\n"));
    assert(warnings == "warning now separate\n");
    assert(errors == "warn follows error\nerror too\nnew error\n");
    std::string errors2;
    pl_error = std::make_shared<Pl_String>("errors", nullptr, errors2);
    l->setError(pl_error);
    l->warn("new warning\n");
    l->error("another new error\n");
    assert(warnings == "warning now separate\nnew warning\n");
    assert(errors == "warn follows error\nerror too\nnew error\n");
    assert(errors2 == "another new error\n");

    // Restore warnings to default -- follows error again
    l->setWarn(nullptr);
    l->warn("warning 3\n");
    l->error("error 3\n");
    assert(warnings == "warning now separate\nnew warning\n");
    assert(errors == "warn follows error\nerror too\nnew error\n");
    assert(errors2 == "another new error\nwarning 3\nerror 3\n");

    // Restore everything to default
    l->setInfo(nullptr);
    l->setWarn(nullptr);
    l->setError(nullptr);
    l->info("after reset, info to stdout\n");
    l->warn("after reset, warn to stderr\n");
    l->error("after reset, error to stderr\n");
}

int
main()
{
    test1();
    test2();
    test3();
    return 0;
}
