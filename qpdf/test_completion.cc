/*

This program is purpose-built for completion.test. It creates a pty (pseudoterminal) pair so that it
can interact with the shell the way a user would. This carries complexity, but it is the most
reliable way to ensure completion works as expected.

For iterating on completion, you can enter a shell configured the way the tests configure it, which
may differ from your default shell behavior. From the cmake build directory, for zsh:

eval $(./qpdf/test_completion --invoke --shell zsh)
source <(./qpdf/test_completion --init --shell zsh)

For bash, it's the same but with --shell bash.

When running in test mode, the --tests argument takes the path to a file
(qpdf/qtest/qpdf/completion-tests as time of initial writing) whose format is as follows:

* Lines starting with `#` are comments and are ignored.
* Blank lines separate test cases; extra blank lines are ignored.
* The rest of the file contains test cases.

A test case has the following format:

name: name of test case
cmd: typed-at-prompt-before-tab
end: unique text; the first occurrence in the output is considered the end of the test's input
end-zsh: overrides end for zsh only
end-bash: overrides end for bash only
out:
 --must-appear
 !--must-not-appear
out-zsh:
 additional for zsh
out-bash:
 additional for bash

NOTES:
* If `end-zsh` or `end-bash` appear, they override `end` for zsh or bash.
* If `out-zsh` or `out-bash` appear, they *add to* `out` for zsh or bash, so `out` contains common
  output.
* If `end` starts with `@`, the output is expected to be terminated by the test prompt followed
  by the original command. Any other text (following @ or as the whole text) is expected to follow
  the original command and also to follow the end text, if different. If the prompt starts with `@`,
  it is an error for `out` to be empty. Otherwise, it is an error for it to be non-empty.

The actual shell output is split into words. Within output, lines must start with *exactly one
space*. If the next character is !, the rest of the line *must not appear* as a word in the output.
Otherwise, the line must appear as a word in the output.

For examples, see the completion-tests file itself.

*/

// iostream is used by the WIN32 branch as well as the POSIX branch.
#include <iostream>

#ifdef _WIN32
int
main()
{
    std::cerr << "completion testing is not available for Windows\n";
    // The test suite checks for this exit status to indicate that completion tests should be
    // skipped.
    return 3;
}
#else

# include <qpdf/QPDFExc.hh>
# include <qpdf/QUtil.hh>

# include <algorithm>
# include <cctype>
# include <cerrno>
# include <csignal>
# include <cstring>
# include <fcntl.h>
# include <map>
# include <optional>
# include <poll.h>
# include <set>
# include <sys/ioctl.h>
# include <unistd.h>

static constexpr std::string_view test_prompt = "@TEST@";
static const char* whoami = nullptr;
static std::string global_buf;
static bool errors = false;
static bool debug = false;

static std::vector<char const*> bash_init = {
    "COMP_QUERY_ITEMS=0\n",
    "bind 'set page-completions off'\n",
    "bind 'set show-all-if-ambiguous on'\n",
    "bind 'set completion-query-items -1'\n",
    "bind 'set bell-style none'\n",
    "bind 'set completion-display-width 0'\n",
    "bind 'set colored-stats off'\n",
    "bind 'set colored-completion-prefix off'\n",
    "source <(qpdf --completion-bash)\n",
    "PS1=@TE''ST@\n",
};
static std::vector<char const*> zsh_init = {
    "autoload -U compinit && compinit -u\n",
    "zstyle ':completion:*' list-prompt ''\n",
    "unset zle_bracketed_paste\n",
    "setopt auto_list\n",
    "setopt glob_dots\n",
    "unsetopt list_ambiguous\n",
    "setopt nobeep\n",
    "unsetopt list_beep\n",
    "LINES=50000\n",
    "source <(qpdf --completion-zsh)\n",
    "PS1=@TE''ST@\n",
};

// How many milliseconds to wait when we are not expecting additional data.
static constexpr int short_timeout = 100;

// How many milliseconds to wait when we are waiting for something that may take a while.
static constexpr int long_timeout = 1000;

struct Descriptors
{
    Descriptors() = default;
    ~Descriptors();
    Descriptors(Descriptors const&) = delete;
    void close_secondary();
    int primary{-1};
    int secondary{-1};
};

struct PtyPair
{
    PtyPair();

    // Store descriptors in a separate struct so it gets destroyed even if PtyPair's constructor
    // throws an exception.
    Descriptors d;
};

