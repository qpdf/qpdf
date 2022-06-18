/*
 * This is an example program to linearize a PDF file using the C
 * QPDFJob API.
 */

#include <qpdf/qpdfjob-c.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char const* whoami = 0;

static void
usage()
{
    fprintf(stderr, "Usage: %s infile outfile\n", whoami);
    exit(2);
}

int
main(int argc, char* argv[])
{
    char* infile = NULL;
    char* outfile = NULL;
    char const* new_argv[6];
    int r = 0;
    char* p = 0;

    if ((p = strrchr(argv[0], '/')) != NULL) {
        whoami = p + 1;
    } else if ((p = strrchr(argv[0], '\\')) != NULL) {
        whoami = p + 1;
    } else {
        whoami = argv[0];
    }

    if (argc != 3) {
        usage();
    }

    infile = argv[1];
    outfile = argv[2];

    new_argv[0] = "qpdfjob";
    new_argv[1] = infile;
    new_argv[2] = outfile;
    new_argv[3] = "--linearize";
    new_argv[4] = "--static-id"; /* for testing only */
    new_argv[5] = NULL;

    /* See qpdf-job.cc for a C++ example of using the json interface.
     * To use that from C just like the argv one, call
     * qpdfjob_run_from_json instead and pass the json string as a
     * single char const* argument.
     *
     * See qpdfjob-c-save-attachment.c for an example of using the
     * full form of the qpdfjob interface with init and cleanup
     * functions.
     */
    r = qpdfjob_run_from_argv(new_argv);
    return r;
}
