use warnings;
use strict;

while (<>)
{
    s/D:\d{14}\S+/<date>/;
    print;
}
