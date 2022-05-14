use File::Spec;

my $devNull = File::Spec->devnull();

my $compare_images = 0;
if ((exists $ENV{'QPDF_TEST_COMPARE_IMAGES'}) &&
    ($ENV{'QPDF_TEST_COMPARE_IMAGES'} eq '1'))
{
    $compare_images = 1;
}

sub calc_ntests
{
    my ($n_tests, $n_compare_pdfs) = @_;
    my $result = $n_tests;
    if ($compare_images)
    {
        $result += 3 * ($n_compare_pdfs);
    }
    $result;
}

sub check_pdf
{
    my ($td, $description, $command, $output, $status) = @_;
    unlink "a.pdf";
    $td->runtest($description,
                 {$td->COMMAND => "$command a.pdf"},
                 {$td->STRING => "",
                  $td->EXIT_STATUS => $status});
    $td->runtest("check output",
                 {$td->FILE => "a.pdf"},
                 {$td->FILE => $output});
}

sub flush_tiff_cache
{
    system("rm -rf tiff-cache");
}

sub compare_pdfs
{
    return unless $compare_images;

    # Each call to compare_pdfs generates three tests. This is known
    # in calc_ntests.
    my ($td, $f1, $f2, $exp) = @_;

    $exp = 0 unless defined $exp;

    system("rm -rf tif1 tif2");

    mkdir "tiff-cache", 0777 unless -d "tiff-cache";

    my $md5_1 = get_md5_checksum($f1);
    my $md5_2 = get_md5_checksum($f2);

    mkdir "tif1", 0777 or die;
    mkdir "tif2", 0777 or die;

    if (-f "tiff-cache/$md5_1.tif")
    {
        $td->runtest("get cached original file image",
                     {$td->COMMAND => "cp tiff-cache/$md5_1.tif tif1/a.tif"},
                     {$td->STRING => "",
                      $td->EXIT_STATUS => 0});
    }
    else
    {
        # We discard gs's stderr since it has sometimes been known to
        # complain about files that are not bad.  In particular, gs
        # 9.04 can't handle empty xref sections such as those found in
        # the hybrid xref cases.  We don't really care whether gs
        # complains or not as long as it creates correct images.  If
        # it doesn't create correct images, the test will fail, and we
        # can run manually to see the error message.  If it does, then
        # we don't care about the warning.
        $td->runtest("convert original file to image",
                     {$td->COMMAND =>
                          "(cd tif1;" .
                          " gs 2>$devNull -q -dNOPAUSE -sDEVICE=tiff24nc" .
                          " -sOutputFile=a.tif - < ../$f1)"},
                     {$td->STRING => "",
                      $td->EXIT_STATUS => 0});
        copy("tif1/a.tif", "tiff-cache/$md5_1.tif");
    }

    if (-f "tiff-cache/$md5_2.tif")
    {
        $td->runtest("get cached new file image",
                     {$td->COMMAND => "cp tiff-cache/$md5_2.tif tif2/a.tif"},
                     {$td->STRING => "",
                      $td->EXIT_STATUS => 0});
    }
    else
    {
        $td->runtest("convert new file to image",
                     {$td->COMMAND =>
                          "(cd tif2;" .
                          " gs 2>$devNull -q -dNOPAUSE -sDEVICE=tiff24nc" .
                          " -sOutputFile=a.tif - < ../$f2)"},
                     {$td->STRING => "",
                      $td->EXIT_STATUS => 0});
        copy("tif2/a.tif", "tiff-cache/$md5_2.tif");
    }

    $td->runtest("compare images",
                 {$td->COMMAND => "tiffcmp -t tif1/a.tif tif2/a.tif"},
                 {$td->REGEXP => ".*",
                  $td->EXIT_STATUS => $exp});

    system("rm -rf tif1 tif2");
}

sub check_metadata
{
    my ($td, $file, $exp_encrypted, $exp_cleartext) = @_;
    my $out = "encrypted=$exp_encrypted; cleartext=$exp_cleartext\n" .
        "test 6 done\n";
    $td->runtest("check metadata: $file",
                 {$td->COMMAND => "test_driver 6 $file"},
                 {$td->STRING => $out, $td->EXIT_STATUS => 0},
                 $td->NORMALIZE_NEWLINES);
}

sub get_md5_checksum
{
    my $file = shift;
    open(F, "<$file") or fatal("can't open $file: $!");
    binmode F;
    my $digest = Digest::MD5->new->addfile(*F)->hexdigest;
    close(F);
    $digest;
}

sub cleanup
{
    system("rm -rf a.json *.ps *.pnm ?.pdf ?.qdf *.enc* tif1 tif2 tiff-cache");
    system("rm -rf *split-out* ???-kfo.pdf *.tmpout \@file.pdf auto-*");
}

1;
