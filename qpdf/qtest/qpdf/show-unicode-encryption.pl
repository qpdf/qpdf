use warnings;
use strict;

while (<>)
{
    print if m/invalid password/;
    print "trying other\n" if m/supplied password didn't work/;
    print if m/^R =/;
    print if m/^User password =/;
}
