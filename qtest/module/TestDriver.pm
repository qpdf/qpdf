# -*- perl -*-
#
# This file is part of qtest.
#
# Copyright 1993-2007, Jay Berkenbilt
#
# QTest is distributed under the terms of version 2.0 of the Artistic
# license which may be found in the source distribution.
#

# Search for "PUBLIC METHODS" to find the public methods and
# documentation on how to use them.

require 5.008;
use strict;

package TestDriver::PidKiller;

use vars qw($f_pid);
$f_pid = 'pid';

sub new
{
    my $class = shift;
    my $rep = +{+__PACKAGE__ => {} };
    $rep->{+__PACKAGE__}{$f_pid} = shift;
    bless $rep, $class;
}

sub DESTROY
{
    my $rep = shift;
    my $pid = $rep->{+__PACKAGE__}{$f_pid};
    defined($$pid) && $$pid && kill 15, $$pid;
}

package TestDriver;

use IO::Handle;
use IO::File;
use IO::Socket;
use IO::Select;
use POSIX ':sys_wait_h';
use File::Copy;
use File::Find;
use Carp;
use Cwd;
require QTC;

# Constants

# Possible test case outcomes
use constant PASS => 'PASS';
use constant FAIL => 'FAIL';

# Input/Output keys
use constant STRING => 'STRING';
use constant FILE => 'FILE';
use constant COMMAND => 'COMMAND';
use constant FILTER => 'FILTER';
use constant REGEXP => 'REGEXP';
use constant EXIT_STATUS => 'EXIT_STATUS';
use constant THREAD_DATA => 'THREAD_DATA';
use constant TD_THREADS => 'TD_THREADS';
use constant TD_SEQGROUPS => 'TD_SEQGROUPS';

# Flags
use constant NORMALIZE_NEWLINES =>   1 << 0;
use constant NORMALIZE_WHITESPACE => 1 << 1;
use constant EXPECT_FAILURE =>       1 << 2;

# Field names
use vars qw($f_socket $f_origdir $f_tempdir $f_testlog $f_testxml $f_suitename);
$f_socket = 'socket';
$f_origdir = 'origdir';
$f_tempdir = 'tempdir';
$f_testlog = 'testlog';
$f_testxml = 'testxml';
$f_suitename = 'suitename';

use vars qw($f_passes $f_fails $f_xpasses $f_xfails $f_testnum);
$f_passes = 'passes';		# expected passes
$f_fails = 'fails';		# unexpected failures
$f_xpasses = 'xpasses';		# unexpected passes
$f_xfails = 'xfails';		# expected failures
$f_testnum = 'testnum';

# Static Variables

# QTEST_MARGIN sets the number of spaces to after PASSED or FAILED and
# before the rightmost column of the screen.
my $margin = $ENV{'QTEST_MARGIN'} || 8;
$margin += $ENV{'QTEST_EXTRA_MARGIN'} || 0;

my $ncols = 80;

my $color_reset = "";
my $color_green = "";
my $color_yellow = "";
my $color_red = "";
my $color_magenta = "";
my $color_emph = "";

# MSWin32 support
my $in_windows = 0;
my $winbin = undef;
if ($^O eq 'MSWin32')
{
    $in_windows = 1;
}

sub get_tty_features
{
    my $got_size = 0;
    eval
    {
	require Term::ReadKey;
	($ncols, undef, undef, undef) = Term::ReadKey::GetTerminalSize();
	$got_size = 1;
    };
    if (! $got_size)
    {
	eval
	{
	    # Get screen columns if possible
	    no strict;
	    local $^W = 0;
	    local *X;
	    {
		local $SIG{'__WARN__'} = sub {};
		require 'sys/ioctl.ph';
	    }
	    if ((defined &TIOCGWINSZ) && open(X, "+</dev/tty"))
	    {
		my $winsize = "";
		if (ioctl(X, &TIOCGWINSZ, $winsize))
		{
		    (undef, $ncols) = unpack('S4', $winsize);
		    $got_size = 1;
		}
		close(X);
	    }
	};
    }
    eval
    {
	if ($in_windows)
	{
	    eval
	    {
		# If you don't have this module, you may want to set
		# the environment variable ANSI_COLORS_DISABLED to 1
		# to avoid "garbage" output around PASSED, FAILED,
		# etc.
		require Win32::Console::ANSI;
	    }
	}
	require Term::ANSIColor;
	$color_reset = Term::ANSIColor::RESET();
	$color_green = Term::ANSIColor::GREEN();
	$color_yellow = Term::ANSIColor::YELLOW();
	$color_red = Term::ANSIColor::RED();
	$color_magenta = Term::ANSIColor::MAGENTA();
	$color_emph = Term::ANSIColor::color('bold blue on_black');
    };
}

# Static Methods

sub print_and_pad
{
    my $str = shift;
    my $spaces = $ncols - 10 - length($str) - $margin;
    $spaces = 0 if $spaces < 0;
    print $str . (' ' x $spaces) . ' ... ';
}

sub print_results
{
    my ($outcome, $exp_outcome) = @_;

    my $color = "";
    my $outcome_text;
    if ($outcome eq $exp_outcome)
    {
	if ($outcome eq PASS)
	{
	    &QTC::TC("testdriver", "TestDriver expected pass");
	    $color = $color_green;
	    $outcome_text = "PASSED";
	}
	else
	{
	    &QTC::TC("testdriver", "TestDriver expected fail");
	    $color = $color_yellow;
	    # " (exp)" is fewer characters than the default margin
	    # which keeps this from wrapping lines with default
	    # settings.
	    $outcome_text = "FAILED (exp)";
	}
    }
    else
    {
	if ($outcome eq PASS)
	{
	    &QTC::TC("testdriver", "TestDriver unexpected pass");
	    $color = $color_magenta;
	    $outcome_text = "PASSED-UNEXP";
	}
	else
	{
	    &QTC::TC("testdriver", "TestDriver unexpected fail");
	    $color = $color_red;
	    $outcome_text = "FAILED";
	}
    }

    print $color, $outcome_text, $color_reset, "\n";
    $outcome_text;
}

# Normal Methods

