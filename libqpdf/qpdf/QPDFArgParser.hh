#ifndef QPDFARGPARSER_HH
#define QPDFARGPARSER_HH

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// This is not a general-purpose argument parser. It is tightly crafted to work with qpdf. qpdf's
// command-line syntax is very complex because of its long history, and it doesn't really follow any
// kind of normal standard for arguments, but it's important for backward compatibility to ensure we
// don't break what constitutes a valid command. This class handles the quirks of qpdf's argument
// parsing, bash/zsh completion, and support for @argfile to read arguments from a file. For the
// qpdf CLI, setup of QPDFArgParser is done mostly by automatically-generated code (one-off code for
// qpdf), though the handlers themselves are hand-coded. See generate_auto_job at the top of the
// source tree for details.
class QPDFArgParser
{
  public:
    // progname_env is used to override argv[0] when figuring out the name of the executable for
    // setting up completion. This may be needed if the program is invoked by a wrapper.
    QPDFArgParser(int argc, char const* const argv[], char const* progname_env);

    // Calls exit(0) if a help option is given or if in completion mode. If there are argument
    // parsing errors, QPDFUsage is thrown.
    void parseArgs();

    // Return the program name as the last path element of the program executable.
    std::string getProgname();

    // Methods for registering arguments. QPDFArgParser starts off with the main option table
    // selected. You can add handlers for arguments in the current option table, and you can select
    // which option table is current. The help option table is special and contains arguments that
    // are only valid as the first and only option. Named option tables are for subparsers and
    // always start a series of options that end with `--`.

    typedef std::function<void()> bare_arg_handler_t;
    typedef std::function<void(std::string const&)> param_arg_handler_t;

    void selectMainOptionTable();
    void selectHelpOptionTable();
    void selectOptionTable(std::string const& name);

    // Register a new options table. This also selects the option table.
    void registerOptionTable(std::string const& name, bare_arg_handler_t end_handler);

    // Add handlers for options in the current table

    void addPositional(param_arg_handler_t);
    void addBare(std::string const& arg, bare_arg_handler_t);
    void
    addRequiredParameter(std::string const& arg, param_arg_handler_t, char const* parameter_name);
    void addOptionalParameter(std::string const& arg, param_arg_handler_t);
    void
    addChoices(std::string const& arg, param_arg_handler_t, bool required, char const** choices);

    // The default behavior when an invalid choice is specified with an option that takes choices is
    // to list all the choices. This may not be good if there are too many choices, so you can
    // provide your own handler in this case.
    void addInvalidChoiceHandler(std::string const& arg, param_arg_handler_t);

    // The final check handler is called at the very end of argument
    // parsing.
    void addFinalCheck(bare_arg_handler_t);

    // Help generation methods

    // Help is available on topics and options. Options may be associated with topics. Users can run
    // --help, --help=topic, or --help=--arg to get help. The top-level help tells the user how
    // to run help and lists available topics. Help for a topic prints a short synopsis about the
    // topic and lists any options that may be associated with the topic. Help for an option
    // provides a short synopsis for that option. All help output is appended with a blurb (if
    // supplied) directing the user to the full documentation. Help is not shown for options for
    // which help has not been added. This makes it possible to have undocumented options for
    // testing, backward-compatibility, etc. Also, it could be quite confusing to handle appropriate
    // help for some inner options that may be repeated with different semantics inside different
    // option tables. There is also no checking for whether an option that has help actually exists.
    // In other words, it's up to the caller to ensure that help actually corresponds to the
    // program's actual options. Rather than this being an intentional design decision, it is
    // because this class is specifically for qpdf, qpdf generates its help and has other means to
    // ensure consistency.

    // Note about newlines:
    //
    // short_text should fit easily after the topic/option on the same line and should not end with
    // a newline. Keep it to around 40 to 60 characters.
    //
    // long_text and footer should end with a single newline. They can have embedded newlines. Keep
    // lines to under 80 columns.
    //
    // QPDFArgParser does reformat the text, but it may add blank lines in some situations.
    // Following the above conventions will keep the help looking uniform.

    // If provided, this footer is appended to all help, separated by a blank line.
    void addHelpFooter(std::string const&);

