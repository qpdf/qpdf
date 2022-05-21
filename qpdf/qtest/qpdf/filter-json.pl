use warnings;
use strict;

while (<>)
{
    s/("datafile": ").*?(a.json-.*",)/$1$2/;
    s%("/(?:Width|Height)": )\d+(.*)%${1}50${2}%;
    print;
}
