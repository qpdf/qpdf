/*
 * This is an example of how to write C++ functions and make them usable with the qpdf C API. It
 * consists of three files:
 * - extend-c-api.h -- a plain C header file
 * - extend-c-api.c -- a C program that calls the function
 * - extend-c-api.cc -- a C++ file that implements the function
 */

#include "extend-c-api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char const* whoami = 0;

static void
usage()
{
    fprintf(stderr, "Usage: %s infile\n", whoami);
    exit(2);
}

int
main(int argc, char* argv[])
{
    char* infile = NULL;
    qpdf_data qpdf = qpdf_init();
    int warnings = 0;
    int errors = 0;
    char* p = NULL;

    if ((p = strrchr(argv[0], '/')) != NULL) {
        whoami = p + 1;
    } else if ((p = strrchr(argv[0], '\\')) != NULL) {
        whoami = p + 1;
    } else {
        whoami = argv[0];
    }

    if (argc != 2) {
        usage();
    }

    infile = argv[1];

    if ((qpdf_read(qpdf, infile, NULL) & QPDF_ERRORS) == 0) {
        int npages;
        if ((num_pages(qpdf, &npages) & QPDF_ERRORS) == 0) {
            printf("num pages = %d\n", npages);
        }
    }
    if (qpdf_more_warnings(qpdf)) {
        warnings = 1;
    }
    if (qpdf_has_error(qpdf)) {
        errors = 1;
        printf("error: %s\n", qpdf_get_error_full_text(qpdf, qpdf_get_error(qpdf)));
    }
    qpdf_cleanup(&qpdf);
    if (errors) {
        return 2;
    } else if (warnings) {
        return 3;
    }

    return 0;
}