sub new
{
    my $class = shift;
    my $rep = +{+__PACKAGE__ => {} };

    if (@_ != 1)
    {
	croak "Usage: ", __PACKAGE__, "->new(\"test-suite name\")\n";
    }
    my $suitename = shift;

    if (! ((@ARGV == 11) &&
	   (($ARGV[0] eq '-fd') || ($ARGV[0] eq '-port')) &&
	   ($ARGV[2] eq '-origdir') &&
	   ($ARGV[4] eq '-tempdir') &&
	   ($ARGV[6] eq '-testlog') &&
	   ($ARGV[8] eq '-testxml') &&
	   ($ARGV[10] =~ m/^-stdout-tty=([01])$/) &&
	   (-d $ARGV[5])))
    {
	die +__PACKAGE__, ": improper invocation of test driver $0 (" .
	    join(' ', @ARGV) . ")\n";
    }
    my $fd = ($ARGV[0] eq '-fd') ? $ARGV[1] : undef;
    my $port = ($ARGV[0] eq '-port') ? $ARGV[1] : undef;
    my $origdir = $ARGV[3];
    my $tempdir = $ARGV[5];
    my $testlogfile = $ARGV[7];
    my $testxmlfile = $ARGV[9];
    my $testlog = new IO::File(">>$testlogfile");
    binmode $testlog;
    my $testxml = new IO::File(">>$testxmlfile");
    binmode $testxml;
    $ARGV[10] =~ m/=([01])/ or die +__PACKAGE__, ": INTERNAL ERROR in ARGV[10]";
    my $stdout_is_tty = $1;
    if ($stdout_is_tty)
    {
	get_tty_features();
    }

    my $socket;
    if (defined $fd)
    {
	$socket = new IO::Handle;
	if (! $socket->fdopen($fd, "w+"))
	{
	    warn +__PACKAGE__, ": unable to open file descriptor $fd.\n";
	    warn +__PACKAGE__, " must be created from a program invoked by" .
		" the test driver system\n";
	    die +__PACKAGE__, ": initialization failed";
	}
    }
    else
    {
	$socket = IO::Socket::INET->new(
	    PeerAddr => '127.0.0.1', PeerPort => $port) or
	    die "unable to connect to port $port: $!\n";
    }
    $socket->autoflush();
    binmode $socket;

    # Do some setup that would ordinarily be reserved for a main
    # program.  We want test suites to behave in a certain way so tha
    # the overall system works as desired.

    # Killing the driver should cause to to exit.  Without this, it
    # may cause whatever subsidiary program is being run to exit and
    # the driver to continue to the next test case.
    $SIG{'INT'} = $SIG{'HUP'} = $SIG{'TERM'} = $SIG{'QUIT'} = sub { exit 2 };

    # Unbuffer our output.
    $| = 1;

    $rep->{+__PACKAGE__}{$f_socket} = $socket;
    $rep->{+__PACKAGE__}{$f_origdir} = $origdir;
    $rep->{+__PACKAGE__}{$f_tempdir} = $tempdir;
    $rep->{+__PACKAGE__}{$f_testlog} = $testlog;
    $rep->{+__PACKAGE__}{$f_testxml} = $testxml;
    $rep->{+__PACKAGE__}{$f_suitename} = $suitename;
    $rep->{+__PACKAGE__}{$f_passes} = 0;
    $rep->{+__PACKAGE__}{$f_fails} = 0;
    $rep->{+__PACKAGE__}{$f_xpasses} = 0;
    $rep->{+__PACKAGE__}{$f_xfails} = 0;
    $rep->{+__PACKAGE__}{$f_testnum} = 1;

    # Do protocol handshaking with the test driver system
    my $init = scalar(<$socket>);
    if ($init !~ m/^TEST_DRIVER 1$/)
    {
	die +__PACKAGE__, ": incorrect protocol with test driver system\n";
    }
    $socket->print("TEST_DRIVER_CLIENT 1\n");

    bless $rep, $class;
}

sub _socket
{
    my $rep = shift;
    $rep->{+__PACKAGE__}{$f_socket};
}

sub _tempdir
{
    my $rep = shift;
    $rep->{+__PACKAGE__}{$f_tempdir};
}

sub _testlog
{
    my $rep = shift;
    $rep->{+__PACKAGE__}{$f_testlog};
}

sub _testxml
{
    my $rep = shift;
    $rep->{+__PACKAGE__}{$f_testxml};
}

sub _suitename
{
    my $rep = shift;
    $rep->{+__PACKAGE__}{$f_suitename};
}

sub _testnum
{
    my $rep = shift;
    $rep->{+__PACKAGE__}{$f_testnum} = $_[0] if @_;
    $rep->{+__PACKAGE__}{$f_testnum};
}

# PUBLIC METHODS

# Usage: report(n)
# Specify the number of tests that are expected to have been run.
# Please note: the purpose of reporting the number of test cases with
# "report" is as an extra check to make sure that the test suite
# itself didn't have a logic error that caused some test cases to be
# skipped.  The argument to "report" should therefore be a hard-coded
# number or a number computed only from static features in the test
# suite.  It should not be a number that is counted up during the
# process of running the test suite.  Computing this number as a side
# effect of running test cases would defeat the purpose of the number.
# For example, if the test suite consists of an array of test cases,
# and the test suite code iterates through that loop and calls
# "runtest" twice for each element, it would be reasonable to pass an
# expression that includes the size of the array as an argument to
# "report", but it would not be appropriate to have a variable called
# "$ntests" that is incremented each time "runtest" is called and then
# passed to "report".
sub report
{
    my $rep = shift;
    croak "Usage: ", __PACKAGE__, "->report(num-tests-expected)\n"
	unless @_ && $_[0] =~ m/^\d+$/;

    # Message to test driver system:
    # n-expected-tests passes fails unexpected-passes expected-fails

    my @vals = (shift);
    push(@vals, map { $rep->{+__PACKAGE__}{$_} } ($f_passes, $f_fails,
						  $f_xpasses, $f_xfails));
    my $socket = $rep->_socket();
    $socket->print(join(' ', @vals)), "\n";
}

# Usage: notify(string)
# Prints the string followed by a newline to standard output of the
# test suite.
sub notify
{
    my $rep = shift;
    my $msg = shift;
    &QTC::TC("testdriver", "TestDriver notify");
    print $msg, "\n";
}

# Usage: emphasize(string)
# Prints the string followed by a newline to standard output of the
# test suite.  The string is printed with emphasis if the terminal
# supports color.
sub emphasize
{
    my $rep = shift;
    my $msg = shift;
    &QTC::TC("testdriver", "TestDriver emphasize");
    print $color_emph, $msg, $color_reset, "\n";
}

# Usage: prompt(msg, env, default)
# If the environment variable "env" is set, its value is returned.
# Otherwise, if STDIN is a tty, the user is prompted for an answer
# using msg as the prompt, or if STDIN is not a tty, the value
# specified in "default" is returned.  Note that careless use of
# prompt in test suites may make the test suites unable to be run in
# batch mode.
sub prompt
{
    my $rep = shift;
    my ($msg, $env, $default) = @_;
    &QTC::TC("testdriver", "TestDriver prompt");
    my $answer = $ENV{$env};
    if (defined $answer)
    {
	print "$msg\n";
	print "[Question answered from environment variable \$$env: $answer]\n";
    }
    else
    {
	print "To avoid question, place answer in" .
	    " environment variable \$$env\n";
	# Note: ActiveState perl 5.10.1 gives the wrong answer for -t
	# STDIN.
	if ((-t STDIN) && (-t STDOUT))
	{
	    print "$msg ";
	    chop($answer = <STDIN>);
	    if ($answer eq '')
	    {
		print "[Using default answer for question: $default]\n";
		$answer = $default;
	    }
	}
	else
	{
	    print "$msg\n";
	    print "[Using default answer for question: $default]\n";
	    $answer = $default;
	}
    }
    $answer;
}

