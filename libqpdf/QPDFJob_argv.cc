#include <qpdf/QPDFJob.hh>

// See "HOW TO ADD A COMMAND-LINE ARGUMENT" in README-maintainer.

#include <cstdio>
#include <ctype.h>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdlib.h>
#include <string.h>

#include <qpdf/QIntC.hh>
#include <qpdf/QPDFArgParser.hh>
#include <qpdf/QPDFCryptoProvider.hh>
#include <qpdf/QPDFJob.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>

namespace
{
    class ArgParser
    {
      public:
        ArgParser(QPDFArgParser& ap, std::shared_ptr<QPDFJob::Config> c_main);
        void parseOptions();

      private:
#include <qpdf/auto_job_decl.hh>

        void usage(std::string const& message);
        void initOptionTables();

        QPDFArgParser ap;
        std::shared_ptr<QPDFJob::Config> c_main;
        std::shared_ptr<QPDFJob::CopyAttConfig> c_copy_att;
        std::shared_ptr<QPDFJob::AttConfig> c_att;
        std::shared_ptr<QPDFJob::PagesConfig> c_pages;
        std::shared_ptr<QPDFJob::UOConfig> c_uo;
        std::shared_ptr<QPDFJob::EncConfig> c_enc;
        std::vector<std::string> accumulated_args;
        std::shared_ptr<char> pages_password;
        bool gave_input;
        bool gave_output;
    };
} // namespace

ArgParser::ArgParser(
    QPDFArgParser& ap, std::shared_ptr<QPDFJob::Config> c_main) :
    ap(ap),
    c_main(c_main),
    pages_password(nullptr),
    gave_input(false),
    gave_output(false)
{
    initOptionTables();
}

#include <qpdf/auto_job_help.hh>

void
ArgParser::initOptionTables()
{
#include <qpdf/auto_job_init.hh>
    this->ap.addFinalCheck([this]() { c_main->checkConfiguration(); });
    // add_help is defined in auto_job_help.hh
    add_help(this->ap);
    // Special case: ignore -- at the top level. This undocumented
    // behavior is for backward compatibility; it was unintentionally
    // the case prior to 10.6, and some users were relying on it.
    this->ap.selectMainOptionTable();
    this->ap.addBare("--", []() {});
}

void
ArgParser::argPositional(std::string const& arg)
{
    if (!this->gave_input) {
        c_main->inputFile(arg);
        this->gave_input = true;
    } else if (!this->gave_output) {
        c_main->outputFile(arg);
        this->gave_output = true;
    } else {
        usage("unknown argument " + arg);
    }
}

void
ArgParser::argEmpty()
{
    c_main->emptyInput();
    this->gave_input = true;
}

void
ArgParser::argReplaceInput()
{
    c_main->replaceInput();
    this->gave_output = true;
}

void
ArgParser::argVersion()
{
    auto whoami = this->ap.getProgname();
    std::cout << whoami << " version " << QPDF::QPDFVersion() << std::endl
              << "Run " << whoami
              << " --copyright to see copyright and license information."
              << std::endl;
}

void
ArgParser::argCopyright()
{
    // clang-format off
    // Make sure the output looks right on an 80-column display.
    //               1         2         3         4         5         6         7         8
    //      12345678901234567890123456789012345678901234567890123456789012345678901234567890
    std::cout
        << this->ap.getProgname()
        << " version " << QPDF::QPDFVersion() << std::endl
        << std::endl
        << "Copyright (c) 2005-2022 Jay Berkenbilt"
        << std::endl
        << "QPDF is licensed under the Apache License, Version 2.0 (the \"License\");"
        << std::endl
        << "you may not use this file except in compliance with the License."
        << std::endl
        << "You may obtain a copy of the License at"
        << std::endl
        << std::endl
        << "  http://www.apache.org/licenses/LICENSE-2.0"
        << std::endl
        << std::endl
        << "Unless required by applicable law or agreed to in writing, software"
        << std::endl
        << "distributed under the License is distributed on an \"AS IS\" BASIS,"
        << std::endl
        << "WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied."
        << std::endl
        << "See the License for the specific language governing permissions and"
        << std::endl
        << "limitations under the License."
        << std::endl
        << std::endl
        << "Versions of qpdf prior to version 7 were released under the terms"
        << std::endl
        << "of version 2.0 of the Artistic License. At your option, you may"
        << std::endl
        << "continue to consider qpdf to be licensed under those terms. Please"
        << std::endl
        << "see the manual for additional information."
        << std::endl;
    // clang-format on
}

