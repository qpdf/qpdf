#include <qpdf/QPDFArgParser.hh>

#include <qpdf/QIntC.hh>
#include <qpdf/QPDFLogger.hh>
#include <qpdf/QPDFUsage.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/Util.hh>

#include <cstdlib>
#include <cstring>
#include <iostream>

using namespace qpdf;
using namespace std::literals;

QPDFArgParser::Members::Members(int argc, char const* const argv[], char const* progname_env) :

    argc(argc),
    argv(argv),
    progname_env(progname_env),
    cur_arg(0),
    bash_completion(false),
    zsh_completion(false),
    option_table(nullptr),
    final_check_handler(nullptr)
{
    auto tmp = QUtil::make_unique_cstr(argv[0]);
    whoami = QUtil::getWhoami(tmp.get());
}

QPDFArgParser::QPDFArgParser(int argc, char const* const argv[], char const* progname_env) :
    m(new Members(argc, argv, progname_env))
{
    selectHelpOptionTable();
    char const* help_choices[] = {"all", nullptr};
    // More help choices are added dynamically.
    addChoices("help", bindParam(&QPDFArgParser::argHelp, this), false, help_choices);
    addInvalidChoiceHandler("help", bindParam(&QPDFArgParser::invalidHelpArg, this));
    addBare("completion-bash", bindBare(&QPDFArgParser::argCompletionBash, this));
    addBare("completion-zsh", bindBare(&QPDFArgParser::argCompletionZsh, this));
    selectMainOptionTable();
}

void
QPDFArgParser::selectMainOptionTable()
{
    m->option_table = &m->main_option_table;
    m->option_table_name = "main";
}

void
QPDFArgParser::selectHelpOptionTable()
{
    m->option_table = &m->help_option_table;
    m->option_table_name = "help";
}

void
QPDFArgParser::selectOptionTable(std::string const& name)
{
    auto t = m->option_tables.find(name);
    if (t == m->option_tables.end()) {
        QTC::TC("libtests", "QPDFArgParser select unregistered table");
        throw std::logic_error("QPDFArgParser: selecting unregistered option table " + name);
    }
    m->option_table = &(t->second);
    m->option_table_name = name;
}

void
QPDFArgParser::registerOptionTable(std::string const& name, bare_arg_handler_t end_handler)
{
    if (m->option_tables.contains(name)) {
        QTC::TC("libtests", "QPDFArgParser register registered table");
        throw std::logic_error(
            "QPDFArgParser: registering already registered option table " + name);
    }
    m->option_tables[name];
    selectOptionTable(name);
    addBare("--", end_handler);
}

QPDFArgParser::OptionEntry&
QPDFArgParser::registerArg(std::string const& arg)
{
    if (m->option_table->contains(arg)) {
        QTC::TC("libtests", "QPDFArgParser duplicate handler");
        throw std::logic_error(
            "QPDFArgParser: adding a duplicate handler for option " + arg + " in " +
            m->option_table_name + " option table");
    }
    return ((*m->option_table)[arg]);
}

void
QPDFArgParser::addPositional(param_arg_handler_t handler)
{
    OptionEntry& oe = registerArg("");
    oe.param_arg_handler = handler;
}

void
QPDFArgParser::addBare(std::string const& arg, bare_arg_handler_t handler)
{
    OptionEntry& oe = registerArg(arg);
    oe.parameter_needed = false;
    oe.bare_arg_handler = handler;
}

void
QPDFArgParser::addRequiredParameter(
    std::string const& arg, param_arg_handler_t handler, char const* parameter_name)
{
    OptionEntry& oe = registerArg(arg);
    oe.parameter_needed = true;
    oe.parameter_name = parameter_name;
    oe.param_arg_handler = handler;
}

void
QPDFArgParser::addOptionalParameter(std::string const& arg, param_arg_handler_t handler)
{
    OptionEntry& oe = registerArg(arg);
    oe.parameter_needed = false;
    oe.param_arg_handler = handler;
}

void
QPDFArgParser::addChoices(
    std::string const& arg, param_arg_handler_t handler, bool required, char const** choices)
{
    OptionEntry& oe = registerArg(arg);
    oe.parameter_needed = required;
    oe.param_arg_handler = handler;
    for (char const** i = choices; *i; ++i) {
        oe.choices.insert(*i);
    }
}

