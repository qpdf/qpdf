/*
 * This is an example program to linearize a PDF file using the C API.
 */

#include <qpdf/qpdf-c.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char const* whoami = 0;

static void usage()
{
    fprintf(stderr, "Usage: %s infile infile-password outfile\n", whoami);
    exit(2);
}

int main(int argc, char* argv[])
{
    char* infile = argv[1];
    char* password = argv[2];
    char* outfile = argv[3];
    qpdf_data qpdf = qpdf_init();
    int warnings = 0;
    int errors = 0;
    char* p = 0;

    if ((p = strrchr(argv[0], '/')) != NULL)
    {
	whoami = p + 1;
    }
    else if ((p = strrchr(argv[0], '\\')) != NULL)
    {
	whoami = p + 1;
    }
    else
    {
	whoami = argv[0];
    }

    if (argc != 4)
    {
	usage();
    }

    if (((qpdf_read(qpdf, infile, password) & QPDF_ERRORS) == 0) &&
	((qpdf_init_write(qpdf, outfile) & QPDF_ERRORS) == 0))
    {
	qpdf_set_static_ID(qpdf, QPDF_TRUE);
	qpdf_set_linearization(qpdf, QPDF_TRUE);
	qpdf_write(qpdf);
    }
    while (qpdf_more_warnings(qpdf))
    {
	warnings = 1;
	printf("warning: %s\n", qpdf_next_warning(qpdf));
    }
    while (qpdf_more_errors(qpdf))
    {
	errors = 1;
	printf("error: %s\n", qpdf_next_error(qpdf));
    }
    qpdf_cleanup(&qpdf);
    if (errors)
    {
	return 2;
    }
    else if (warnings)
    {
	return 3;
    }

    return 0;
}