PtyPair::PtyPair()
{
    d.primary = QUtil::os_wrapper("posix_openpt", posix_openpt(O_RDWR | O_NOCTTY));
    QUtil::os_wrapper("grantpt", grantpt(d.primary));
    QUtil::os_wrapper("unlockpt", unlockpt(d.primary));
    char* name = ptsname(d.primary);
    if (name == nullptr) {
        QUtil::throw_system_error("ptsname");
    }
    d.secondary = QUtil::os_wrapper("open secondary pty", open(name, O_RDWR));

    // Set a large window size. This makes bash's readline behave better with TERM=dumb, and it
    // effectively bypasses zsh's built-in pager. We also have a LINES statement the zsh init
    // so that sourcing the init in zsh also results in a bypassed pager.
    winsize ws{};
    ws.ws_row = 50000;
    ws.ws_col = 1000;
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;
    QUtil::os_wrapper("set TIOCWINSZ", ioctl(d.secondary, TIOCSWINSZ, &ws));
}

void
Descriptors::close_secondary()
{
    if (secondary != -1) {
        close(secondary);
        secondary = -1;
    }
}

Descriptors::~Descriptors()
{
    if (primary != -1) {
        close(primary);
    }
    if (secondary != -1) {
        close(secondary);
    }
}

struct Output
{
    std::string value;
    bool wanted{false};
};

struct TestCase
{
    std::string name;
    std::string command;
    std::string end;
    std::vector<Output> out;
};

struct Tests
{
    std::vector<TestCase> cases;
};

// ReSharper disable once CppDFAConstantParameter
static ssize_t
safe_read(int fd, char* buf, const size_t size)
{
    while (true) {
        ssize_t len = read(fd, buf, size);
        if (len == -1) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EIO) {
                // EIO on pty primary means the secondary side closed
                return 0;
            }
            QUtil::throw_system_error("read from pty");
        }
        return len;
    }
}

static void
safe_write(int fd, const char* buf, size_t len)
{
    while (len > 0) {
        ssize_t n = write(fd, buf, len);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            QUtil::throw_system_error("write to pty");
        }
        buf += n;
        len -= static_cast<size_t>(n);
    }
}

static std::optional<std::string>
chop_end(std::string_view end_text)
{
    auto pos = global_buf.find(end_text);
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    auto result = global_buf.substr(0, pos);
    global_buf = global_buf.substr(pos + end_text.length());
    return result;
}

static std::string
read_until(int pty, std::string_view end_text, int timeout, int timeout_factor)
{
    std::optional<std::string> result = std::nullopt;
    char buf[1024];
    while (true) {
        result = chop_end(end_text);
        if (result.has_value()) {
            break;
        }

        pollfd fds[1];
        fds[0].fd = pty;
        fds[0].events = POLLIN;
        fds[0].revents = 0;
        nfds_t n_fds = 1;

        int ret = poll(fds, n_fds, timeout * timeout_factor);

        if (ret == 0) {
            // timeout
            break;
        }

        if (ret == -1) {
            if (errno == EINTR) {
                continue;
            }
            QUtil::throw_system_error("poll");
        }

        if (fds[0].revents & (POLLIN | POLLHUP)) {
            ssize_t len = safe_read(pty, buf, sizeof(buf));
            if (len > 0) {
                if (debug) {
                    QUtil::os_wrapper(
                        "stdout",
                        static_cast<int>(write(STDOUT_FILENO, buf, static_cast<size_t>(len))));
                }
                global_buf.append(buf, static_cast<size_t>(len));
            } else {
                break; // pty EOF means child side is gone
            }
        }

        if (fds[0].revents & (POLLERR | POLLNVAL)) {
            break;
        }
    }
    if (!result.has_value()) {
        throw std::runtime_error("didn't find end text");
    }
    return result.value();
}

static std::string
type_and_read_until(int pty, std::string_view text, std::string_view end_text, int timeout_factor)
{
    auto end = std::ranges::find_if_not(
        text, [](char c) { return std::isprint(static_cast<unsigned char>(c)); });
    auto plain = std::string(text.begin(), end);
    safe_write(pty, text.data(), text.size());
    if (!plain.empty()) {
        // Read and discard the echoed plain text part of the command.
        read_until(pty, plain, short_timeout, timeout_factor);
    }
    return read_until(pty, end_text, short_timeout, timeout_factor);
}