void
QPDFArgParser::addInvalidChoiceHandler(std::string const& arg, param_arg_handler_t handler)
{
    auto i = m->option_table->find(arg);
    if (i == m->option_table->end()) {
        QTC::TC("libtests", "QPDFArgParser invalid choice handler to unknown");
        throw std::logic_error(
            "QPDFArgParser: attempt to add invalid choice handler to unknown argument");
    }
    auto& oe = i->second;
    oe.invalid_choice_handler = handler;
}

void
QPDFArgParser::addFinalCheck(bare_arg_handler_t handler)
{
    m->final_check_handler = handler;
}

bool
QPDFArgParser::isCompleting() const
{
    return m->bash_completion;
}

int
QPDFArgParser::argsLeft() const
{
    return m->argc - m->cur_arg - 1;
}

void
QPDFArgParser::insertCompletion(std::string const& arg)
{
    m->completions.insert(arg);
}

void
QPDFArgParser::completionCommon(bool zsh)
{
    std::string progname = m->argv[0];
    std::string executable;
    std::string appdir;
    std::string appimage;
    if (QUtil::get_env(m->progname_env.c_str(), &executable)) {
        progname = executable;
    } else if (QUtil::get_env("APPDIR", &appdir) && QUtil::get_env("APPIMAGE", &appimage)) {
        // Detect if we're in an AppImage and adjust
        if ((appdir.length() < strlen(m->argv[0])) &&
            (strncmp(appdir.c_str(), m->argv[0], appdir.length()) == 0)) {
            progname = appimage;
        }
    }
    if (zsh) {
        std::cout << "autoload -U +X bashcompinit && bashcompinit && ";
    }
    std::cout << "complete -o bashdefault -o default";
    if (!zsh) {
        std::cout << " -o nospace";
    }
    std::cout << " -C \"" << progname << "\" " << m->whoami << std::endl;
    // Put output before error so calling from zsh works properly
    std::string path = progname;
    size_t slash = path.find('/');
    if ((slash != 0) && (slash != std::string::npos)) {
        std::cerr << "WARNING: " << m->whoami << " completion enabled"
                  << " using relative path to executable" << std::endl;
    }
}

void
QPDFArgParser::argCompletionBash()
{
    completionCommon(false);
}

void
QPDFArgParser::argCompletionZsh()
{
    completionCommon(true);
}

void
QPDFArgParser::argHelp(std::string const& p)
{
    QPDFLogger::defaultLogger()->info(getHelp(p));
    exit(0);
}

void
QPDFArgParser::invalidHelpArg(std::string const& p)
{
    usage(std::string("unknown help option") + (p.empty() ? "" : (" " + p)));
}

void
QPDFArgParser::handleArgFileArguments()
{
    // Support reading arguments from files. Create a new argv. Ensure that argv itself as well as
    // all its contents are automatically deleted by using shared pointers back to the pointers in
    // argv.
    m->new_argv.push_back(QUtil::make_shared_cstr(m->argv[0]));
    for (int i = 1; i < m->argc; ++i) {
        char const* argfile = nullptr;
        if ((strlen(m->argv[i]) > 1) && (m->argv[i][0] == '@')) {
            argfile = 1 + m->argv[i];
            if (strcmp(argfile, "-") != 0) {
                if (!QUtil::file_can_be_opened(argfile)) {
                    // The file's not there; treating as regular option
                    argfile = nullptr;
                }
            }
        }
        if (argfile) {
            readArgsFromFile(1 + m->argv[i]);
        } else {
            m->new_argv.push_back(QUtil::make_shared_cstr(m->argv[i]));
        }
    }
    m->argv_ph = QUtil::make_shared_array<char const*>(1 + m->new_argv.size());
    for (size_t i = 0; i < m->new_argv.size(); ++i) {
        m->argv_ph.get()[i] = m->new_argv.at(i).get();
    }
    m->argc = QIntC::to_int(m->new_argv.size());
    m->argv_ph.get()[m->argc] = nullptr;
    m->argv = m->argv_ph.get();
}

