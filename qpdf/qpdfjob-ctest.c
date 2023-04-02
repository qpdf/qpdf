#include <qpdf/assert_test.h>

#include <qpdf/qpdfjob-c.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef QPDF_NO_WCHAR_T
static void
wide_test()
{
    wchar_t const* argv[5];
    argv[0] = L"qpdfjob";
    argv[1] = L"minimal.pdf";
    argv[2] = L"a.pdf";
    argv[3] = L"--static-id";
    argv[4] = NULL;
    assert(qpdfjob_run_from_wide_argv(argv) == 0);
    printf("wide test passed\n");
}
#endif // QPDF_NO_WCHAR_T

static void
custom_progress(int progress, void* data)
{
    printf("%s: write progress: %d%%\n", (char const*)data, progress);
}

static int
custom_log(char const* data, size_t size, void* udata)
{
    fprintf(stderr, "|custom|");
    fwrite(data, 1, size, stderr);
    fflush(stderr);
    return 0;
}

static void
run_tests()
{
    /* Be sure to use a different output file for each test. */
    qpdfjob_handle j = NULL;

    char const* argv[6];
    argv[0] = "qpdfjob";
    argv[1] = "minimal.pdf";
    argv[2] = "a.pdf";
    argv[3] = "--deterministic-id";
    argv[4] = "--progress";
    argv[5] = NULL;
    j = qpdfjob_init();
    qpdfjob_register_progress_reporter(j, custom_progress, (void*)"potato");
    assert(qpdfjob_initialize_from_argv(j, argv) == 0);
    assert(qpdfjob_run(j) == 0);
    qpdfjob_cleanup(&j);
    printf("argv test passed\n");

    assert(qpdfjob_run_from_json("{\n\
  \"inputFile\": \"20-pages.pdf\",\n\
  \"password\": \"user\",\n\
  \"outputFile\": \"b.pdf\",\n\
  \"staticId\": \"\",\n\
  \"decrypt\": \"\",\n\
  \"objectStreams\": \"generate\"\n\
}") == 0);
    printf("json test passed\n");
    fflush(stdout);

    assert(qpdfjob_run_from_json("{\n\
  \"inputFile\": \"xref-with-short-size.pdf\",\n\
  \"outputFile\": \"c.pdf\",\n\
  \"staticId\": \"\",\n\
  \"decrypt\": \"\",\n\
  \"objectStreams\": \"generate\"\n\
}") == 3);
    printf("json warn test passed\n");
    fflush(stdout);

    /* Also exercise custom logger */
    j = qpdfjob_init();
    qpdflogger_handle l1 = qpdfjob_get_logger(j);
    qpdflogger_handle l2 = qpdflogger_default_logger();
    assert(qpdflogger_equal(l1, l2));
    qpdflogger_cleanup(&l1);
    qpdflogger_cleanup(&l2);
    qpdflogger_handle l = qpdflogger_create();
    qpdflogger_set_error(l, qpdf_log_dest_custom, custom_log, NULL);
    qpdfjob_set_logger(j, l);
    qpdflogger_handle l3 = qpdfjob_get_logger(j);
    assert(qpdflogger_equal(l, l3));
    qpdflogger_cleanup(&l);
    qpdflogger_cleanup(&l3);

    qpdfjob_initialize_from_json(j, "{\n\
  \"inputFile\": \"nothing-there.pdf\"\n\
}");
    assert(qpdfjob_run(j) == 2);
    qpdfjob_cleanup(&j);
    printf("json error test passed\n");

    /* qpdfjob_create_qpdf and qpdfjob_write_qpdf test */
    argv[0] = "qpdfjob";
    argv[1] = "minimal.pdf";
    argv[2] = "d.pdf";
    argv[3] = "--deterministic-id";
    argv[4] = "--progress";
    argv[5] = NULL;
    j = qpdfjob_init();
    assert(qpdfjob_initialize_from_argv(j, argv) == 0);
    qpdf_data qpdf = qpdfjob_create_qpdf(j);
    assert(qpdfjob_write_qpdf(j, qpdf) == 0);
    qpdf_cleanup(&qpdf);
    qpdfjob_cleanup(&j);

    /* Try to open a missing file to test case of QPDFJob::createQPDF returning
     * nullptr.
     */
    argv[0] = "qpdfjob";
    argv[1] = "m.pdf";
    argv[2] = "--check";
    argv[3] = NULL;
    j = qpdfjob_init();
    assert(qpdfjob_initialize_from_argv(j, argv) == 0);
    assert(qpdfjob_create_qpdf(j) == NULL);
    qpdfjob_cleanup(&j);
    printf("qpdfjob_create_qpdf and qpdfjob_write_qpdf test passed\n");
}

int
main(int argc, char* argv[])
{
    if ((argc == 2) && (strcmp(argv[1], "wide") == 0)) {
#ifndef QPDF_NO_WCHAR_T
        wide_test();
#else
        printf("skipped wide\n");
#endif // QPDF_NO_WCHAR_T
        return 0;
    }

    run_tests();
    return 0;
}
