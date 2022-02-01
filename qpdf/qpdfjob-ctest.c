#include <qpdf/qpdfjob-c.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifndef QPDF_NO_WCHAR_T
static void wide_test()
{
    wchar_t* argv[5];
    argv[0] = (wchar_t*)(L"qpdfjob");
    argv[1] = (wchar_t*)(L"minimal.pdf");
    argv[2] = (wchar_t*)(L"a.pdf");
    argv[3] = (wchar_t*)(L"--static-id");
    argv[4] = NULL;
    assert(qpdfjob_run_from_wide_argv(4, argv) == 0);
    printf("wide test passed\n");
}
#endif // QPDF_NO_WCHAR_T

static void run_tests()
{
    /* Be sure to use a different output file for each test. */

    char* argv[5];
    argv[0] = (char*)("qpdfjob");
    argv[1] = (char*)("minimal.pdf");
    argv[2] = (char*)("a.pdf");
    argv[3] = (char*)("--deterministic-id");
    argv[4] = NULL;
    assert(qpdfjob_run_from_argv(4, argv) == 0);
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

int main(int argc, char* argv[])
{
    if ((argc == 2) && (strcmp(argv[1], "wide") == 0))
    {
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