void
QPDFArgParser::handleBashArguments()
{
    // Do a minimal job of parsing bash_line into arguments. This doesn't do everything the shell
    // does (e.g. $(...), variable expansion, arithmetic, globs, etc.), but it should be good enough
    // for purposes of handling completion. As we build up the new argv, we can't use m->new_argv
    // because this code has to interoperate with @file arguments, so memory for both ways of
    // fabricating argv has to be protected.

    bool last_was_backslash = false;
    enum { st_top, st_squote, st_dquote } state = st_top;
    std::string arg;
    for (char ch: m->bash_line) {
        if (last_was_backslash) {
            arg.append(1, ch);
            last_was_backslash = false;
        } else if (ch == '\\') {
            last_was_backslash = true;
        } else {
            bool append = false;
            switch (state) {
            case st_top:
                if (util::is_space(ch)) {
                    if (!arg.empty()) {
                        m->bash_argv.push_back(QUtil::make_shared_cstr(arg));
                        arg.clear();
                    }
                } else if (ch == '"') {
                    state = st_dquote;
                } else if (ch == '\'') {
                    state = st_squote;
                } else {
                    append = true;
                }
                break;

            case st_squote:
                if (ch == '\'') {
                    state = st_top;
                } else {
                    append = true;
                }
                break;

            case st_dquote:
                if (ch == '"') {
                    state = st_top;
                } else {
                    append = true;
                }
                break;
            }
            if (append) {
                arg.append(1, ch);
            }
        }
    }
    if (m->bash_argv.empty()) {
        // This can't happen if properly invoked by bash, but ensure we have a valid argv[0]
        // regardless.
        m->bash_argv.push_back(QUtil::make_shared_cstr(m->argv[0]));
    }
    // Explicitly discard any non-space-terminated word. The "current word" is handled specially.
    m->bash_argv_ph = QUtil::make_shared_array<char const*>(1 + m->bash_argv.size());
    for (size_t i = 0; i < m->bash_argv.size(); ++i) {
        m->bash_argv_ph.get()[i] = m->bash_argv.at(i).get();
    }
    m->argc = QIntC::to_int(m->bash_argv.size());
    m->bash_argv_ph.get()[m->argc] = nullptr;
    m->argv = m->bash_argv_ph.get();
}

void
QPDFArgParser::usage(std::string const& message)
{
    if (m->bash_completion) {
        // This will cause bash to fall back to regular file completion.
        exit(0);
    }
    throw QPDFUsage(message);
}

void
QPDFArgParser::readArgsFromFile(std::string const& filename)
{
    std::list<std::string> lines;
    if (filename == "-") {
        QTC::TC("libtests", "QPDFArgParser read args from stdin");
        lines = QUtil::read_lines_from_file(std::cin);
    } else {
        QTC::TC("libtests", "QPDFArgParser read args from file");
        lines = QUtil::read_lines_from_file(filename.c_str());
    }
    for (auto const& line: lines) {
        m->new_argv.push_back(QUtil::make_shared_cstr(line));
    }
}

void
QPDFArgParser::checkCompletion()
{
    // See if we're being invoked from bash completion.
    std::string bash_point_env;
    // On Windows with mingw, there have been times when there appears to be no way to distinguish
    // between an empty environment variable and an unset variable. There are also conditions under
    // which bash doesn't set COMP_LINE. Therefore, enter this logic if either COMP_LINE or
    // COMP_POINT are set. They will both be set together under ordinary circumstances.
    bool got_line = QUtil::get_env("COMP_LINE", &m->bash_line);
    bool got_point = QUtil::get_env("COMP_POINT", &bash_point_env);
    if (got_line || got_point) {
        size_t p = QUtil::string_to_uint(bash_point_env.c_str());
        if (p < m->bash_line.length()) {
            // Truncate the line. We ignore everything at or after the
            // cursor for completion purposes.
            m->bash_line = m->bash_line.substr(0, p);
        }
        if (p > m->bash_line.length()) {
            p = m->bash_line.length();
        }
        // Set bash_cur and bash_prev based on bash_line rather than relying on argv. This enables
        // us to use bashcompinit to get completion in zsh too since bashcompinit sets COMP_LINE and
        // COMP_POINT but doesn't invoke the command with options like bash does.

        // p is equal to length of the string. Walk backwards looking for the first separator.
        // bash_cur is everything after the last separator, possibly empty.
        char sep(0);
        while (p > 0) {
            --p;
            char ch = m->bash_line.at(p);
            if ((ch == ' ') || (ch == '=') || (ch == ':')) {
                sep = ch;
                break;
            }
        }
        if (1 + p <= m->bash_line.length()) {
            m->bash_cur = m->bash_line.substr(1 + p, std::string::npos);
        }
        if ((sep == ':') || (sep == '=')) {
            // Bash sets prev to the non-space separator if any. Actually, if there are multiple
            // separators in a row, they are all included in prev, but that detail is not important
            // to us and not worth coding.
            m->bash_prev = m->bash_line.substr(p, 1);
        } else {
            // Go back to the last separator and set prev based on
            // that.
            size_t p1 = p;
            while (p1 > 0) {
                --p1;
                char ch = m->bash_line.at(p1);
                if ((ch == ' ') || (ch == ':') || (ch == '=')) {
                    m->bash_prev = m->bash_line.substr(p1 + 1, p - p1 - 1);
                    break;
                }
            }
        }
        if (m->bash_prev.empty()) {
            m->bash_prev = m->bash_line.substr(0, p);
        }
        if (m->argc == 1) {
            // This is probably zsh using bashcompinit. There are a few differences in the expected
            // output.
            m->zsh_completion = true;
        }
        handleBashArguments();
        m->bash_completion = true;
    }
}

