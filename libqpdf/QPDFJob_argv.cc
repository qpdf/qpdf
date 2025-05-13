#include <qpdf/QPDFJob.hh>

// See "HOW TO ADD A COMMAND-LINE ARGUMENT" in README-maintainer.

#include <cstring>
#include <iostream>
#include <memory>

#include <qpdf/Pl_Flate.hh>
#include <qpdf/QPDFArgParser.hh>
#include <qpdf/QPDFCryptoProvider.hh>
#include <qpdf/QPDFLogger.hh>
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
        std::string user_password;
        std::string owner_password;
        bool called_pages_file{false};
        bool called_pages_range{false};
        bool used_enc_password_args{false};
        bool gave_input{false};
        bool gave_output{false};
    };
} // namespace

ArgParser::ArgParser(QPDFArgParser& ap, std::shared_ptr<QPDFJob::Config> c_main) :
    ap(ap),
    c_main(c_main)
{
    initOptionTables();
}

#include <qpdf/auto_job_help.hh>

void
ArgParser::initOptionTables()
{
#include <qpdf/auto_job_init.hh>
    ap.addFinalCheck([this]() { c_main->checkConfiguration(); });
    // add_help is defined in auto_job_help.hh
    add_help(ap);
    // Special case: ignore -- at the top level. This undocumented behavior is for backward
    // compatibility; it was unintentionally the case prior to 10.6, and some users were relying on
    // it.
    ap.selectMainOptionTable();
    ap.addBare("--", []() {});
}

void
ArgParser::argPositional(std::string const& arg)
{
    if (!gave_input) {
        c_main->inputFile(arg);
        gave_input = true;
    } else if (!gave_output) {
        c_main->outputFile(arg);
        gave_output = true;
    } else {
        usage("unknown argument " + arg);
    }
}

void
ArgParser::argEmpty()
{
    c_main->emptyInput();
    gave_input = true;
}

void
ArgParser::argReplaceInput()
{
    c_main->replaceInput();
    gave_output = true;
}

void
ArgParser::argVersion()
{
    auto whoami = ap.getProgname();
    *QPDFLogger::defaultLogger()->getInfo()
        << whoami << " version " << QPDF::QPDFVersion() << "\n"
        << "Run " << whoami << " --copyright to see copyright and license information.\n";
}

void
ArgParser::argZopfli()
{
    auto logger = QPDFLogger::defaultLogger();
    if (Pl_Flate::zopfli_supported()) {
        if (Pl_Flate::zopfli_enabled()) {
            logger->info("zopfli support is enabled, and zopfli is active\n");
        } else {
            logger->info("zopfli support is enabled but not active\n");
            logger->info("Set the environment variable QPDF_ZOPFLI to activate.\n");
            logger->info("* QPDF_ZOPFLI=disabled or QPDF_ZOPFLI not set: don't use zopfli.\n");
            logger->info("* QPDF_ZOPFLI=force: use zopfli, and fail if not available.\n");
            logger->info(
                "* QPDF_ZOPFLI=silent: use zopfli if available and silently fall back if not.\n");
            logger->info(
                "* QPDF_ZOPFLI= any other value: use zopfli if available, and warn if not.\n");
        }
    } else {
        logger->error("zopfli support is not enabled\n");
        std::exit(qpdf_exit_error);
    }
}

void
ArgParser::argCopyright()
{
    // clang-format off
    // Make sure the output looks right on an 80-column display.
    //               1         2         3         4         5         6         7         8
    //      12345678901234567890123456789012345678901234567890123456789012345678901234567890
    *QPDFLogger::defaultLogger()->getInfo()
        << ap.getProgname()
        << " version " << QPDF::QPDFVersion() << "\n"
        << "\n"
        << "Copyright (c) 2005-2021 Jay Berkenbilt\n"
        << "Copyright (c) 2022-2025 Jay Berkenbilt and Manfred Holger\n"
        << "\n"
        << "qpdf is licensed under the Apache License, Version 2.0 (the \"License\");\n"
        << "you may not use this file except in compliance with the License.\n"
        << "You may obtain a copy of the License at\n"
        << "\n"
        << "  http://www.apache.org/licenses/LICENSE-2.0\n"
        << "\n"
        << "Unless required by applicable law or agreed to in writing, software\n"
        << "distributed under the License is distributed on an \"AS IS\" BASIS,\n"
        << "WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.\n"
        << "See the License for the specific language governing permissions and\n"
        << "limitations under the License.\n"
        << "\n"
        << "Versions of qpdf prior to version 7 were released under the terms\n"
        << "of version 2.0 of the Artistic License. At your option, you may\n"
        << "continue to consider qpdf to be licensed under those terms. Please\n"
        << "see the manual for additional information.\n";
    // clang-format on
}

void
ArgParser::argJsonHelp(std::string const& parameter)
{
    int version = JSON::LATEST;
    if (!(parameter.empty() || (parameter == "latest"))) {
        version = QUtil::string_to_int(parameter.c_str());
    }
    if ((version < 1) || (version > JSON::LATEST)) {
        usage(std::string("unsupported json version ") + parameter);
    }
    *QPDFLogger::defaultLogger()->getInfo() << QPDFJob::json_out_schema(version) << "\n";
}