static pid_t
start_shell(int primary, int secondary, char const* argv[])
{
    switch (pid_t child_pid = fork()) {
    case 0:
        break;

    case -1:
        QUtil::throw_system_error("fork");

    default:
        return child_pid;
    }

    // child process
    setsid();
    ioctl(secondary, TIOCSCTTY, 0);

    close(primary);
    dup2(secondary, STDIN_FILENO);
    dup2(secondary, STDOUT_FILENO);
    dup2(secondary, STDERR_FILENO);
    if (secondary > STDERR_FILENO) {
        close(secondary);
    }

    setenv("TERM", "dumb", 1);
    execvp(argv[0], const_cast<char**>(argv));
    std::cerr << "exec " << argv[0] << " failed: " << strerror(errno) << "\n";
    _exit(2);
}

static void
usage(std::string_view s)
{
    std::cerr << s << "\n";
    std::cerr << "Usage: " << whoami
              << " --shell {bash|zsh} { --invoke | --init | --tests x } [--debug]\n";
    std::cerr << " --invoke -- print the command to start the shell\n";
    std::cerr << " --init -- print the commands to initialize the shell\n";
    std::cerr << " --tests test_file -- run tests from the given file\n";
    std::cerr << "To test manually (or substitute zsh with bash below):\n";
    std::cerr << "  eval $(test_completion --shell zsh --invoke)\n";
    std::cerr << "  source <(test_completion --shell zsh --init)\n";
    _exit(2);
}

static bool
input_ready(int pty)
{
    if (!global_buf.empty()) {
        return true;
    }

    while (true) {
        pollfd fd{};
        fd.fd = pty;
        fd.events = POLLIN;
        fd.revents = 0;
        nfds_t n_fds = 1;

        int ret = poll(&fd, n_fds, short_timeout);
        if (ret == 0) {
            return false;
        }
        if (ret == -1) {
            if (errno == EINTR) {
                continue;
            }
            QUtil::throw_system_error("poll");
        }
        return true;
    }
}

static void
error(std::string_view msg)
{
    errors = true;
    // Use std::cout here since this is reporting test failures, not program errors.
    std::cout << msg << "\n";
}

static bool
check_no_input(int pty, std::string const& where)
{
    if (input_ready(pty)) {
        if (global_buf.empty()) {
            char buf[100];
            auto len = safe_read(pty, buf, sizeof(buf));
            if (len > 0) {
                global_buf.append(buf, static_cast<size_t>(len));
            }
        }
        std::cerr << "available input: |" << global_buf << "|\n";
        error("input is unexpectedly ready " + where);
        return false;
    }
    return true;
}

static void
setup(int pty, char const* prompt, std::vector<char const*> const& init, int timeout_factor)
{
    read_until(pty, prompt, long_timeout, timeout_factor);
    for (auto const& cmd: init) {
        safe_write(pty, cmd, strlen(cmd));
    }
    read_until(pty, test_prompt, long_timeout, timeout_factor);
    check_no_input(pty, "after setup");
}

static std::set<std::string>
split_into_words(std::string const& out)
{
    std::set<std::string> words;
    size_t i = 0;
    while (i < out.size()) {
        while (i < out.size() && std::isspace(static_cast<unsigned char>(out[i]))) {
            ++i;
        }
        size_t start = i;
        while (i < out.size() && !std::isspace(static_cast<unsigned char>(out[i]))) {
            ++i;
        }
        if (i > start) {
            std::string w = out.substr(start, i - start);
            if (w != "%") {
                words.insert(std::move(w));
            }
        }
    }
    return words;
}

static std::string
complete(
    int pty,
    std::string_view fragment,
    bool expect_prompt,
    std::string_view extra,
    int timeout_factor)
{
    std::string end_text;
    if (expect_prompt) {
        end_text += test_prompt;
        end_text += fragment;
    }
    std::string text(fragment);
    text += "\t";
    end_text += extra;
    std::string out;
    try {
        out = type_and_read_until(pty, text, end_text, timeout_factor);
    } catch (std::runtime_error const& e) {
        // Don't treat this as a fatal error; just try to resync below.
        error(std::string("while looking for ") + end_text + ": " + e.what());
    }
    if (!extra.empty() && out.starts_with(extra)) {
        // zsh echoes extra text before and after choices; bash only echoes it after choices.
        // If the shell expanded to the unique prefix before choices, consume it so we don't
        // see it in the output as a separate word.
        out = out.substr(extra.length());
    }
    check_no_input(pty, "after expected output");
    // send CTRL-U to clear the line. This can generate lots of spaces and multiple prompt
    // occurrences, so ignore its output.
    safe_write(pty, "\x15", 1);
    // Resync on a colon.
    type_and_read_until(pty, ":\n", test_prompt, timeout_factor);
    if (!check_no_input(pty, "after blank line")) {
        throw std::runtime_error("unable to resync; bailing");
    }
    return out;
}

