// Copyright (c) 2005-2021 Jay Berkenbilt
//
// This file is part of qpdf.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Versions of qpdf prior to version 7 were released under the terms
// of version 2.0 of the Artistic License. At your option, you may
// continue to consider qpdf to be licensed under those terms. Please
// see the manual for additional information.

#ifndef QPDFARGPARSER_HH
#define QPDFARGPARSER_HH

#include <qpdf/DLL.h>
#include <qpdf/PointerHolder.hh>
#include <string>
#include <set>
#include <map>
#include <vector>
#include <functional>
#include <stdexcept>

// This is not a general-purpose argument parser. It is tightly
// crafted to work with qpdf. qpdf's command-line syntax is very
// complex because of its long history, and it doesn't really follow
// any kind of normal standard for arguments, but it's important for
// backward compatibility to ensure we don't break what constitutes a
// valid command. This class handles the quirks of qpdf's argument
// parsing, bash/zsh completion, and support for @argfile to read
// arguments from a file.

// Note about memory: there is code that expects argv to be a char*[],
// meaning that arguments are writable. Several operations, including
// reading arguments from a file or parsing a line for bash
// completion, involve fabricating an argv array. To ensure that the
// memory is valid and is cleaned up properly, we keep various vectors
// of smart character pointers that argv points into. In order for
// those pointers to remain valid, the QPDFArgParser instance must
// remain in scope for the life of any code that may reference
// anything from argv.
class QPDFArgParser
{
  public:
    // Usage exception is thrown if there are any errors parsing
    // arguments
    class QPDF_DLL_CLASS Usage: public std::runtime_error
    {
      public:
        QPDF_DLL
        Usage(std::string const&);
    };

    // progname_env is used to override argv[0] when figuring out the
    // name of the executable for setting up completion. This may be
    // needed if the program is invoked by a wrapper.
    QPDF_DLL
    QPDFArgParser(int argc, char* argv[], char const* progname_env);

    // Calls exit(0) if a help option is given or if in completion
    // mode. If there are argument parsing errors,
    // QPDFArgParser::Usage is thrown.
    QPDF_DLL
    void parseArgs();

    // Return the program name as the last path element of the program
    // executable.
    QPDF_DLL
    std::string getProgname();

    // Methods for registering arguments. QPDFArgParser starts off
    // with the main option table selected. You can add handlers for
    // arguments in the current option table, and you can select which
    // option table is current. The help option table is special and
    // contains arguments that are only valid as the first and only
    // option. Named option tables are for subparsers and always start
    // a series of options that end with `--`.

    typedef std::function<void()> bare_arg_handler_t;
    typedef std::function<void(char*)> param_arg_handler_t;

    QPDF_DLL
    void selectMainOptionTable();
    QPDF_DLL
    void selectHelpOptionTable();
    QPDF_DLL
    void selectOptionTable(std::string const& name);

    // Register a new options table. This also selects the option table.
    QPDF_DLL
    void registerOptionTable(
        std::string const& name, bare_arg_handler_t end_handler);

    // Add handlers for options in the current table

    QPDF_DLL
    void addPositional(param_arg_handler_t);
    QPDF_DLL
    void addBare(std::string const& arg, bare_arg_handler_t);
    QPDF_DLL
    void addRequiredParameter(
        std::string const& arg,
        param_arg_handler_t,
        char const* parameter_name);
    QPDF_DLL
    void addOptionalParameter(std::string const& arg, param_arg_handler_t);
    QPDF_DLL
    void addChoices(
        std::string const& arg, param_arg_handler_t,
        bool required, char const** choices);

    // If an option is shared among multiple tables and uses identical
    // handlers, you can just copy it instead of repeating the
    // registration call.
    QPDF_DLL
    void copyFromOtherTable(std::string const& arg,
                            std::string const& other_table);

    // The final check handler is called at the very end of argument
    // parsing.
    QPDF_DLL
    void addFinalCheck(bare_arg_handler_t);

    // Convenience methods for adding member functions of a class as
    // handlers.
    template <class T>
    static bare_arg_handler_t bindBare(void (T::*f)(), T* o)
    {
        return std::bind(std::mem_fn(f), o);
    }
    template <class T>
    static param_arg_handler_t bindParam(void (T::*f)(char *), T* o)
    {
        return std::bind(std::mem_fn(f), o, std::placeholders::_1);
    }

    // When processing arguments, indicate how many arguments remain
    // after the one whose handler is being called.
    QPDF_DLL
    int argsLeft() const;

    // Indicate whether we are in completion mode.
    QPDF_DLL
    bool isCompleting() const;

    // Insert a completion during argument parsing; useful for
    // customizing completion in the position argument handler. Should
    // only be used in completion mode.
    QPDF_DLL
    void insertCompletion(std::string const&);

    // Throw a Usage exception with the given message. In completion
    // mode, this just exits to prevent errors from partial commands
    // or other error messages from messing up completion.
    QPDF_DLL
    void usage(std::string const& message);

  private:
    struct OptionEntry
    {
        OptionEntry() :
            parameter_needed(false),
            bare_arg_handler(0),
            param_arg_handler(0)
        {
        }
        bool parameter_needed;
        std::string parameter_name;
        std::set<std::string> choices;
        bare_arg_handler_t bare_arg_handler;
        param_arg_handler_t param_arg_handler;
    };
    typedef std::map<std::string, OptionEntry> option_table_t;

    OptionEntry& registerArg(std::string const& arg);

    void completionCommon(bool zsh);

    void argCompletionBash();
    void argCompletionZsh();
    void argHelp(char*);

    void checkCompletion();
    void handleArgFileArguments();
    void handleBashArguments();
    void readArgsFromFile(char const* filename);
    void doFinalChecks();
    void addOptionsToCompletions(option_table_t&);
    void addChoicesToCompletions(
        option_table_t&, std::string const&, std::string const&);
    void insertCompletions(
        option_table_t&, std::string const&, std::string const&);
    void handleCompletion();

    class Members
    {
        friend class QPDFArgParser;

      public:
        QPDF_DLL
        ~Members() = default;

      private:
        Members(int argc, char* argv[], char const* progname_env);
        Members(Members const&) = delete;

        int argc;
        char** argv;
        char const* whoami;
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
        std::vector<PointerHolder<char>> new_argv;
        std::vector<PointerHolder<char>> bash_argv;
        PointerHolder<char*> argv_ph;
        PointerHolder<char*> bash_argv_ph;
    };
    PointerHolder<Members> m;
};

#endif // QPDFARGPARSER_HH