# Usage: get_start_dir()
# Returns the name of the directory from which the test driver was
# originally invoked.  This can be useful for test suites that are
# designed to be run from read-only areas or from multiple locations
# simultaneously: they can get the original invocation directory and
# use it as a place to write temporary files.
sub get_start_dir
{
    my $rep = shift;
    $rep->{+__PACKAGE__}{$f_origdir};
}

# Usage: runtest description input output [ flags ]
# Returns true iff test passes; i.e., input matches output

# Parameters:

#   description: a short textual description of the test case

#   input: a hash reference that defines the input to the test case
#   input keys and associated values:

#      STRING: a string that is used verbatim as the test input

#      FILE: a file whose contents are used as the test input

#      COMMAND: an array reference containing a command and arguments
#      or a string representing the command.  This is passed to exec,
#      so the rules that exec uses to determine whether to pass this
#      to a shell are followed.  The command is run with STDIN set to
#      /dev/null, STDOUT redirected to an internal file, and STDERR
#      copied to STDOUT.

#      Note that exactly one of STRING, FILE, or COMMAND must appear.

#      FILTER: if specified, it is a program that is run on the test
#      input specified above to generate the true test input.

#   output: a hash reference that defines the expected output of the
#   test case

#      STRING: a string that contains the expected test output

#      FILE: a file that contains the expected test output

#      REGEXP: a regular expression that must match the test output

#      Note that exactly one of STRING, FILE, or REGEXP must appear.

#      EXIT_STATUS: the exit status of the command.  Required iff the
#      intput is specified by COMMAND.  A value of undef means that we
#      don't care about the exit status of a command.  The special
#      value of '!0' means we allow any abnormal exit status but we
#      don't care what the specific exit status is.  An integer value
#      is the ordinary exit status of a command.  A string of the form
#      SIG:n indicates that the program has exited with signal n.
#      Note that SIG:n is not reliable in a Windows (non-Cygwin)
#      environment.

#      THREAD_DATA: If specified, the test output is expected to
#      contain multithreaded output with output lines marked by thread
#      and sequence group identifiers.  The value must be a hash that
#      contains required key TD_THREADS and optional key TD_SEQGROUPS.
#      The value of each key is an array reference containing a list
#      of threads or sequence groups as appropriate.  When THREAD_DATA
#      is specified, the single call to runtest actually generates t +
#      s + 3 tests where "t" is the number of threads and "s" is the
#      number of sequence groups specified.  See the documentation for
#      full details on how multithreaded output is handled by the test
#      driver.

#   flags: additional flags to control the test case; should be
#   logically orred together (e.g. NORMALIZE_WHITESPACE | EXPECT_FAILURE)

#      NORMALIZE_NEWLINES: If specified, all newlines or carriage
#      return/newline combinations in the input are translated to
#      straight UNIX-style newlines.  This is done before writing
#      through any filter.  Newlines are also normalized in the
#      expected output.

#      NORMALIZE_WHITESPACE: If specified, all carriage returns are
#      removed, and all strings of one or more space or tab characters
#      are replaced by a single space character in the input.  This is
#      done before writing through any filter.  The expected output
#      must be normalized in this way as well in order for the test to
#      pass.

#      EXPECT_FAILURE: If specified, the test case is expected to
#      fail.  In this case, a test case failure will not generate
#      verbose output or cause overall test suite failure, and a pass
#      will generate test suite failure.  This should be used for
#      place-holder test cases that exercise a known bug that cannot
#      yet be fixed.