static void
do_test(int pty, std::string_view shell, TestCase const& test_case, int timeout_factor)
{
    // No output is issued for passing tests, so any output or error results in a test failure.
    auto extra = test_case.end;
    bool expect_prompt = false;
    if (!extra.empty() && extra.at(0) == '@') {
        extra = extra.substr(1);
        expect_prompt = true;
    }
    auto raw_out = complete(pty, test_case.command, expect_prompt, extra, timeout_factor);
    auto words = split_into_words(raw_out);
    std::set<std::string_view> wanted;
    std::set<std::string_view> not_wanted;
    for (auto const& out: test_case.out) {
        if (out.wanted) {
            wanted.insert(out.value);
        } else {
            not_wanted.insert(out.value);
        }
    }
    bool passed = true;
    for (auto const& word: words) {
        if (not_wanted.contains(word)) {
            passed = false;
            error("test " + test_case.name + ": " + word + " seen but not wanted");
        }
    }
    for (auto const& word: wanted) {
        if (!words.contains(std::string(word))) {
            passed = false;
            error("test " + test_case.name + ": " + std::string(word) + " wanted but not seen");
        }
    }
    if (!passed) {
        std::cout << "--- test " << test_case.name << " (" << shell << ") failed; output: ---\n"
                  << "|" << raw_out << "|\n"
                  << "\n--- test " << test_case.name << " words ---\n";
        for (auto const& w: words) {
            std::cout << w << "\n";
        }
        std::cout << "--- test " << test_case.name << " end output ---\n";
    }
}

static void
run(int pty, std::string_view shell, Tests const& tests, int timeout_factor)
{
    for (auto const& test_case: tests.cases) {
        do_test(pty, shell, test_case, timeout_factor);
    }
}

static Tests
parse_tests(char const* test_file, char const* shell)
{
    // This is a relatively loose parser as it is only reading test files that are part of the
    // qpdf test suite. It doesn't have to be super helpful to the user. It basically only ever
    // parsers one input file in its life. As such, we only parse enough to get what we need out
    // of a correct input file.
    std::string shell_out = std::string("out-") + shell;
    std::string shell_end = std::string("end-") + shell;
    auto lines = QUtil::read_lines_from_file(test_file);
    int lineno = 0;
    std::vector<TestCase> cases;
    TestCase* cur = nullptr;
    bool accumulating = false;
    for (auto const& line: lines) {
        ++lineno;
        if (line.empty()) {
            // Blank lines separate test cases
            if (cur) {
                cur = nullptr;
                accumulating = false;
            }
            continue;
        }
        if (line.starts_with('#')) {
            continue;
        }
        auto sep = line.find(':');
        if (sep != std::string::npos) {
            accumulating = false;
            if (!cur) {
                cases.push_back(TestCase{});
                cur = &cases.back();
            }
            auto key = line.substr(0, sep);
            auto value = line.substr(sep + 1);
            auto pos = value.find_first_not_of(' ');
            if (pos != std::string::npos) {
                value = value.substr(pos);
            }
            if (key == "name") {
                cur->name = value;
            } else if (key == "cmd") {
                cur->command = value;
            } else if (key == shell_end || (key == "end" && cur->end.empty())) {
                // shell_end overrides end
                cur->end = value;
            } else if (key == shell_out || key == "out") {
                // shell_out and out accumulate together
                accumulating = true;
            }
        } else if (cur && line.at(0) == ' ') {
            if (accumulating) {
                auto rest = line.substr(1);
                bool wanted = !(rest.length() > 1 && rest.at(0) == '!');
                if (!wanted) {
                    rest = rest.substr(1);
                }
                cur->out.push_back(
                    Output{
                        .value = rest,
                        .wanted = wanted,
                    });
            }
        } else {
            error(
                std::string(test_file) + ":" + std::to_string(lineno) +
                ": invalid syntax; see comments in test_completion.cc");
        }
    }

    // Minimal validation
    std::set<std::string> names;
    for (auto const& test_case: cases) {
        if (test_case.name.empty()) {
            error("a test case is missing a name");
        } else {
            if (names.contains(test_case.name)) {
                error(test_case.name + " is duplicated as a test name");
            }
            names.insert(test_case.name);
        }
        if (test_case.end.starts_with('@') == test_case.out.empty()) {
            error(test_case.name + ": out is required if and only if end starts with @");
        }
    }
    if (errors) {
        throw std::runtime_error(std::string("errors parsing ") + test_file);
    }

    return Tests{.cases = cases};
}