void
ArgParser::argJsonHelp()
{
    std::cout << QPDFJob::json_out_schema_v1() << std::endl;
}

void
ArgParser::argShowCrypto()
{
    auto crypto = QPDFCryptoProvider::getRegisteredImpls();
    std::string default_crypto = QPDFCryptoProvider::getDefaultProvider();
    std::cout << default_crypto << std::endl;
    for (auto const& iter : crypto) {
        if (iter != default_crypto) {
            std::cout << iter << std::endl;
        }
    }
}

void
ArgParser::argEncrypt()
{
    this->accumulated_args.clear();
    if (this->ap.isCompleting() && this->ap.argsLeft() == 0) {
        this->ap.insertCompletion("user-password");
    }
    this->ap.selectOptionTable(O_ENCRYPTION);
}

void
ArgParser::argEncPositional(std::string const& arg)
{
    this->accumulated_args.push_back(arg);
    size_t n_args = this->accumulated_args.size();
    if (n_args < 3) {
        if (this->ap.isCompleting() && (this->ap.argsLeft() == 0)) {
            if (n_args == 1) {
                this->ap.insertCompletion("owner-password");
            } else if (n_args == 2) {
                this->ap.insertCompletion("40");
                this->ap.insertCompletion("128");
                this->ap.insertCompletion("256");
            }
        }
        return;
    }
    std::string user_password = this->accumulated_args.at(0);
    std::string owner_password = this->accumulated_args.at(1);
    std::string len_str = this->accumulated_args.at(2);
    int keylen = 0;
    if (len_str == "40") {
        keylen = 40;
        this->ap.selectOptionTable(O_40_BIT_ENCRYPTION);
    } else if (len_str == "128") {
        keylen = 128;
        this->ap.selectOptionTable(O_128_BIT_ENCRYPTION);
    } else if (len_str == "256") {
        keylen = 256;
        this->ap.selectOptionTable(O_256_BIT_ENCRYPTION);
    } else {
        usage("encryption key length must be 40, 128, or 256");
    }
    this->c_enc = c_main->encrypt(keylen, user_password, owner_password);
}

void
ArgParser::argPages()
{
    this->accumulated_args.clear();
    this->c_pages = c_main->pages();
    this->ap.selectOptionTable(O_PAGES);
}

void
ArgParser::argPagesPassword(std::string const& parameter)
{
    if (this->pages_password) {
        QTC::TC("qpdf", "QPDFJob duplicated pages password");
        usage("--password already specified for this file");
    }
    if (this->accumulated_args.size() != 1) {
        QTC::TC("qpdf", "QPDFJob misplaced pages password");
        usage("in --pages, --password must immediately follow a file name");
    }
    this->pages_password = QUtil::make_shared_cstr(parameter);
}

