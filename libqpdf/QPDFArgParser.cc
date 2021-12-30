#include <qpdf/QPDFArgParser.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/QIntC.hh>
#include <qpdf/QTC.hh>
#include <iostream>
#include <cstring>
#include <cstdlib>

QPDFArgParser::Usage::Usage(std::string const& msg) :
    std::runtime_error(msg)
{
}

QPDFArgParser::Members::Members(
    int argc, char* argv[], char const* progname_env) :

    argc(argc),
    argv(argv),
    whoami(QUtil::getWhoami(argv[0])),
    progname_env(progname_env),
    cur_arg(0),
    bash_completion(false),
    zsh_completion(false),
    option_table(nullptr),
    final_check_handler(nullptr)
{
}

QPDFArgParser::QPDFArgParser(int argc, char* argv[], char const* progname_env) :
    m(new Members(argc, argv, progname_env))
{
    selectHelpOptionTable();
    addBare("completion-bash",
            std::bind(std::mem_fn(&QPDFArgParser::argCompletionBash), this));
    addBare("completion-zsh",
            std::bind(std::mem_fn(&QPDFArgParser::argCompletionZsh), this));
    selectMainOptionTable();
}

void
QPDFArgParser::selectMainOptionTable()
{
    this->m->option_table = &this->m->main_option_table;
    this->m->option_table_name = "main";
}

void
QPDFArgParser::selectHelpOptionTable()
{
    this->m->option_table = &this->m->help_option_table;
    this->m->option_table_name = "help";
}

void
QPDFArgParser::selectOptionTable(std::string const& name)
{
    auto t = this->m->option_tables.find(name);
    if (t == this->m->option_tables.end())
    {
        QTC::TC("libtests", "QPDFArgParser select unregistered table");
        throw std::logic_error(
            "QPDFArgParser: selecting unregistered option table " + name);
    }
    this->m->option_table = &(t->second);
    this->m->option_table_name = name;
}

void
QPDFArgParser::registerOptionTable(
    std::string const& name,
    bare_arg_handler_t end_handler)
{
    if (0 != this->m->option_tables.count(name))
    {
        QTC::TC("libtests", "QPDFArgParser register registered table");
        throw std::logic_error(
            "QPDFArgParser: registering already registered option table "
            + name);
    }
    this->m->option_tables[name];
    selectOptionTable(name);
    addBare("--", end_handler);
}

QPDFArgParser::OptionEntry&
QPDFArgParser::registerArg(std::string const& arg)
{
    if (0 != this->m->option_table->count(arg))
    {
        QTC::TC("libtests", "QPDFArgParser duplicate handler");
        throw std::logic_error(
            "QPDFArgParser: adding a duplicate handler for option " +
            arg + " in " + this->m->option_table_name +
            " option table");
    }
    return ((*this->m->option_table)[arg]);
}

void
QPDFArgParser::addPositional(param_arg_handler_t handler)
{
    OptionEntry& oe = registerArg("");
    oe.param_arg_handler = handler;
}

void
QPDFArgParser::addBare(
    std::string const& arg, bare_arg_handler_t handler)
{
    OptionEntry& oe = registerArg(arg);
    oe.parameter_needed = false;
    oe.bare_arg_handler = handler;
}

void
QPDFArgParser::addRequiredParameter(
    std::string const& arg,
    param_arg_handler_t handler,
    char const* parameter_name)
{
    OptionEntry& oe = registerArg(arg);
    oe.parameter_needed = true;
    oe.parameter_name = parameter_name;
    oe.param_arg_handler = handler;
}

void
QPDFArgParser::addOptionalParameter(
    std::string const& arg, param_arg_handler_t handler)
{
    OptionEntry& oe = registerArg(arg);
    oe.parameter_needed = false;
    oe.param_arg_handler = handler;
}

void
QPDFArgParser::addRequiredChoices(
    std::string const& arg,
    param_arg_handler_t handler,
    char const** choices)
{
    OptionEntry& oe = registerArg(arg);
    oe.parameter_needed = true;
    oe.param_arg_handler = handler;
    for (char const** i = choices; *i; ++i)
    {
        oe.choices.insert(*i);
    }
}

void
QPDFArgParser::addFinalCheck(bare_arg_handler_t handler)
{
    this->m->final_check_handler = handler;
}

bool
QPDFArgParser::isCompleting() const
{
    return this->m->bash_completion;
}

int
QPDFArgParser::argsLeft() const
{
    return this->m->argc - this->m->cur_arg - 1;
}

