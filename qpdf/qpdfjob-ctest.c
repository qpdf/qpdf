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

    assert(qpdfjob_run_from_json("{\n\
  \"inputFile\": \"xref-with-short-size.pdf\",\n\
  \"outputFile\": \"c.pdf\",\n\
  \"staticId\": \"\",\n\
  \"decrypt\": \"\",\n\
  \"objectStreams\": \"generate\"\n\
}") == 3);
    printf("json warn test passed\n");

    assert(qpdfjob_run_from_json("{\n\
  \"inputFile\": \"nothing-there.pdf\"\n\
}") == 2);
    printf("json error test passed\n");
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