sub runtest
{
    my $rep = shift;

    if (! ((@_ == 3) || (@_ == 4)))
    {
	croak +("Usage: ", +__PACKAGE__,
		"->runtest(description, input, output[, flags])\n");
    }

    my ($description, $input, $output, $flags) = @_;
    $flags = 0 unless defined $flags;

    my $tempdir = $rep->_tempdir();

    if (ref($description) ne '')
    {
	&QTC::TC("testdriver", "TestDriver description not string");
	croak +__PACKAGE__, "->runtest: description must be a string\n";
    }
    if (ref($input) ne 'HASH')
    {
	&QTC::TC("testdriver", "TestDriver input not hash");
	croak +__PACKAGE__, "->runtest: input must be a hash reference\n";
    }
    if (ref($output) ne 'HASH')
    {
	&QTC::TC("testdriver", "TestDriver output not hash");
	croak +__PACKAGE__, "->runtest: output must be a hash reference\n";
    }
    if ((ref($flags) ne '') || ($flags !~ m/^\d+$/))
    {
	&QTC::TC("testdriver", "TestDriver flags not integer");
	croak +__PACKAGE__, "->runtest: flags must be an integer\n";
    }

    my ($extra_in_keys, $in_string, $in_file, $in_command, $in_filter) =
	check_hash_keys($input, $rep->STRING,
			$rep->FILE, $rep->COMMAND, $rep->FILTER);
    if ($extra_in_keys)
    {
	&QTC::TC("testdriver", "TestDriver extraneous input keys");
	croak +(+__PACKAGE__,
		"->runtest: extraneous keys in intput hash: $extra_in_keys\n");
    }
    my ($extra_out_keys, $out_string, $out_file, $out_regexp,
	$out_exit_status, $thread_data) =
	    check_hash_keys($output, $rep->STRING,
			    $rep->FILE, $rep->REGEXP, $rep->EXIT_STATUS,
			    $rep->THREAD_DATA);
    if ($extra_out_keys)
    {
	&QTC::TC("testdriver", "TestDriver extraneous output keys");
	croak +(+__PACKAGE__,
		"->runtest: extraneous keys in output hash: $extra_out_keys\n");
    }

    if ((((defined $in_string) ? 1 : 0) +
	 ((defined $in_file) ? 1 : 0) +
	 ((defined $in_command) ? 1 : 0)) != 1)
    {
	&QTC::TC("testdriver", "TestDriver invalid input");
	croak +__PACKAGE__, "->runtest: exactly one of" .
	    " STRING, FILE, or COMMAND must be present for input\n";
    }
    if ((((defined $out_string) ? 1 : 0) +
	 ((defined $out_file) ? 1 : 0) +
	 ((defined $out_regexp) ? 1 : 0)) != 1)
    {
	&QTC::TC("testdriver", "TestDriver invalid output");
	croak +__PACKAGE__, "->runtest: exactly one of" .
	    " STRING, FILE, or REGEXP must be present for output\n";
    }
    if ((defined $in_command) != (exists $output->{$rep->EXIT_STATUS}))
    {
	&QTC::TC("testdriver", "TestDriver invalid status");
	croak +__PACKAGE__, "->runtest: input COMMAND and output EXIT_STATUS"
	    . " must either both appear both not appear\n";
    }

    my ($threads, $seqgroups) = (undef, undef);
    if (defined $thread_data)
    {
	if (ref($thread_data) ne 'HASH')
	{
	    &QTC::TC("testdriver", "TestDriver thread_data not hash");
	    croak +__PACKAGE__, "->runtest: THREAD_DATA" .
		" must be a hash reference\n";
	}
	my $extra_thread_keys;
	($extra_thread_keys, $threads, $seqgroups) =
	    check_hash_keys($thread_data, $rep->TD_THREADS, $rep->TD_SEQGROUPS);
	if ($extra_thread_keys)
	{
	    &QTC::TC("testdriver", "TestDriver extraneous thread_data keys");
	    croak +(+__PACKAGE__,
		    "->runtest: extraneous keys in THREAD_DATA hash:" .
		    " $extra_thread_keys\n");
	}
	if (! defined $threads)
	{
	    &QTC::TC("testdriver", "TestDriver thread_data no threads");
	    croak +__PACKAGE__, "->runtest: THREAD_DATA" .
		" must contain TD_THREADS\n";
	}
	elsif (ref($threads) ne 'ARRAY')
	{
	    &QTC::TC("testdriver", "TestDriver threads not array ref");
	    croak +__PACKAGE__, "->runtest: TD_THREADS" .
		" must be an array reference\n";
	}
	if ((defined $seqgroups) && (ref($seqgroups) ne 'ARRAY'))
	{
	    &QTC::TC("testdriver", "TestDriver seqgroups not array ref");
	    croak +__PACKAGE__, "->runtest: TD_SEQGROUPS" .
		" must be an array reference\n";
	}
    }

    # testnum is incremented by print_testid
    my $testnum = $rep->_testnum();
    my $category = $rep->_suitename();
    $rep->print_testid($description);

    # Open a file handle to read the raw (unfiltered) test input
    my $pid = undef;
    my $pid_killer = new TestDriver::PidKiller(\$pid);
    my $in = new IO::Handle;
    my $use_tempfile = $in_windows;
    my $tempout_status = undef;
    if (defined $in_string)
    {
	&QTC::TC("testdriver", "TestDriver input string");
	open($in, '<', \$in_string) or
	    die +(+__PACKAGE__,
		  "->runtest: unable to read from input string: $!\n");
    }
    elsif (defined $in_file)
    {
	&QTC::TC("testdriver", "TestDriver input file");
	open($in, '<', $in_file) or
	    croak +(+__PACKAGE__,
		    "->runtest: unable to read from input file $in_file: $!\n");
    }
    elsif (defined $in_command)
    {
	if (ref($in_command) eq 'ARRAY')
	{
	    &QTC::TC("testdriver", "TestDriver input command array");
	}
	elsif (ref($in_command) eq '')
	{
	    &QTC::TC("testdriver", "TestDriver input command string");
	}

	if ($use_tempfile)
	{
	    my $tempout = "$tempdir/tempout";
	    $tempout_status = $rep->winrun(
		$in_command, File::Spec->devnull(), $tempout);
	    open($in, "<$tempout") or
		croak +(+__PACKAGE__,
			"->runtest: unable to read from" .
			" input file $tempout: $!\n");
	}
	else
	{
	    $pid = open($in, "-|");
	    croak +__PACKAGE__, "->runtest: fork failed: $!\n"
		unless defined $pid;
	    if ($pid == 0)
	    {
		open(STDERR, ">&STDOUT");
		open(STDIN, '<', \ "");
		if (ref($in_command) eq 'ARRAY')
		{
		    exec @$in_command or
			croak+(+__PACKAGE__,
			       "->runtest: unable to run command ",
			       join(' ', @$in_command), "\n");
		}
		else
		{
		    exec $in_command or
			croak+(+__PACKAGE__,
			       "->runtest: unable to run command ",
			       $in_command, "\n");
		}
	    }
	}
    }
    else
    {
	die +__PACKAGE__, ": INTERNAL ERROR: invalid test input";
    }
    binmode $in;

    # Open file handle into which to write the actual output
    my $actual = new IO::File;
    my $actual_file = "$tempdir/actual";

    if (defined $in_filter)
    {
	&QTC::TC("testdriver", "TestDriver filter defined");
	if ($use_tempfile)
	{
	    my $filter_file = "$tempdir/filter";
	    open(F, ">$filter_file.1") or
		croak+(+__PACKAGE__,
		       "->runtest: unable to create $filter_file.1: $!\n");
	    binmode F;
	    while (<$in>)
	    {
		print F;
	    }
	    $in->close();
	    close(F);
	    $rep->winrun($in_filter, "$filter_file.1", $filter_file);
	    open($in, "<$filter_file") or
		croak +(+__PACKAGE__,
			"->runtest: unable to read from" .
			" input file $filter_file: $!\n");
	    binmode $in;
	    $in_filter = undef;
	}
    }
    if (defined $in_filter)
    {
	# Write through filter to actual file
	open($actual, "| $in_filter > $actual_file") or
	    croak +(+__PACKAGE__,
		    ": pipe to filter $in_filter failed: $!\n");
    }
    else
    {
	&QTC::TC("testdriver", "TestDriver filter not defined");
	open($actual, ">$actual_file") or
	    die +(+__PACKAGE__, ": write to $actual_file failed: $!\n");
    }
    binmode $actual;

    # Write from input to actual output, normalizing spaces and
    # newlines if needed
    my $exit_status = undef;
    while (1)
    {
	my ($line, $status) = read_line($in, $pid);
	$exit_status = $status if defined $status;
	last unless defined $line;
	if ($flags & $rep->NORMALIZE_WHITESPACE)
	{
	    &QTC::TC("testdriver", "TestDriver normalize whitespace");
	    $line =~ s/[ \t]+/ /g;
	}
	else
	{
	    &QTC::TC("testdriver", "TestDriver no normalize whitespace");
	}
	if ($flags & $rep->NORMALIZE_NEWLINES)
	{
	    &QTC::TC("testdriver", "TestDriver normalize newlines");
	    $line =~ s/\r$//;
	}
	else
	{
	    &QTC::TC("testdriver", "TestDriver no normalize newlines");
	}
	$actual->print($line);
	$actual->flush();
	last if defined $exit_status;
    }
    $in->close();
    if (defined $tempout_status)
    {
	$exit_status = $tempout_status;
    }
    if (defined $in_command)
    {
	if (! defined $exit_status)
	{
	    $exit_status = $?;
	}
	my $exit_status_number = 0;
	my $exit_status_signal = 0;
	if ($in_windows)
	{
	    # WIFSIGNALED et al are not defined.  This is emperically
	    # what happens with MSYS 1.0.11 and ActiveState Perl
	    # 5.10.1.
	    if ($exit_status & 0x8000)
	    {
		$exit_status_signal = 1;
		$exit_status = ($exit_status & 0xfff) >> 8;
		$exit_status = "SIG:$exit_status";
	    }
	    elsif ($exit_status >= 256)
	    {
		$exit_status_number = 1;
		$exit_status = $exit_status >> 8;
	    }
	}
	elsif (WIFSIGNALED($exit_status))
	{
	    $exit_status_signal = 1;
	    $exit_status = "SIG:" . WTERMSIG($exit_status);
	}
	elsif (WIFEXITED($exit_status))
	{
	    $exit_status_number = 1;
	    $exit_status = WEXITSTATUS($exit_status);
	}
	if ($exit_status_number)
	{
	    &QTC::TC("testdriver", "TestDriver exit status number");
	}
	if ($exit_status_signal)
	{
	    &QTC::TC("testdriver", "TestDriver exit status signal");
	}
    }
    $? = 0;
    $actual->close();
    $pid = undef;
    if ($?)
    {
	die +(+__PACKAGE__,
	      "->runtest: failure closing actual output; status = $?\n");
    }

    # Compare exit statuses.  This expression is always true when the
    # input was not from a command.
    if ((defined $out_exit_status) && ($out_exit_status eq '!0'))
    {
	&QTC::TC("testdriver", "TestDriver non-zero exit status");
    }
    my $status_match =
	((! defined $out_exit_status) ||
	 ((defined $exit_status) &&
	  ( (($out_exit_status eq '!0') && ($exit_status ne 0)) ||
	    ($exit_status eq $out_exit_status) )));

    # Compare actual output with expected output.
    my $expected_file = undef;
    my $output_match = undef;
    if (defined $out_string)
    {
	&QTC::TC("testdriver", "TestDriver output string");
	# Write output string to a file so we can run diff
	$expected_file = "$tempdir/expected";
	my $e = new IO::File;
	open($e, ">$expected_file") or
	    die +(__PACKAGE__,
		  "->runtest: unable to write to $expected_file: $!\n");
	binmode $e;
	$e->print($out_string);
	$e->close();
    }
    elsif (defined $out_file)
    {
	&QTC::TC("testdriver", "TestDriver output file");
	if ($flags & $rep->NORMALIZE_NEWLINES)
	{
	    # Normalize newlines in expected output file
	    $expected_file = "$tempdir/expected";
	    unlink $expected_file;
	    my $in = new IO::File;
	    if (open($in, "<$out_file"))
	    {
		binmode $in;
		my $e = new IO::File;
		open($e, ">$expected_file") or
		    die +(__PACKAGE__,
			  "->runtest: unable to write to $expected_file: $!\n");
		binmode $e;
		while (<$in>)
		{
		    s/\r?$//;
		    $e->print($_);
		}
		$e->close();
		$in->close();
	    }
	}
	else
	{
	    $expected_file = $out_file;
	}
    }
    elsif (defined $out_regexp)
    {
	&QTC::TC("testdriver", "TestDriver output regexp");
	# No expected file; do regexp test to determine whether output
	# matches
	$actual = new IO::File;
	open($actual, "<$actual_file") or
	    die +(__PACKAGE__,
		  "->runtest: unable to read $actual_file: $!\n");
	binmode $actual;
	local $/ = undef;
	my $actual_output = <$actual>;
	$actual->close();
	$output_match = ($actual_output =~ m/$out_regexp/);
    }
    else
    {
	die +__PACKAGE__, ": INTERNAL ERROR: invalid test output";
    }

    my $output_diff = undef;
    if (! defined $output_match)
    {
	if (! defined $expected_file)
	{
	    die +__PACKAGE__, ": INTERNAL ERROR: expected_file not defined";
	}
	if (defined $threads)
	{
	    # Real output comparisons are done later.
	    $output_match = 1;
	}
	else
	{
	    $output_diff = "$tempdir/difference";
	    my $r = $rep->safe_pipe(['diff', '-a', '-u',
				     $expected_file, $actual_file],
				    $output_diff);
	    $output_match = ($r == 0);
	}
    }

    my $outcome = ($output_match && $status_match) ? PASS : FAIL;
    my $exp_outcome = (($flags & $rep->EXPECT_FAILURE) ? FAIL : PASS);
    my $outcome_text = print_results($outcome, $exp_outcome);
    my $passed = $rep->update_counters($outcome, $exp_outcome);

    my $testxml = $rep->_testxml();
    my $testlog = $rep->_testlog();
    # $outcome_text is for the human-readable.  We need something
    # different for the xml file.
    $testxml->print("  <testcase\n" .
		    "   testid=\"" . xmlify($category, 1) . " $testnum\"\n" .
		    "   description=\"" . xmlify($description, 1) . "\"\n" .
		    "   outcome=\"" .
		    (($outcome eq PASS)
		     ? ($passed ? "pass" : "unexpected-pass")
		     : ($passed ? "expected-fail" : "fail")) .
		    "\"\n");

    if (($outcome eq FAIL) && ($outcome ne $exp_outcome))
    {
	# Test failed and failure was not expected

	$testxml->print("  >\n");
	$testlog->printf("$category test %d (%s) FAILED\n",
			 $testnum, $description);
	my $cwd = getcwd();
	$testlog->print("cwd: $cwd\n");
	$testxml->print("   <cwd>" . xmlify($cwd) . "</cwd>\n");
	my $cmd = $in_command;
	if ((defined $cmd) && (ref($cmd) eq 'ARRAY'))
	{
	    $cmd = join(' ', @$cmd);
	}
	if (defined $cmd)
	{
	    $testlog->print("command: $cmd\n");
	    $testxml->print("   <command>" . xmlify($cmd) . "</command>\n");
	}
	if (defined $out_file)
	{
	    # Use $out_file, not $expected_file -- we are only
	    # interested in dispaying this information if the user's
	    # real output was original in a file.
	    $testlog->print("expected output in $out_file\n");
	    $testxml->print(
		"   <expected-output-file>" . xmlify($out_file) .
		"</expected-output-file>\n");
	}

	# It would be nice if we could filter out internal calls for
	# times when runtest is called inside of the module for
	# multithreaded testing.
	$testlog->print(Carp::longmess());

	$testxml->print("   <stacktrace>test failure" .
			xmlify(Carp::longmess()) .
			"</stacktrace>\n");

	if (! $status_match)
	{
	    &QTC::TC("testdriver", "TestDriver status mismatch");
	    $testlog->printf("\tExpected status: %s\n", $out_exit_status);
	    $testlog->printf("\tActual   status: %s\n", $exit_status);
	    $testxml->print(
		"   <expected-status>$out_exit_status</expected-status>\n");
	    $testxml->print(
		"   <actual-status>$exit_status</actual-status>\n");
	}
	if (! $output_match)
	{
	    &QTC::TC("testdriver", "TestDriver output mismatch");
	    $testlog->print("--> BEGIN EXPECTED OUTPUT <--\n");
	    $testxml->print("   <expected-output>");
	    if (defined $expected_file)
	    {
		write_file_to_fh($expected_file, $testlog);
		xml_write_file_to_fh($expected_file, $testxml);
	    }
	    elsif (defined $out_regexp)
	    {
		$testlog->print("regexp: " . $out_regexp);
		if ($out_regexp !~ m/\n$/s)
		{
		    $testlog->print("\n");
		}
		$testxml->print("regexp: " . xmlify($out_regexp));
	    }
	    else
	    {
		die +(+__PACKAGE__,
		      "->runtest: INTERNAL ERROR: no expected output\n");
	    }
	    $testlog->print("--> END EXPECTED OUTPUT <--\n" .
			    "--> BEGIN ACTUAL OUTPUT <--\n");
	    $testxml->print("</expected-output>\n" .
			    "   <actual-output>");
	    write_file_to_fh($actual_file, $testlog);
	    xml_write_file_to_fh($actual_file, $testxml);
	    $testlog->print("--> END ACTUAL OUTPUT <--\n");
	    $testxml->print("</actual-output>\n");
	    if (defined $output_diff)
	    {
		&QTC::TC("testdriver", "TestDriver display diff");
		$testlog->print("--> DIFF EXPECTED ACTUAL <--\n");
		$testxml->print("   <diff-output>");
		write_file_to_fh($output_diff, $testlog);
		xml_write_file_to_fh($output_diff, $testxml);
		$testlog->print("--> END DIFFERENCES <--\n");
		$testxml->print("</diff-output>\n");
	    }
	    else
	    {
		&QTC::TC("testdriver", "TestDriver display no diff");
	    }
	}
	$testxml->print("  </testcase>\n");
    }
    else
    {
	$testxml->print("  />\n");
    }

    if (defined $threads)
    {
	if (! defined $expected_file)
	{
	    &QTC::TC("testdriver", "TestDriver thread data but no exp output");
	    croak +(+__PACKAGE__,
		    "->runtest: thread data invalid".
		    " without fixed test output\n");
	}

	my $thread_expected = "$tempdir/thread-expected";
	my $thread_actual = "$tempdir/thread-actual";
	copy($actual_file, $thread_actual);
	filter_seqgroups($expected_file, $thread_expected);

	$passed =
	    $rep->analyze_thread_data($description,
				      $expected_file, $actual_file,
				      $threads, $seqgroups)
	    && $passed;

	if ($passed)
	{
	    $rep->runtest($description . ": all subcases passed",
			  {$rep->STRING => ""},
			  {$rep->STRING => ""});
	}
	else
	{
	    $rep->runtest($description . ": original output",
			  {$rep->FILE => $thread_actual},
			  {$rep->FILE => $thread_expected});
	}

	unlink $thread_expected, $thread_actual;
    }

    $passed;
}