void
QPDFArgParser::insertCompletion(std::string const& arg)
{
    this->m->completions.insert(arg);
}

void
QPDFArgParser::completionCommon(bool zsh)
{
    std::string progname = this->m->argv[0];
    std::string executable;
    std::string appdir;
    std::string appimage;
    if (QUtil::get_env(this->m->progname_env.c_str(), &executable))
    {
        progname = executable;
    }
    else if (QUtil::get_env("APPDIR", &appdir) &&
             QUtil::get_env("APPIMAGE", &appimage))
    {
        // Detect if we're in an AppImage and adjust
        if ((appdir.length() < strlen(this->m->argv[0])) &&
            (strncmp(appdir.c_str(), this->m->argv[0], appdir.length()) == 0))
        {
            progname = appimage;
        }
    }
    if (zsh)
    {
        std::cout << "autoload -U +X bashcompinit && bashcompinit && ";
    }
    std::cout << "complete -o bashdefault -o default";
    if (! zsh)
    {
        std::cout << " -o nospace";
    }
    std::cout << " -C " << progname << " " << this->m->whoami << std::endl;
    // Put output before error so calling from zsh works properly
    std::string path = progname;
    size_t slash = path.find('/');
    if ((slash != 0) && (slash != std::string::npos))
    {
        std::cerr << "WARNING: " << this->m->whoami << " completion enabled"
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
QPDFArgParser::handleArgFileArguments()
{
    // Support reading arguments from files. Create a new argv. Ensure
    // that argv itself as well as all its contents are automatically
    // deleted by using PointerHolder objects to back the pointers in
    // argv.
    this->m->new_argv.push_back(
        PointerHolder<char>(true, QUtil::copy_string(this->m->argv[0])));
    for (int i = 1; i < this->m->argc; ++i)
    {
        char* argfile = 0;
        if ((strlen(this->m->argv[i]) > 1) && (this->m->argv[i][0] == '@'))
        {
            argfile = 1 + this->m->argv[i];
            if (strcmp(argfile, "-") != 0)
            {
                if (! QUtil::file_can_be_opened(argfile))
                {
                    // The file's not there; treating as regular option
                    argfile = nullptr;
                }
            }
        }
        if (argfile)
        {
            readArgsFromFile(1 + this->m->argv[i]);
        }
        else
        {
            this->m->new_argv.push_back(
                PointerHolder<char>(
                    true, QUtil::copy_string(this->m->argv[i])));
        }
    }
    this->m->argv_ph =
        PointerHolder<char*>(true, new char*[1 + this->m->new_argv.size()]);
    this->m->argv = this->m->argv_ph.getPointer();
    for (size_t i = 0; i < this->m->new_argv.size(); ++i)
    {
        this->m->argv[i] = this->m->new_argv.at(i).getPointer();
    }
    this->m->argc = QIntC::to_int(this->m->new_argv.size());
    this->m->argv[this->m->argc] = 0;
}

void
QPDFArgParser::handleBashArguments()
{
    // Do a minimal job of parsing bash_line into arguments. This
    // doesn't do everything the shell does (e.g. $(...), variable
    // expansion, arithmetic, globs, etc.), but it should be good
    // enough for purposes of handling completion. As we build up the
    // new argv, we can't use this->m->new_argv because this code has to
    // interoperate with @file arguments, so memory for both ways of
    // fabricating argv has to be protected.

    bool last_was_backslash = false;
    enum { st_top, st_squote, st_dquote } state = st_top;
    std::string arg;
    for (std::string::iterator iter = this->m->bash_line.begin();
         iter != this->m->bash_line.end(); ++iter)
    {
        char ch = (*iter);
        if (last_was_backslash)
        {
            arg.append(1, ch);
            last_was_backslash = false;
        }
        else if (ch == '\\')
        {
            last_was_backslash = true;
        }
        else
        {
            bool append = false;
            switch (state)
            {
              case st_top:
                if (QUtil::is_space(ch))
                {
                    if (! arg.empty())
                    {
                        this->m->bash_argv.push_back(
                            PointerHolder<char>(
                                true, QUtil::copy_string(arg.c_str())));
                        arg.clear();
                    }
                }
                else if (ch == '"')
                {
                    state = st_dquote;
                }
                else if (ch == '\'')
                {
                    state = st_squote;
                }
                else
                {
                    append = true;
                }
                break;

              case st_squote:
                if (ch == '\'')
                {
                    state = st_top;
                }
                else
                {
                    append = true;
                }
                break;

              case st_dquote:
                if (ch == '"')
                {
                    state = st_top;
                }
                else
                {
                    append = true;
                }
                break;
            }
            if (append)
            {
                arg.append(1, ch);
            }
        }
    }
    if (this->m->bash_argv.empty())
    {
        // This can't happen if properly invoked by bash, but ensure
        // we have a valid argv[0] regardless.
        this->m->bash_argv.push_back(
            PointerHolder<char>(
                true, QUtil::copy_string(this->m->argv[0])));
    }
    // Explicitly discard any non-space-terminated word. The "current
    // word" is handled specially.
    this->m->bash_argv_ph =
        PointerHolder<char*>(true, new char*[1 + this->m->bash_argv.size()]);
    this->m->argv = this->m->bash_argv_ph.getPointer();
    for (size_t i = 0; i < this->m->bash_argv.size(); ++i)
    {
        this->m->argv[i] = this->m->bash_argv.at(i).getPointer();
    }
    this->m->argc = QIntC::to_int(this->m->bash_argv.size());
    this->m->argv[this->m->argc] = 0;
}

void
QPDFArgParser::usage(std::string const& message)
{
    if (this->m->bash_completion)
    {
        // This will cause bash to fall back to regular file completion.
        exit(0);
    }
    throw Usage(message);
}

void
QPDFArgParser::readArgsFromFile(char const* filename)
{
    std::list<std::string> lines;
    if (strcmp(filename, "-") == 0)
    {
        QTC::TC("libtests", "QPDFArgParser read args from stdin");
        lines = QUtil::read_lines_from_file(std::cin);
    }
    else
    {
        QTC::TC("libtests", "QPDFArgParser read args from file");
        lines = QUtil::read_lines_from_file(filename);
    }
    for (std::list<std::string>::iterator iter = lines.begin();
         iter != lines.end(); ++iter)
    {
        this->m->new_argv.push_back(
            PointerHolder<char>(true, QUtil::copy_string((*iter).c_str())));
    }
}

void
QPDFArgParser::checkCompletion()
{
    // See if we're being invoked from bash completion.
    std::string bash_point_env;
    // On Windows with mingw, there have been times when there appears
    // to be no way to distinguish between an empty environment
    // variable and an unset variable. There are also conditions under
    // which bash doesn't set COMP_LINE. Therefore, enter this logic
    // if either COMP_LINE or COMP_POINT are set. They will both be
    // set together under ordinary circumstances.
    bool got_line = QUtil::get_env("COMP_LINE", &this->m->bash_line);
    bool got_point = QUtil::get_env("COMP_POINT", &bash_point_env);
    if (got_line || got_point)
    {
        size_t p = QUtil::string_to_uint(bash_point_env.c_str());
        if (p < this->m->bash_line.length())
        {
            // Truncate the line. We ignore everything at or after the
            // cursor for completion purposes.
            this->m->bash_line = this->m->bash_line.substr(0, p);
        }
        if (p > this->m->bash_line.length())
        {
            p = this->m->bash_line.length();
        }
        // Set bash_cur and bash_prev based on bash_line rather than
        // relying on argv. This enables us to use bashcompinit to get
        // completion in zsh too since bashcompinit sets COMP_LINE and
        // COMP_POINT but doesn't invoke the command with options like
        // bash does.

        // p is equal to length of the string. Walk backwards looking
        // for the first separator. bash_cur is everything after the
        // last separator, possibly empty.
        char sep(0);
        while (p > 0)
        {
            --p;
            char ch = this->m->bash_line.at(p);
            if ((ch == ' ') || (ch == '=') || (ch == ':'))
            {
                sep = ch;
                break;
            }
        }
        if (1+p <= this->m->bash_line.length())
        {
            this->m->bash_cur = this->m->bash_line.substr(
                1+p, std::string::npos);
        }
        if ((sep == ':') || (sep == '='))
        {
            // Bash sets prev to the non-space separator if any.
            // Actually, if there are multiple separators in a row,
            // they are all included in prev, but that detail is not
            // important to us and not worth coding.
            this->m->bash_prev = this->m->bash_line.substr(p, 1);
        }
        else
        {
            // Go back to the last separator and set prev based on
            // that.
            size_t p1 = p;
            while (p1 > 0)
            {
                --p1;
                char ch = this->m->bash_line.at(p1);
                if ((ch == ' ') || (ch == ':') || (ch == '='))
                {
                    this->m->bash_prev =
                        this->m->bash_line.substr(p1 + 1, p - p1 - 1);
                    break;
                }
            }
        }
        if (this->m->bash_prev.empty())
        {
            this->m->bash_prev = this->m->bash_line.substr(0, p);
        }
        if (this->m->argc == 1)
        {
            // This is probably zsh using bashcompinit. There are a
            // few differences in the expected output.
            this->m->zsh_completion = true;
        }
        handleBashArguments();
        this->m->bash_completion = true;
    }
}

void
QPDFArgParser::parseArgs()
{
    selectMainOptionTable();
    checkCompletion();
    handleArgFileArguments();
    for (this->m->cur_arg = 1;
         this->m->cur_arg < this->m->argc;
         ++this->m->cur_arg)
    {
        bool help_option = false;
        bool end_option = false;
        auto oep = this->m->option_table->end();
        char* arg = this->m->argv[this->m->cur_arg];
        char* parameter = nullptr;
        std::string o_arg(arg);
        std::string arg_s(arg);
        if ((strcmp(arg, "--") == 0) &&
            (this->m->option_table != &this->m->main_option_table))
        {
            // Special case for -- option, which is used to break out
            // of subparsers.
            oep = this->m->option_table->find("--");
            end_option = true;
            if (oep == this->m->option_table->end())
            {
                // This is registered automatically, so this can't happen.
                throw std::logic_error("ArgParser: -- handler not registered");
            }
        }
        else if ((arg[0] == '-') && (strcmp(arg, "-") != 0))
        {
            ++arg;
            if (arg[0] == '-')
            {
                // Be lax about -arg vs --arg
                ++arg;
            }
            else
            {
                QTC::TC("libtests", "QPDFArgParser single dash");
            }
            if (strlen(arg) > 0)
            {
                // Prevent --=something from being treated as an empty
                // arg since the empty string in the option table is
                // for positional arguments.
                parameter = const_cast<char*>(strchr(1 + arg, '='));
            }
            if (parameter)
            {
                *parameter++ = 0;
            }

            arg_s = arg;

            if ((! this->m->bash_completion) &&
                (this->m->argc == 2) && (this->m->cur_arg == 1) &&
                this->m->help_option_table.count(arg_s))
            {
                // Handle help option, which is only valid as the sole
                // option.
                QTC::TC("libtests", "QPDFArgParser help option");
                oep = this->m->help_option_table.find(arg_s);
                help_option = true;
            }

            if (! (help_option || arg_s.empty() || (arg_s.at(0) == '-')))
            {
                oep = this->m->option_table->find(arg_s);
            }
        }
        else
        {
            // The empty string maps to the positional argument
            // handler.
            QTC::TC("libtests", "QPDFArgParser positional");
            oep = this->m->option_table->find("");
            parameter = arg;
        }

        if (oep == this->m->option_table->end())
        {
            QTC::TC("libtests", "QPDFArgParser unrecognized");
            std::string message = "unrecognized argument " + o_arg;
            if (this->m->option_table != &this->m->main_option_table)
            {
                message += " (" + this->m->option_table_name +
                    " options must be terminated with --)";
            }
            usage(message);
        }

        OptionEntry& oe = oep->second;
        if ((oe.parameter_needed && (0 == parameter)) ||
            ((! oe.choices.empty() &&
              ((0 == parameter) ||
               (0 == oe.choices.count(parameter))))))
        {
            std::string message =
                "--" + arg_s + " must be given as --" + arg_s + "=";
            if (! oe.choices.empty())
            {
                QTC::TC("libtests", "QPDFArgParser required choices");
                message += "{";
                for (std::set<std::string>::iterator iter =
                         oe.choices.begin();
                     iter != oe.choices.end(); ++iter)
                {
                    if (iter != oe.choices.begin())
                    {
                        message += ",";
                    }
                    message += *iter;
                }
                message += "}";
            }
            else if (! oe.parameter_name.empty())
            {
                QTC::TC("libtests", "QPDFArgParser required parameter");
                message += oe.parameter_name;
            }
            else
            {
                // should not be possible
                message += "option";
            }
            usage(message);
        }
        if (oe.bare_arg_handler)
        {
            oe.bare_arg_handler();
        }
        else if (oe.param_arg_handler)
        {
            oe.param_arg_handler(parameter);
        }
        if (help_option)
        {
            exit(0);
        }
        if (end_option)
        {
            selectMainOptionTable();
        }
    }
    if (this->m->bash_completion)
    {
        handleCompletion();
    }
    else
    {
        doFinalChecks();
    }
}

void
QPDFArgParser::doFinalChecks()
{
    if (this->m->option_table != &(this->m->main_option_table))
    {
        QTC::TC("libtests", "QPDFArgParser missing --");
        usage("missing -- at end of " + this->m->option_table_name +
              " options");
    }
    if (this->m->final_check_handler != nullptr)
    {
        this->m->final_check_handler();
    }
}

void
QPDFArgParser::addChoicesToCompletions(std::string const& option,
                                       std::string const& extra_prefix)
{
    if (this->m->option_table->count(option) != 0)
    {
        OptionEntry& oe = (*this->m->option_table)[option];
        for (std::set<std::string>::iterator iter = oe.choices.begin();
             iter != oe.choices.end(); ++iter)
        {
            QTC::TC("libtests", "QPDFArgParser complete choices");
            this->m->completions.insert(extra_prefix + *iter);
        }
    }
}

void
QPDFArgParser::addOptionsToCompletions()
{
    for (std::map<std::string, OptionEntry>::iterator iter =
             this->m->option_table->begin();
         iter != this->m->option_table->end(); ++iter)
    {
        std::string const& arg = (*iter).first;
        if (arg == "--")
        {
            continue;
        }
        OptionEntry& oe = (*iter).second;
        std::string base = "--" + arg;
        if (oe.param_arg_handler)
        {
            if (this->m->zsh_completion)
            {
                // zsh doesn't treat = as a word separator, so add all
                // the options so we don't get a space after the =.
                addChoicesToCompletions(arg, base + "=");
            }
            this->m->completions.insert(base + "=");
        }
        if (! oe.parameter_needed)
        {
            this->m->completions.insert(base);
        }
    }
}

void
QPDFArgParser::handleCompletion()
{
    std::string extra_prefix;
    if (this->m->completions.empty())
    {
        // Detect --option=... Bash treats the = as a word separator.
        std::string choice_option;
        if (this->m->bash_cur.empty() && (this->m->bash_prev.length() > 2) &&
            (this->m->bash_prev.at(0) == '-') &&
            (this->m->bash_prev.at(1) == '-') &&
            (this->m->bash_line.at(this->m->bash_line.length() - 1) == '='))
        {
            choice_option = this->m->bash_prev.substr(2, std::string::npos);
        }
        else if ((this->m->bash_prev == "=") &&
                 (this->m->bash_line.length() >
                  (this->m->bash_cur.length() + 1)))
        {
            // We're sitting at --option=x. Find previous option.
            size_t end_mark = this->m->bash_line.length() -
                this->m->bash_cur.length() - 1;
            char before_cur = this->m->bash_line.at(end_mark);
            if (before_cur == '=')
            {
                size_t space = this->m->bash_line.find_last_of(' ', end_mark);
                if (space != std::string::npos)
                {
                    std::string candidate =
                        this->m->bash_line.substr(
                            space + 1, end_mark - space - 1);
                    if ((candidate.length() > 2) &&
                        (candidate.at(0) == '-') &&
                        (candidate.at(1) == '-'))
                    {
                        choice_option =
                            candidate.substr(2, std::string::npos);
                    }
                }
            }
        }
        if (! choice_option.empty())
        {
            if (this->m->zsh_completion)
            {
                // zsh wants --option=choice rather than just choice
                extra_prefix = "--" + choice_option + "=";
            }
            addChoicesToCompletions(choice_option, extra_prefix);
        }
        else if ((! this->m->bash_cur.empty()) &&
                 (this->m->bash_cur.at(0) == '-'))
        {
            addOptionsToCompletions();
            if (this->m->argc == 1)
            {
                // Help options are valid only by themselves.
                for (std::map<std::string, OptionEntry>::iterator iter =
                         this->m->help_option_table.begin();
                     iter != this->m->help_option_table.end(); ++iter)
                {
                    this->m->completions.insert("--" + (*iter).first);
                }
            }
        }
    }
    std::string prefix = extra_prefix + this->m->bash_cur;
    for (std::set<std::string>::iterator iter = this->m->completions.begin();
         iter != this->m->completions.end(); ++iter)
    {
        if (prefix.empty() ||
            ((*iter).substr(0, prefix.length()) == prefix))
        {
            std::cout << *iter << std::endl;
        }
    }
    exit(0);
}