void
QPDFArgParser::parseArgs()
{
    selectMainOptionTable();
    checkCompletion();
    handleArgFileArguments();
    for (m->cur_arg = 1; m->cur_arg < m->argc; ++m->cur_arg) {
        bool help_option = false;
        bool end_option = false;
        auto oep = m->option_table->end();
        char const* arg = m->argv[m->cur_arg];
        std::string parameter;
        bool have_parameter = false;
        std::string o_arg(arg);
        std::string arg_s(arg);
        if (strcmp(arg, "--") == 0) {
            // Special case for -- option, which is used to break out of subparsers.
            oep = m->option_table->find("--");
            end_option = true;
            if (oep == m->option_table->end()) {
                // This is registered automatically, so this can't happen.
                throw std::logic_error("QPDFArgParser: -- handler not registered");
            }
        } else if ((arg[0] == '-') && (strcmp(arg, "-") != 0)) {
            ++arg;
            if (arg[0] == '-') {
                // Be lax about -arg vs --arg
                ++arg;
            } else {
                QTC::TC("libtests", "QPDFArgParser single dash");
            }

            // Prevent --=something from being treated as an empty arg by searching for = from after
            // the first character. We do this since the empty string in the option table is for
            // positional arguments. Besides, it doesn't make sense to have an empty option.
            arg_s = arg;
            size_t equal_pos = std::string::npos;
            if (!arg_s.empty()) {
                equal_pos = arg_s.find('=', 1);
            }
            if (equal_pos != std::string::npos) {
                have_parameter = true;
                parameter = arg_s.substr(equal_pos + 1);
                arg_s = arg_s.substr(0, equal_pos);
            }

            if ((!m->bash_completion) && (m->argc == 2) && (m->cur_arg == 1) &&
                m->help_option_table.contains(arg_s)) {
                // Handle help option, which is only valid as the sole option.
                QTC::TC("libtests", "QPDFArgParser help option");
                oep = m->help_option_table.find(arg_s);
                help_option = true;
            }

            if (!(help_option || arg_s.empty() || (arg_s.at(0) == '-'))) {
                oep = m->option_table->find(arg_s);
            }
        } else {
            // The empty string maps to the positional argument handler.
            QTC::TC("libtests", "QPDFArgParser positional");
            oep = m->option_table->find("");
            parameter = arg;
        }

        if (oep == m->option_table->end()) {
            QTC::TC("libtests", "QPDFArgParser unrecognized");
            std::string message = "unrecognized argument " + o_arg;
            if (m->option_table != &m->main_option_table) {
                message += " (" + m->option_table_name + " options must be terminated with --)";
            }
            usage(message);
        }

        OptionEntry& oe = oep->second;
        if ((oe.parameter_needed && !have_parameter) ||
            (!oe.choices.empty() && have_parameter && !oe.choices.contains(parameter))) {
            std::string message = "--" + arg_s + " must be given as --" + arg_s + "=";
            if (oe.invalid_choice_handler) {
                oe.invalid_choice_handler(parameter);
                // Method should call usage() or exit. Just in case it doesn't...
                message += "option";
            } else if (!oe.choices.empty()) {
                QTC::TC("libtests", "QPDFArgParser required choices");
                message += "{";
                bool first = true;
                for (auto const& choice: oe.choices) {
                    if (first) {
                        first = false;
                    } else {
                        message += ",";
                    }
                    message += choice;
                }
                message += "}";
            } else if (!oe.parameter_name.empty()) {
                QTC::TC("libtests", "QPDFArgParser required parameter");
                message += oe.parameter_name;
            } else {
                // should not be possible
                message += "option";
            }
            usage(message);
        }

        if (oe.bare_arg_handler) {
            if (have_parameter) {
                usage(
                    "--"s + arg_s + " does not take a parameter, but \"" + parameter +
                    "\" was given");
            }
            oe.bare_arg_handler();
        } else if (oe.param_arg_handler) {
            oe.param_arg_handler(parameter);
        }
        if (help_option) {
            exit(0);
        }
        if (end_option) {
            selectMainOptionTable();
        }
    }
    if (m->bash_completion) {
        handleCompletion();
    } else {
        doFinalChecks();
    }
}