sub read_line
{
    my ($fh, $pid) = @_;
    my $line = undef;
    my $status = undef;

    if (defined $pid)
    {
	# It doesn't work to just call <$fh> in this case.  For some
	# unknown reason, some programs occasionally exit and cause an
	# interrupted system call return from read which perl just
	# ignores, making the call to <$fh> hang.  To protect
	# ourselves, we explicitly check for the program having exited
	# periodically if read hasn't returned anything.

	while (1)
	{
	    my $s = new IO::Select();
	    $s->add($fh);
	    my @ready = $s->can_read(1);
	    if (@ready == 0)
	    {
		if (waitpid($pid, WNOHANG) > 0)
		{
		    $status = $?;
		    last;
		}
		next;
	    }
	    else
	    {
		my $buf = "";
		my $status = sysread($fh, $buf, 1);
		if ((defined $status) && ($status == 1))
		{
		    $line = "" unless defined $line;
		    $line .= $buf;
		    last if $buf eq "\n";
		}
		else
		{
		    last;
		}
	    }
	}
    }
    else
    {
	$line = <$fh>;
    }
    ($line, $status);
}

sub write_file_to_fh
{
    my ($file, $out) = @_;
    my $in = new IO::File("<$file");
    if (defined $in)
    {
	binmode $in;
	my $ended_with_newline = 1;
	while (<$in>)
	{
	    $out->print($_);
	    $ended_with_newline = m/\n$/s;
	}
	if (! $ended_with_newline)
	{
	    $out->print("[no newline at end of data]\n");
	}
	$in->close();
    }
    else
    {
	$out->print("[unable to open $file: $!]\n");
    }
}

