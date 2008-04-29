# -*- perl -*-

require 5.005;
use strict;
use FileHandle;

package QTC;

sub TC
{
    my ($scope, $case, $n) = @_;
    local $!;
    $n = 0 unless defined $n;
    return unless ($scope eq ($ENV{'TC_SCOPE'} || ""));
    my $filename = $ENV{'TC_FILENAME'} || return;
    my $fh = new FileHandle(">>$filename") or
	die "open test coverage file: $!\n";
    print $fh "$case $n\n";
    $fh->close();
}

1;

#
# END OF QTC
#
