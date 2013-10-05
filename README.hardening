Avoiding operator[]
===================

During a security review by Red Hat security team (specifically
Florian Weimer), it was discovered that qpdf used std::string and
std::vector's operator[], which has no bounds checking by design.
Instead, using those objects' at() method is preferable since it does
bounds checking.  Florian has a tool that can detect all uses of these
methods and report them.  I have a short perl script that
automatically corrects any such uses.  The perl script is not intended
to be general, but it could be reasonably general.  The only known
shortcut is that it might not work very well with some cases of nested
[]'s like a[b[c]] or with cases where there are line breaks inside the
brackets.  For qpdf's coding style, it worked on all cases reported.

To use this, obtain htcondor-analyzer, run it, and respond to the
report.  Here's what I did.

sudo aptitude install libclang-dev llvm llvm-dev clang
cd /tmp
git clone https://github.com/fweimer/htcondor-analyzer
# HEAD = 5fa06fc68a9b0677e9de162279185d58ba1e8477 at this writing
cd htcondor-analyzer
make

in qpdf

./autogen.sh
/tmp/htcondor-analyzer/create-db
CC=/tmp/htcondor-analyzer/cc CXX=/tmp/htcondor-analyzer/cxx ./configure --disable-shared --disable-werror
# to remove conftest.c
\rm htcondor-analyzer.sqlite
/tmp/htcondor-analyzer/create-db

Repeat until no more errors:

/tmp/fix-at.pl is shown below.

make
/tmp/htcondor-analyzer/report | grep std:: | grep qpdf >| /tmp/r
perl /tmp/fix-at.pl /tmp/r
# move all *.new over the original file.  patmv is my script.  Can
# also use a for loop.
patmv -f s/.new// **/*.new

---------- /tmp/fix-at.pl ----------
#!/usr/bin/env perl
require 5.008;
use warnings;
use strict;
use File::Basename;

my $whoami = basename($0);

my %to_fix = ();
while (<>)
{
    chomp;
    die unless m/^([^:]+):(\d+):(\d+):\s*(.*)$/;
    my ($file, $line, $col, $message) = ($1, $2, $3, $4);
    if ($message !~ m/operator\[\]/)
    {
        warn "skipping $_\n";
        next;
    }
    push(@{$to_fix{$file}}, [$line, $col, $message]);
}
foreach my $file (sort keys %to_fix)
{
    open(F, "<$file") or die;
    my @lines = (<F>);
    close(F);
    my $last = "";
    my @data = reverse sort { ($a->[0] <=> $b->[0]) || ($a->[1] <=> $b->[1]) } @{$to_fix{$file}};
    foreach my $d (@data)
    {
        my ($line, $col) = @$d;
        next if $last eq "$line:$col";
        $last = "$line:$col";
        die if $line-- < 1;
        die if $col-- < 1;
        print $lines[$line];
        $lines[$line] =~ s/^(.{$col})([^\[]+)\[([^\]]+)\]/$1$2.at($3)/ or die "$file:$last\n";
        print $lines[$line];
    }
    open(F, ">$file.new") or die;
    foreach my $line (@lines)
    {
        print F $line;
    }
    close(F) or die;
}
--------------------