    // Add a help topic along with the text for that topic
    void addHelpTopic(
        std::string const& topic, std::string const& short_text, std::string const& long_text);

    // Add help for an option, and associate it with a topic.
    void addOptionHelp(
        std::string const& option_name,
        std::string const& topic,
        std::string const& short_text,
        std::string const& long_text);

    // Return the help text for a topic or option. Passing a null pointer returns the top-level help
    // information. Passing an unknown value returns a string directing the user to run the
    // top-level --help option.
    std::string getHelp(std::string const& topic_or_option);

    // Convenience methods for adding member functions of a class as handlers.
    template <class T>
    static bare_arg_handler_t
    bindBare(void (T::*f)(), T* o)
    {
        return std::bind(std::mem_fn(f), o);
    }
    template <class T>
    static param_arg_handler_t
    bindParam(void (T::*f)(std::string const&), T* o)
    {
        return std::bind(std::mem_fn(f), o, std::placeholders::_1);
    }

    // When processing arguments, indicate how many arguments remain after the one whose handler is
    // being called.
    int argsLeft() const;

    // Indicate whether we are in completion mode.
    bool isCompleting() const;

    // Insert a completion during argument parsing; useful for customizing completion in the
    // position argument handler. Should only be used in completion mode.
    void insertCompletion(std::string const&);

    // Throw a Usage exception with the given message. In completion mode, this just exits to
    // prevent errors from partial commands or other error messages from messing up completion.
    void usage(std::string const& message);

  private:
    struct OptionEntry
    {
        OptionEntry() :
            parameter_needed(false),
            bare_arg_handler(nullptr),
            param_arg_handler(nullptr),
            invalid_choice_handler(nullptr)
        {
        }
        bool parameter_needed;
        std::string parameter_name;
        std::set<std::string> choices;
        bare_arg_handler_t bare_arg_handler;
        param_arg_handler_t param_arg_handler;
        param_arg_handler_t invalid_choice_handler;
    };
    typedef std::map<std::string, OptionEntry> option_table_t;

    struct HelpTopic
    {
        HelpTopic() = default;
        HelpTopic(std::string const& short_text, std::string const& long_text) :
            short_text(short_text),
            long_text(long_text)
        {
        }

        std::string short_text;
        std::string long_text;
        std::set<std::string> options;
    };

    OptionEntry& registerArg(std::string const& arg);

    void completionCommon(bool zsh);

    void argCompletionBash();
    void argCompletionZsh();
    void argHelp(std::string const&);
    void invalidHelpArg(std::string const&);

    void checkCompletion();
    void handleArgFileArguments();
    void handleBashArguments();
    void readArgsFromFile(std::string const& filename);
    void doFinalChecks();
    void addOptionsToCompletions(option_table_t&);
    void addChoicesToCompletions(option_table_t&, std::string const&, std::string const&);
    void insertCompletions(option_table_t&, std::string const&, std::string const&);
    void handleCompletion();

    void getTopHelp(std::ostringstream&);
    void getAllHelp(std::ostringstream&);
    void getTopicHelp(std::string const& name, HelpTopic const&, std::ostringstream&);

    class Members
    {
        friend class QPDFArgParser;

      public:
        ~Members() = default;

      private:
        Members(int argc, char const* const argv[], char const* progname_env);
        Members(Members const&) = delete;

        int argc;
        char const* const* argv;
        std::string whoami;
        std::string progname_env;
        int cur_arg;
        bool bash_completion;
        bool zsh_completion;
        std::string bash_prev;
        std::string bash_cur;
        std::string bash_line;
        std::set<std::string> completions;
        std::map<std::string, option_table_t> option_tables;
        option_table_t main_option_table;
        option_table_t help_option_table;
        option_table_t* option_table;
        std::string option_table_name;
        bare_arg_handler_t final_check_handler;
        std::vector<std::shared_ptr<char const>> new_argv;
        std::vector<std::shared_ptr<char const>> bash_argv;
        std::shared_ptr<char const*> argv_ph;
        std::shared_ptr<char const*> bash_argv_ph;
        std::map<std::string, HelpTopic> help_topics;
        std::map<std::string, HelpTopic> option_help;
        std::string help_footer;
    };
    std::shared_ptr<Members> m;
};

#endif // QPDFARGPARSER_HH
