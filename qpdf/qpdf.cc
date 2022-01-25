#include <qpdf/QPDFJob.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/QPDFArgParser.hh>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

static int constexpr EXIT_ERROR = 2;
static int constexpr EXIT_WARNING = 3;

// For is-encrypted and requires-password
static int constexpr EXIT_IS_NOT_ENCRYPTED = 2;
static int constexpr EXIT_CORRECT_PASSWORD = 3;

static char const* whoami = 0;

static void usageExit(std::string const& msg)
{
    std::cerr
	<< std::endl
	<< whoami << ": " << msg << std::endl
	<< std::endl
	<< "For detailed help, run " << whoami << " --help" << std::endl
	<< std::endl;
    exit(EXIT_ERROR);
}

int realmain(int argc, char* argv[])
{
    whoami = QUtil::getWhoami(argv[0]);
    QUtil::setLineBuf(stdout);

    // Remove prefix added by libtool for consistency during testing.
    if (strncmp(whoami, "lt-", 3) == 0)
    {
	whoami += 3;
    }

    QPDFJob j;

    bool errors = false;
    try
    {
        // See "HOW TO ADD A COMMAND-LINE ARGUMENT" in README-maintainer.
        j.initializeFromArgv(argc, argv);
        j.run();
    }
    catch (QPDFArgParser::Usage& e)
    {
        usageExit(e.what());
    }
    catch (QPDFJob::ConfigError& e)
    {
        usageExit(e.what());
    }
    catch (std::exception& e)
    {
	std::cerr << whoami << ": " << e.what() << std::endl;
	errors = true;
    }

    bool warnings = j.hasWarnings();
    if (warnings)
    {
        if (! j.suppressWarnings())
        {
            if (j.createsOutput())
            {
                std::cerr << whoami << ": operation succeeded with warnings;"
                          << " resulting file may have some problems"
                          << std::endl;
            }
            else
            {
                std::cerr << whoami << ": operation succeeded with warnings"
                          << std::endl;
            }
        }
        // Still return with warning code even if warnings were
        // suppressed, so leave warnings == true unless we've been
        // specifically instructed to do otherwise.
        if (j.warningsExitZero())
        {
            warnings = false;
        }
    }

    unsigned long encryption_status = j.getEncryptionStatus();
    if (j.checkIsEncrypted())
    {
        if (encryption_status & qpdf_es_encrypted)
        {
            QTC::TC("qpdf", "qpdf check encrypted encrypted");
            return 0;
        }
        else
        {
            QTC::TC("qpdf", "qpdf check encrypted not encrypted");
            return EXIT_IS_NOT_ENCRYPTED;
        }
    }
    else if (j.checkRequiresPassword())
    {
        if (encryption_status & qpdf_es_encrypted)
        {
            if (encryption_status & qpdf_es_password_incorrect)
            {
                QTC::TC("qpdf", "qpdf check password password incorrect");
                return 0;
            }
            else
            {
                QTC::TC("qpdf", "qpdf check password password correct");
                return EXIT_CORRECT_PASSWORD;
            }
        }
        else
        {
            QTC::TC("qpdf", "qpdf check password not encrypted");
            return EXIT_IS_NOT_ENCRYPTED;
        }
    }

    return (errors ? EXIT_ERROR :
            warnings ? EXIT_WARNING :
            0);
}

#ifdef WINDOWS_WMAIN

extern "C"
int wmain(int argc, wchar_t* argv[])
{
    return QUtil::call_main_from_wmain(argc, argv, realmain);
}

#else

int main(int argc, char* argv[])
{
    return realmain(argc, argv);
}

#endif