void
ArgParser::argShowCrypto()
{
    auto crypto = QPDFCryptoProvider::getRegisteredImpls();
    std::string default_crypto = QPDFCryptoProvider::getDefaultProvider();
    *QPDFLogger::defaultLogger()->getInfo() << default_crypto << "\n";
    for (auto const& iter: crypto) {
        if (iter != default_crypto) {
            *QPDFLogger::defaultLogger()->getInfo() << iter << "\n";
        }
    }
}

void
ArgParser::argEncrypt()
{
    c_enc = c_main->encrypt(0, "", "");
    accumulated_args.clear();
    ap.selectOptionTable(O_ENCRYPTION);
}

void
ArgParser::argEncPositional(std::string const& arg)
{
    if (used_enc_password_args) {
        usage("positional and dashed encryption arguments may not be mixed");
    }

    accumulated_args.push_back(arg);
    if (accumulated_args.size() < 3) {
        return;
    }
    user_password = accumulated_args.at(0);
    owner_password = accumulated_args.at(1);
    auto len_str = accumulated_args.at(2);
    accumulated_args.clear();
    argEncBits(len_str);
}

void
ArgParser::argEncUserPassword(std::string const& arg)
{
    if (!accumulated_args.empty()) {
        usage("positional and dashed encryption arguments may not be mixed");
    }
    used_enc_password_args = true;
    user_password = arg;
}

void
ArgParser::argEncOwnerPassword(std::string const& arg)
{
    if (!accumulated_args.empty()) {
        usage("positional and dashed encryption arguments may not be mixed");
    }
    used_enc_password_args = true;
    owner_password = arg;
}

void
ArgParser::argEncBits(std::string const& arg)
{
    if (!accumulated_args.empty()) {
        usage("positional and dashed encryption arguments may not be mixed");
    }
    int keylen = 0;
    if (arg == "40") {
        keylen = 40;
        ap.selectOptionTable(O_40_BIT_ENCRYPTION);
    } else if (arg == "128") {
        keylen = 128;
        ap.selectOptionTable(O_128_BIT_ENCRYPTION);
    } else if (arg == "256") {
        keylen = 256;
        ap.selectOptionTable(O_256_BIT_ENCRYPTION);
    } else {
        usage("encryption key length must be 40, 128, or 256");
    }
    c_enc = c_main->encrypt(keylen, user_password, owner_password);
}

void
ArgParser::argPages()
{
    accumulated_args.clear();
    c_pages = c_main->pages();
    ap.selectOptionTable(O_PAGES);
}

void
ArgParser::argPagesPositional(std::string const& arg)
{
    if (!called_pages_file) {
        c_pages->file(arg);
        called_pages_file = true;
        return;
    }
    if (called_pages_range) {
        c_pages->file(arg);
        called_pages_range = false;
        return;
    }
    // This could be a range or a file. Try parsing.
    try {
        QUtil::parse_numrange(arg.c_str(), 0);
        c_pages->range(arg);
        called_pages_range = true;
    } catch (std::runtime_error& e1) {
        // The range is invalid.  Let's see if it's a file.
        if (arg == ".") {
            // "." means the input file.
            QTC::TC("qpdf", "QPDFJob pages range omitted with .");
        } else if (QUtil::file_can_be_opened(arg.c_str())) {
            QTC::TC("qpdf", "QPDFJob pages range omitted in middle");
            // Yup, it's a file.
        } else {
            // Give the range error
            usage(e1.what());
        }
        c_pages->file(arg);
        called_pages_range = false;
    }
}

void
ArgParser::argEndPages()
{
    c_pages->endPages();
    c_pages = nullptr;
}

void
ArgParser::argUnderlay()
{
    c_uo = c_main->underlay();
    ap.selectOptionTable(O_UNDERLAY_OVERLAY);
}

void
ArgParser::argOverlay()
{
    c_uo = c_main->overlay();
    ap.selectOptionTable(O_UNDERLAY_OVERLAY);
}

void
ArgParser::argAddAttachment()
{
    c_att = c_main->addAttachment();
    ap.selectOptionTable(O_ATTACHMENT);
}

void
ArgParser::argCopyAttachmentsFrom()
{
    c_copy_att = c_main->copyAttachmentsFrom();
    ap.selectOptionTable(O_COPY_ATTACHMENT);
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
ArgParser::argSetPageLabels()
{
    ap.selectOptionTable(O_SET_PAGE_LABELS);
    accumulated_args.clear();
}

void
ArgParser::argPageLabelsPositional(std::string const& arg)
{
    accumulated_args.push_back(arg);
}

void
ArgParser::argEndSetPageLabels()
{
    c_main->setPageLabels(accumulated_args);
    accumulated_args.clear();
}

void
ArgParser::argJobJsonHelp()
{
    *QPDFLogger::defaultLogger()->getInfo()
        << QPDFJob::job_json_schema(QPDFJob::LATEST_JOB_JSON) << "\n";
}

void
ArgParser::usage(std::string const& message)
{
    ap.usage(message);
}

void
ArgParser::parseOptions()
{
    try {
        ap.parseArgs();
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