std::string
QPDFArgParser::getProgname()
{
    return m->whoami;
}

void
QPDFArgParser::doFinalChecks()
{
    if (m->option_table != &(m->main_option_table)) {
        QTC::TC("libtests", "QPDFArgParser missing --");
        usage("missing -- at end of " + m->option_table_name + " options");
    }
    if (m->final_check_handler != nullptr) {
        m->final_check_handler();
    }
}

void
QPDFArgParser::addChoicesToCompletions(
    option_table_t& option_table, std::string const& option, std::string const& extra_prefix)
{
    if (option_table.contains(option)) {
        OptionEntry& oe = option_table[option];
        for (auto const& choice: oe.choices) {
            QTC::TC("libtests", "QPDFArgParser complete choices");
            m->completions.insert(extra_prefix + choice);
        }
    }
}

void
QPDFArgParser::addOptionsToCompletions(option_table_t& option_table)
{
    for (auto& iter: option_table) {
        std::string const& arg = iter.first;
        if (arg == "--") {
            continue;
        }
        OptionEntry& oe = iter.second;
        std::string base = "--" + arg;
        if (oe.param_arg_handler) {
            if (m->zsh_completion) {
                // zsh doesn't treat = as a word separator, so add all the options so we don't get a
                // space after the =.
                addChoicesToCompletions(option_table, arg, base + "=");
            }
            m->completions.insert(base + "=");
        }
        if (!oe.parameter_needed) {
            m->completions.insert(base);
        }
    }
}

void
QPDFArgParser::insertCompletions(
    option_table_t& option_table, std::string const& choice_option, std::string const& extra_prefix)
{
    if (!choice_option.empty()) {
        addChoicesToCompletions(option_table, choice_option, extra_prefix);
    } else if ((!m->bash_cur.empty()) && (m->bash_cur.at(0) == '-')) {
        addOptionsToCompletions(option_table);
    }
}

void
QPDFArgParser::handleCompletion()
{
    std::string extra_prefix;
    if (m->completions.empty()) {
        // Detect --option=... Bash treats the = as a word separator.
        std::string choice_option;
        if (m->bash_cur.empty() && (m->bash_prev.length() > 2) && (m->bash_prev.at(0) == '-') &&
            (m->bash_prev.at(1) == '-') && (m->bash_line.at(m->bash_line.length() - 1) == '=')) {
            choice_option = m->bash_prev.substr(2, std::string::npos);
        } else if ((m->bash_prev == "=") && (m->bash_line.length() > (m->bash_cur.length() + 1))) {
            // We're sitting at --option=x. Find previous option.
            size_t end_mark = m->bash_line.length() - m->bash_cur.length() - 1;
            char before_cur = m->bash_line.at(end_mark);
            if (before_cur == '=') {
                size_t space = m->bash_line.find_last_of(' ', end_mark);
                if (space != std::string::npos) {
                    std::string candidate = m->bash_line.substr(space + 1, end_mark - space - 1);
                    if ((candidate.length() > 2) && (candidate.at(0) == '-') &&
                        (candidate.at(1) == '-')) {
                        choice_option = candidate.substr(2, std::string::npos);
                    }
                }
            }
        }
        if (m->zsh_completion && (!choice_option.empty())) {
            // zsh wants --option=choice rather than just choice
            extra_prefix = "--" + choice_option + "=";
        }
        insertCompletions(*m->option_table, choice_option, extra_prefix);
        if (m->argc == 1) {
            // Help options are valid only by themselves.
            insertCompletions(m->help_option_table, choice_option, extra_prefix);
        }
    }
    std::string prefix = extra_prefix + m->bash_cur;
    for (auto const& iter: m->completions) {
        if (prefix.empty() || (iter.substr(0, prefix.length()) == prefix)) {
            std::cout << iter << std::endl;
        }
    }
    exit(0);
}