void
ArgParser::argPagesPositional(std::string const& arg)
{
    if (arg.empty()) {
        if (this->accumulated_args.empty()) {
            return;
        }
    } else {
        this->accumulated_args.push_back(arg);
    }

    std::string file = this->accumulated_args.at(0);
    char const* range_p = nullptr;

    size_t n_args = this->accumulated_args.size();
    if (n_args >= 2) {
        // will be copied before accumulated_args is cleared
        range_p = this->accumulated_args.at(1).c_str();
    }

    // See if the user omitted the range entirely, in which case we
    // assume "1-z".
    std::string next_file;
    if (range_p == nullptr) {
        if (arg.empty()) {
            // The filename or password was the last argument
            QTC::TC(
                "qpdf",
                "QPDFJob pages range omitted at end",
                this->pages_password ? 0 : 1);
        } else {
            // We need to accumulate some more arguments
            return;
        }
    } else {
        try {
            QUtil::parse_numrange(range_p, 0);
        } catch (std::runtime_error& e1) {
            // The range is invalid.  Let's see if it's a file.
            if (strcmp(range_p, ".") == 0) {
                // "." means the input file.
                QTC::TC("qpdf", "QPDFJob pages range omitted with .");
            } else if (QUtil::file_can_be_opened(range_p)) {
                QTC::TC("qpdf", "QPDFJob pages range omitted in middle");
                // Yup, it's a file.
            } else {
                // Give the range error
                usage(e1.what());
            }
            next_file = range_p;
            range_p = nullptr;
        }
    }
    std::string range(range_p ? range_p : "1-z");
    this->c_pages->pageSpec(file, range, this->pages_password.get());
    this->accumulated_args.clear();
    this->pages_password = nullptr;
    if (!next_file.empty()) {
        this->accumulated_args.push_back(next_file);
    }
}

void
ArgParser::argEndPages()
{
    argPagesPositional("");
    c_pages->endPages();
    c_pages = nullptr;
}

void
ArgParser::argUnderlay()
{
    this->c_uo = c_main->underlay();
    this->ap.selectOptionTable(O_UNDERLAY_OVERLAY);
}

void
ArgParser::argOverlay()
{
    this->c_uo = c_main->overlay();
    this->ap.selectOptionTable(O_UNDERLAY_OVERLAY);
}

void
ArgParser::argAddAttachment()
{
    this->c_att = c_main->addAttachment();
    this->ap.selectOptionTable(O_ATTACHMENT);
}

void
ArgParser::argCopyAttachmentsFrom()
{
    this->c_copy_att = c_main->copyAttachmentsFrom();
    this->ap.selectOptionTable(O_COPY_ATTACHMENT);
}

void
ArgParser::argEndEncryption()
{
    c_enc->endEncrypt();
    c_enc = nullptr;
}

void
ArgParser::argEnd40BitEncryption()
{
    argEndEncryption();
}

void
ArgParser::argEnd128BitEncryption()
{
    argEndEncryption();
}

void
ArgParser::argEnd256BitEncryption()
{
    argEndEncryption();
}

void
ArgParser::argUOPositional(std::string const& arg)
{
    c_uo->file(arg);
}

void
ArgParser::argEndUnderlayOverlay()
{
    c_uo->endUnderlayOverlay();
    c_uo = nullptr;
}

void
ArgParser::argAttPositional(std::string const& arg)
{
    c_att->file(arg);
}

void
ArgParser::argEndAttachment()
{
    c_att->endAddAttachment();
    c_att = nullptr;
}

void
ArgParser::argCopyAttPositional(std::string const& arg)
{
    c_copy_att->file(arg);
}

void
ArgParser::argEndCopyAttachment()
{
    c_copy_att->endCopyAttachmentsFrom();
    c_copy_att = nullptr;
}

void
ArgParser::argJobJsonHelp()
{
    std::cout << QPDFJob::job_json_schema_v1() << std::endl;
}

void
ArgParser::usage(std::string const& message)
{
    this->ap.usage(message);
}

void
ArgParser::parseOptions()
{
    try {
        this->ap.parseArgs();
    } catch (std::runtime_error& e) {
        usage(e.what());
    }
}

void
QPDFJob::initializeFromArgv(char const* const argv[], char const* progname_env)
{
    if (progname_env == nullptr) {
        progname_env = "QPDF_EXECUTABLE";
    }
    int argc = 0;
    for (auto k = argv; *k; ++k) {
        ++argc;
    }
    QPDFArgParser qap(argc, argv, progname_env);
    setMessagePrefix(qap.getProgname());
    ArgParser ap(qap, config());
    ap.parseOptions();
}