sub xmlify
{
    my ($str, $attr) = @_;
    $attr = 0 unless defined $attr;
    $str =~ s/\&/\&amp;/g;
    $str =~ s/</&lt;/g;
    $str =~ s/>/&gt;/g;
    $str =~ s/\"/&quot;/g if $attr;
    $str =~ s/([\000-\010\013-\037\177-\377])/sprintf("&#x%02x;", ord($1))/ge;
    $str;
}

sub xml_write_file_to_fh
{
    my ($file, $out) = @_;
    my $in = new IO::File("<$file");
    if (defined $in)
    {
	binmode $in;
	while (defined ($_ = <$in>))
	{
	    $out->print(xmlify($_));
	}
	$in->close();
    }
    else
    {
	$out->print("[unable to open $file: $!]");
    }
}

sub check_hash_keys
{
    my ($hash, @keys) = @_;
    my %actual_keys = ();
    foreach my $k (keys %$hash)
    {
	$actual_keys{$k} = 1;
    }
    foreach my $k (@keys)
    {
	delete $actual_keys{$k};
    }
    my $extra_keys = join(', ', sort (keys %actual_keys));
    ($extra_keys, (map { $hash->{$_} } @keys));
}

sub print_testid
{
    my $rep = shift;
    my ($description) = @_;

    my $testnum = $rep->_testnum();
    my $category = $rep->_suitename();
    print_and_pad(sprintf("$category %2d (%s)", $testnum, $description));
    my $tc_filename = $ENV{'TC_FILENAME'} || "";
    if ($tc_filename && open(F, ">>$tc_filename"))
    {
	binmode F;
	printf F "# $category %2d (%s)\n", $testnum, $description;
	close(F);
    }
    $rep->_testnum(++$testnum);
}

sub update_counters
{
    my $rep = shift;
    my ($outcome, $exp_outcome) = @_;

    (($outcome eq PASS) && ($exp_outcome eq PASS)) &&
	$rep->{+__PACKAGE__}{$f_passes}++;
    (($outcome eq PASS) && ($exp_outcome eq FAIL)) &&
	$rep->{+__PACKAGE__}{$f_xpasses}++;
    (($outcome eq FAIL) && ($exp_outcome eq PASS)) &&
	$rep->{+__PACKAGE__}{$f_fails}++;
    (($outcome eq FAIL) && ($exp_outcome eq FAIL)) &&
	$rep->{+__PACKAGE__}{$f_xfails}++;

    ($outcome eq PASS);
}

