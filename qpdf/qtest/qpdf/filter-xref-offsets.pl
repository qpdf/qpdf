use warnings;
use strict;

while (<>)
{
    s/(uncompressed; offset =) \d+/$1 .../;
    print;
}
