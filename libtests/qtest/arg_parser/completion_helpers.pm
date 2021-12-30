# This is also used by qpdf.test.

sub bash_completion
{
    my ($x, $line, $point) = @_;
    if (! defined $point)
    {
        $point = length($line);
    }
    my $before_point = substr($line, 0, $point);
    my $first = '';
    my $sep = '';
    my $cur = '';
    if ($before_point =~ m/^(.*)([ =])([^= ]*)$/)
    {
        ($first, $sep, $cur) = ($1, $2, $3);
    }
    my $prev = ($sep eq '=' ? $sep : $first);
    $prev =~ s/.* (\S+)$/$1/;
    my $this = $first;
    $this =~ s/(\S+)\s.*/$1/;
    ['env', "COMP_LINE=$line", "COMP_POINT=$point",
     $x, $this, $cur, $prev];
}

sub zsh_completion
{
    my ($x, $line, $point) = @_;
    if (! defined $point)
    {
        $point = length($line);
    }
    ['env', "COMP_LINE=$line", "COMP_POINT=$point", $x];
}

1;