sub analyze_thread_data
{
    my $rep = shift;
    my ($description, $expected, $actual,
	$expected_threads, $expected_seqgroups) = @_;

    my $tempdir = $rep->_tempdir();

    my %actual_threads = ();
    my %actual_seqgroups = ();
    my @errors = ();

    $rep->thread_cleanup();
    $rep->split_combined($expected);
    $rep->analyze_threaded_output
	($actual, \%actual_threads, \%actual_seqgroups, \@errors);

    # Make sure we saw the right threads and sequences

    my $desired = "threads:\n";
    $desired .= join('', map { "  $_\n" } (sort @$expected_threads));
    $desired .= "sequence groups:\n";
    if (defined $expected_seqgroups)
    {
	$desired .= join('', map { "  $_\n" } (sort @$expected_seqgroups));
    }

    my $observed = "threads:\n";
    $observed .= join('', map { "  $_\n" } (sort keys %actual_threads));
    $observed .= "sequence groups:\n";
    $observed .= join('', map { "  $_\n" } (sort keys %actual_seqgroups));

    if (@errors)
    {
	$observed .= join('', @errors);
    }

    my $passed =
	$rep->runtest("$description: multithreaded data",
		      {$rep->STRING => $observed},
		      {$rep->STRING => $desired});


    foreach my $th (@{$expected_threads})
    {
	create_if_missing("$tempdir/$th.thread-actual",
			  "[no actual output]\n");
	filter_seqgroups("$tempdir/$th.thread-expected",
			 "$tempdir/$th.thread-filtered");
	$passed =
	    $rep->runtest($description . ": thread $th",
			  {$rep->FILE => "$tempdir/$th.thread-actual"},
			  {$rep->FILE => "$tempdir/$th.thread-filtered"})
	    && $passed;
    }
    if (defined $expected_seqgroups)
    {
	foreach my $sg (@{$expected_seqgroups})
	{
	    create_if_missing("$tempdir/$sg.seq-actual",
			      "[no actual output]\n");
	    $passed =
		$rep->runtest($description . ": seqgroup $sg",
			      {$rep->FILE => "$tempdir/$sg.seq-actual"},
			      {$rep->FILE => "$tempdir/$sg.seq-expected"})
		&& $passed;
	}
    }

    $rep->thread_cleanup();

    $passed;
}

sub analyze_threaded_output
{
    my $rep = shift;
    my ($file, $threads, $seqgroups, $errors) = @_;
    my $sequence_checking = 1;
    open(F, "<$file") or die +__PACKAGE__, ": can't open $file: $!\n";
    binmode F;
    my $cur_thread = undef;
    while (<F>)
    {
	if (m/^(\[\[(.+?)\]\]:)/)
	{
	    my $tag = $1;
	    my $thread = $2;
	    my $rest = $';	#' [unconfuse emacs font lock mode]

	    $rep->handle_line($file, $., $tag, $thread, $rest,
			      \$sequence_checking, $threads, $seqgroups,
			      $errors);

	    $cur_thread = $thread;
	}
	else
	{
	    $rep->handle_line($file, $., "", $cur_thread, $_,
			      \$sequence_checking, $threads, $seqgroups,
			      $errors);
	}
    }
    close(F);
}

sub handle_line
{
    my $rep = shift;
    my ($file, $lineno, $tag, $thread, $rest,
	$sequence_checking, $threads, $seqgroups, $errors) = @_;

    my $tempdir = $rep->_tempdir();

    if (! exists $threads->{$thread})
    {
	my $fh = new IO::File("<$tempdir/$thread.thread-expected");
	if (! $fh)
	{
	    &QTC::TC("testdriver", "TestDriver no input file for thread");
	    $fh = undef;
	    $$sequence_checking = 0;
	    push(@$errors,
		 "$file:$.: no input file for thread $thread; " .
		 "sequence checking abandoned\n");
	}
	else
	{
	    binmode $fh;
	}
	$threads->{$thread} = $fh;
    }
    my $known = defined($threads->{$thread});

    my $seqs = "";
    if ($$sequence_checking)
    {
	my $fh = $threads->{$thread};
	my $next_input_line = scalar(<$fh>);
	if (! defined $next_input_line)
	{
	    $next_input_line = "[EOF]\n";
	}
	$seqs = $rep->strip_seqs(\$next_input_line);
	if ($next_input_line eq $rest)
	{
	    if ($seqs ne "")
	    {
		$rep->handle_seqs($seqs, $tag . $rest, $seqgroups);
	    }
	}
	else
	{
	    &QTC::TC("testdriver", "TestDriver thread mismatch");
	    $$sequence_checking = 0;
	    push(@$errors,
		 "$file:$.: thread $thread mismatch; " .
		 "sequencing checking abandoned\n" .
		 "actual $rest" .
		 "expected $next_input_line");
	}
    }
    output_line("$tempdir/$thread.thread-actual", $rest);
    if (! $known)
    {
	&QTC::TC("testdriver", "TestDriver output from unknown thread");
	push(@$errors, "[[$thread]]:$rest");
    }
}

