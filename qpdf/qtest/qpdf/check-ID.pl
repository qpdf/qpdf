use strict;
$^W=1;

my $okay = 0;
my $id = '31415926535897932384626433832795';
while (<>)
{
    if ((m,/ID ?\[<([[:xdigit:]]{32})><$id>\],) && ($1 ne $id))
    {
	$okay = 1;
    }
}
if ($okay)
{
    print "ID okay\n";
}
else
{
    print "ID bad\n";
}
