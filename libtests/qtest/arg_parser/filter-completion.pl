use warnings;
use strict;

# THIS SCRIPT IS ALSO USED IN qpdf.test

# Output every line from STDIN that appears in the file.
my %wanted = ();
my %notwanted = ();
my $f = $ARGV[0];
if (open(F, "<$f"))
{
    while (<F>)
    {
        chomp;
        if (s/^!//)
        {
            $notwanted{$_} = 1;
        }
        else
        {
            $wanted{$_} = 1;
        }
    }
    close(F);
}
while (<STDIN>)
{
    chomp;
    if (exists $wanted{$_})
    {
        print $_, "\n";
    }
    elsif (exists $notwanted{$_})
    {
        delete $notwanted{$_};
    }
}
foreach my $k (sort keys %notwanted)
{
    print "!$k\n";
}