sub strip_seqs
{
    my $rep = shift;
    my $linep = shift;
    my $seqs = "";
    if ($$linep =~ s/^\(\(.*?\)\)//)
    {
	$seqs = $&;
    }
    $seqs;
}

sub handle_seqs
{
    my $rep = shift;
    my ($seqs, $line, $seqgroups) = @_;
    my $tempdir = $rep->_tempdir();
    $seqs =~ s/^\(\((.*?)\)\)/$1/;
    foreach my $seq (split(',', $seqs))
    {
	$seqgroups->{$seq} = 1;
	output_line("$tempdir/$seq.seq-actual", $line);
    }
}

sub filter_seqgroups
{
    my ($infile, $outfile) = @_;
    open(F, "<$infile") or
	die +__PACKAGE__, ": can't open $infile: $!\n";
    binmode F;
    open(O, ">$outfile") or
	die +__PACKAGE__, ": can't create $outfile: $!\n";
    binmode O;
    while (<F>)
    {
	s/^((?:\[\[.+?\]\]:)?)\(\(.+?\)\)/$1/;
	print O;
    }
    close(O);
    close(F);
}

sub output_line
{
    my ($file, $line) = @_;
    open(O, ">>$file") or die +__PACKAGE__, ": can't open $file: $!\n";
    binmode O;
    print O $line or die +__PACKAGE__, ": can't append to $file: $!\n";
    close(O) or die +__PACKAGE__, ": close $file failed: $!\n";
}

sub create_if_missing
{
    my ($file, $line) = @_;
    if (! -e $file)
    {
	open(O, ">$file") or die +__PACKAGE__, ": can't create $file: $!\n";
	binmode O;
	print O $line;
	close(O);
    }
}

sub split_combined
{
    my $rep = shift;
    my $combined = shift;
    my $tempdir = $rep->_tempdir();

    open(C, "<$combined") or die +__PACKAGE__, ": can't open $combined: $!\n";
    binmode C;
    my %files = ();
    my $last_thread_fh = undef;
    while (<C>)
    {
	my $thread_fh = $last_thread_fh;
	my $thread_out = undef;
	if (m/^(\[\[(.+?)\]\]:)(\(\((.+?)\)\))?(.*\n?)$/)
	{
	    my $thread_full = $1;
	    my $thread = $2;
	    my $seq_full = $3;
	    my $seq = $4;
	    my $rest = $5;
	    my $seq_out = undef;
	    $thread_out = $rest;

	    my @seq_files = ();
	    my $thread_file = "$tempdir/$thread.thread-expected";
	    if (defined $seq_full)
	    {
		$thread_out = $seq_full . $thread_out;
		$seq_out = $thread_full . $rest;
		foreach my $s (split(/,/, $seq))
		{
		    my $f = "$tempdir/$s.seq-expected";
		    my $fh = cache_open(\%files, $f);
		    $fh->print($seq_out);
		}
	    }

	    $thread_fh = cache_open(\%files, $thread_file);
	}
	else
	{
	    $thread_out = $_;
	}
	if ((defined $thread_out) && (! defined $thread_fh))
	{
	    die +__PACKAGE__, ": no place to put output lines\n";
	}
	$thread_fh->print($thread_out) if defined $thread_out;
	$last_thread_fh = $thread_fh;
    }
    close(C);
    map { $_->close() } (values %files);
}

sub cache_open
{
    my ($cache, $file) = @_;
    if (! defined $file)
    {
	return undef;
    }
    if (! exists $cache->{$file})
    {
	unlink $file;
	my $fh = new IO::File(">$file") or
	    die +__PACKAGE__, ": can't open $file: $!\n";
	binmode $fh;
	$cache->{$file} = $fh;
    }
    $cache->{$file};
}

sub thread_cleanup
{
    my $rep = shift;
    my $dir = $rep->_tempdir();
    my @files = +(grep { m/\.(thread|seq)-(actual|expected|filtered)$/ }
		  (glob("$dir/*")));
    if (@files)
    {
	unlink @files;
    }
}

sub rmrf
{
    my $path = shift;
    return unless -e $path;
    my $wanted = sub
    {
	if ((-d $_) && (! -l $_))
	{
	    rmdir $_ or die "rmdir $_ failed: $!\n";
	}
	else
	{
	    unlink $_ or die "unlink $_ failed: $!\n";
	}
    };
    finddepth({wanted => $wanted, no_chdir => 1}, $path);
}

sub safe_pipe
{
    my $rep = shift;
    my ($cmd, $outfile) = @_;
    my $result = 0;

    if ($in_windows)
    {
	$result = $rep->winrun($cmd, File::Spec->devnull(), $outfile);
    }
    else
    {
	my $pid = open(C, "-|");

	if ($pid)
	{
	    # parent
	    my $out = new IO::File(">$outfile") or
		die +__PACKAGE__, ": can't open $outfile: $!\n";
	    binmode C;
	    while (<C>)
	    {
		$out->print($_);
	    }
	    close(C);
	    $result = $?;
	    $out->close();
	}
	else
	{
	    # child
	    open(STDERR, ">&STDOUT");
	    exec(@$cmd) || die +__PACKAGE__, ": $cmd->[0] failed: $!\n";
	}
    }

    $result;
}

sub winrun
{
    # This function does several things to make running stuff on
    # Windows look sort of like running things on UNIX.  It assumes
    # MinGW perl is running in an MSYS/MinGW environment.
    #
    #  * When an MSYS/MinGW program is run with system("..."), its
    #    newlines generate \r\n, but when it's run from MSYS sh, its
    #    newlines generate \n.  We want \n for UNIX-like programs.
    #
    #  * system("...") in perl doesn't have any special magic to
    #    handle #! lines in scripts.  A lot of test suites will count
    #    on that.
    #
    #  * There's no Windows equivalent to execve with separate
    #    arguments, so all sorts of fancy quoting is necessary when *
    #    dealing with arguments with spaces, etc.
    #
    #  * Pipes work unreliably.  Fork emulation is very incomplete.
    #
    # To work around these issues, we ensure that everything is
    # actually executed from the MSYS /bin/sh.  We find the actual
    # path of that and then write a shell script which we explicitly
    # invoke as an argument to /bin/sh.  If we have a string that we
    # want executed with /bin/sh, we include the string in the shell
    # script.  If we have an array, we pass the array on the
    # commandline to the shell script and let it preserve spacing.  We
    # also do our output redirection in the shell script itself since
    # redirection of STDOUT and STDERR doesn't carry forward to
    # programs invoked by programs we invoke.  Finally, we filter out
    # errors generated by the script itself, since it is supposed to
    # be an invisible buffer for smoother execution of programs.
    # Experience shows that its output comes from things like printing
    # the names of signals generated by subsidiary programs.

    my $rep = shift;
    my ($in_command, $in, $out) = @_;
    my $tempdir = $rep->_tempdir();
    my $tempfilename = "$tempdir/winrun.tmp";
    if (! defined $winbin)
    {
	my $comspec = $ENV{'COMSPEC'};
	$comspec =~ s,\\,/,g;
	if ((system("sh -c 'cd /bin; $comspec /c cd'" .
		    " > $tempfilename") == 0) &&
	    open(F, "<$tempfilename"))
	{
	    $winbin = <F>;
	    close(F);
	    $winbin =~ s,[\r\n],,g;
	    $winbin =~ s,\\,/,g;
	}
	if (! defined $winbin)
	{
	    die +__PACKAGE__, ": unable to find windows path to /bin\n";
	}
    }
    my $script = "$tempdir/tmpscript";
    open(F, ">$script") or
	croak +(+__PACKAGE__,
		"->runtest: unable to open $script to write: $!\n");
    binmode F;
    print F "exec >$tempfilename\n";
    print F "exec 2>&1\n";
    print F "exec <$in\n";
    my @cmd = ("$winbin/sh", $script);
    if (ref($in_command) eq 'ARRAY')
    {
	# For debugging, write out the args
	foreach my $arg (@$in_command)
	{
	    print F "# $arg\n";
	}
	print F '"$@"', "\n";
	push(@cmd, @$in_command);
    }
    else
    {
	print F "$in_command\n";
    }
    close(F);
    my $status = system @cmd;
    if (open(IN, "<$tempfilename") &&
	open(OUT, ">$out"))
    {
	binmode IN;
	binmode OUT;
	while (<IN>)
	{
	    next if m/^$script:/;
	    print OUT;
	}
	close(IN);
	close(OUT);
    }
    $status;
}

1;

#
# END OF TestDriver
#