bool
do_tests(
    char const* test_file,
    char const* shell,
    char const* shell_argv[],
    char const* prompt,
    std::vector<char const*> const& init,
    int timeout_factor)
{
    pid_t child_pid = 0;
    try {
        auto tests = parse_tests(test_file, shell);
        if (debug) {
            std::cout << "-- BEGIN TESTS --\n";
            for (auto const& test_case: tests.cases) {
                std::cout << "name: " << test_case.name << "\n";
                std::cout << "cmd: " << test_case.command << "\n";
                std::cout << "end: " << test_case.end << "\n";
                std::cout << "out:\n";
                for (auto const& out: test_case.out) {
                    std::cout << "  " << out.value << " (wanted=" << out.wanted << ")\n";
                }
                std::cout << "\n";
            }
            std::cout << "-- END TESTS --\n";
        }
        PtyPair pty_pair;
        child_pid = start_shell(pty_pair.d.primary, pty_pair.d.secondary, shell_argv);
        pty_pair.d.close_secondary();
        setup(pty_pair.d.primary, prompt, init, timeout_factor);
        run(pty_pair.d.primary, shell, tests, timeout_factor);
    } catch (std::exception& e) {
        errors = true;
        std::cerr << whoami << ": ERROR: " << e.what() << "\n";
    }
    // Kill child process if still running, but ignore any errors.
    if (child_pid > 0) {
        static_cast<void>(kill(-child_pid, SIGHUP));
    }
    return errors;
}

int
main(int argc, char* argv[])
{
    if ((whoami = strrchr(argv[0], '/')) == nullptr) {
        whoami = argv[0];
    } else {
        ++whoami;
    }

    char* test_file = nullptr;
    char* shell = nullptr;
    bool invoke = false;
    bool init_mode = false;
    std::vector<char const*>* init = nullptr;
    for (int arg = 1; arg < argc; ++arg) {
        if (strcmp(argv[arg], "--debug") == 0) {
            debug = true;
        } else if (strcmp(argv[arg], "--invoke") == 0) {
            invoke = true;
        } else if (strcmp(argv[arg], "--init") == 0) {
            init_mode = true;
        } else if (strcmp(argv[arg], "--shell") == 0) {
            if (++arg >= argc) {
                usage("--shell requires an argument");
            }
            shell = argv[arg];
        } else if (strcmp(argv[arg], "--tests") == 0) {
            if (++arg >= argc) {
                usage("--tests requires an argument");
            }
            test_file = argv[arg];
        } else {
            usage(std::string("unknown argument ") + argv[arg]);
        }
    }

    if (!shell) {
        usage("--shell is required");
    }
    if ((test_file ? 1 : 0) + (invoke ? 1 : 0) + (init_mode ? 1 : 0) != 1) {
        usage("exactly one of --test-file, --invoke, or --init is required");
    }

    std::vector<char const*> shell_argv;
    char const* prompt;
    if (strcmp(shell, "zsh") == 0) {
        shell_argv = {"zsh", "-f", nullptr};
        prompt = "% ";
        init = &zsh_init;
    } else if (strcmp(shell, "bash") == 0) {
        shell_argv = {"bash", "--noprofile", "--norc", nullptr};
        prompt = "$ ";
        init = &bash_init;
    } else {
        std::cerr << "unknown shell " << shell << "\n";
        std::exit(2);
    }

    if (invoke) {
        bool first = true;
        for (auto const* arg: shell_argv) {
            if (!arg) {
                break;
            }
            if (first) {
                first = false;
            } else {
                std::cout << " ";
            }
            std::cout << arg;
        }
        std::cout << "\n";
    } else if (init_mode) {
        for (auto const& cmd: *init) {
            std::cout << cmd;
        }
    } else {
        bool first = true;
        for (int timeout_factor: {1, 2, 5}) {
            errors = false;
            if (first) {
                first = false;
            } else {
                std::cout << "*** retrying with longer timeout ***\n";
            }
            errors = do_tests(test_file, shell, shell_argv.data(), prompt, *init, timeout_factor);
            if (!errors) {
                std::cout << "All tests passed.\n";
                break;
            }
        }
    }

    return errors ? 2 : 0;
}

#endif // _WIN32