void
QPDFArgParser::addHelpFooter(std::string const& text)
{
    m->help_footer = "\n" + text;
}

void
QPDFArgParser::addHelpTopic(
    std::string const& topic, std::string const& short_text, std::string const& long_text)
{
    if (topic == "all") {
        QTC::TC("libtests", "QPDFArgParser add reserved help topic");
        throw std::logic_error("QPDFArgParser: can't register reserved help topic " + topic);
    }
    if (topic.empty() || topic.at(0) == '-') {
        QTC::TC("libtests", "QPDFArgParser bad topic for help");
        throw std::logic_error("QPDFArgParser: help topics must not start with -");
    }
    if (m->help_topics.contains(topic)) {
        QTC::TC("libtests", "QPDFArgParser add existing topic");
        throw std::logic_error("QPDFArgParser: topic " + topic + " has already been added");
    }

    m->help_topics[topic] = HelpTopic(short_text, long_text);
    m->help_option_table["help"].choices.insert(topic);
}

void
QPDFArgParser::addOptionHelp(
    std::string const& option_name,
    std::string const& topic,
    std::string const& short_text,
    std::string const& long_text)
{
    if (!((option_name.length() > 2) && (option_name.at(0) == '-') && (option_name.at(1) == '-'))) {
        QTC::TC("libtests", "QPDFArgParser bad option for help");
        throw std::logic_error("QPDFArgParser: options for help must start with --");
    }
    if (m->option_help.contains(option_name)) {
        QTC::TC("libtests", "QPDFArgParser duplicate option help");
        throw std::logic_error("QPDFArgParser: option " + option_name + " already has help");
    }
    auto ht = m->help_topics.find(topic);
    if (ht == m->help_topics.end()) {
        QTC::TC("libtests", "QPDFArgParser add to unknown topic");
        throw std::logic_error(
            "QPDFArgParser: unable to add option " + option_name + " to unknown help topic " +
            topic);
    }
    m->option_help[option_name] = HelpTopic(short_text, long_text);
    ht->second.options.insert(option_name);
    m->help_option_table["help"].choices.insert(option_name);
}

void
QPDFArgParser::getTopHelp(std::ostringstream& msg)
{
    msg << "Run \"" << m->whoami << " --help=topic\" for help on a topic." << std::endl
        << "Run \"" << m->whoami << " --help=--option\" for help on an option." << std::endl
        << "Run \"" << m->whoami << " --help=all\" to see all available help." << std::endl
        << std::endl
        << "Topics:" << std::endl;
    for (auto const& i: m->help_topics) {
        msg << "  " << i.first << ": " << i.second.short_text << std::endl;
    }
}

void
QPDFArgParser::getAllHelp(std::ostringstream& msg)
{
    getTopHelp(msg);
    auto show = [this, &msg](std::map<std::string, HelpTopic>& topics) {
        for (auto const& i: topics) {
            auto const& topic = i.first;
            msg << std::endl
                << "== " << topic << " (" << i.second.short_text << ") ==" << std::endl
                << std::endl;
            getTopicHelp(topic, i.second, msg);
        }
    };
    show(m->help_topics);
    show(m->option_help);
    msg << std::endl << "====" << std::endl;
}

void
QPDFArgParser::getTopicHelp(std::string const& name, HelpTopic const& ht, std::ostringstream& msg)
{
    if (ht.long_text.empty()) {
        msg << ht.short_text << std::endl;
    } else {
        msg << ht.long_text;
    }
    if (!ht.options.empty()) {
        msg << std::endl << "Related options:" << std::endl;
        for (auto const& i: ht.options) {
            msg << "  " << i << ": " << m->option_help[i].short_text << std::endl;
        }
    }
}

std::string
QPDFArgParser::getHelp(std::string const& arg)
{
    std::ostringstream msg;
    if (arg.empty()) {
        getTopHelp(msg);
    } else {
        if (arg == "all") {
            getAllHelp(msg);
        } else if (m->option_help.contains(arg)) {
            getTopicHelp(arg, m->option_help[arg], msg);
        } else if (m->help_topics.contains(arg)) {
            getTopicHelp(arg, m->help_topics[arg], msg);
        } else {
            // should not be possible
            getTopHelp(msg);
        }
    }
    msg << m->help_footer;
    return msg.str();
}
