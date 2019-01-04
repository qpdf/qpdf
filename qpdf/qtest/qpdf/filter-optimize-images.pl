use strict;
use warnings;

while (<>)
{
    s/(reduces size from \d+ to )\d+/$1.../;
    print;
}
