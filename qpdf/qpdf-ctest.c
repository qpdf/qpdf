#include <qpdf/qpdf-c.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static qpdf_data qpdf = 0;

static void report_errors()
{
    while (qpdf_more_warnings(qpdf))
    {
	printf("warning: %s\n", qpdf_next_warning(qpdf));
    }
    while (qpdf_more_errors(qpdf))
    {
	printf("error: %s\n", qpdf_next_error(qpdf));
    }
}

static void test01(char const* infile,
		   char const* password,
		   char const* outfile)
{
    qpdf_read(qpdf, infile, password);
    printf("version: %s\n", qpdf_get_pdf_version(qpdf));
    printf("linearized: %d\n", qpdf_is_linearized(qpdf));
    printf("encrypted: %d\n", qpdf_is_encrypted(qpdf));
    if (qpdf_is_encrypted(qpdf))
    {
	printf("user password: %s\n", qpdf_get_user_password(qpdf));
    }
    report_errors();
}

int main(int argc, char* argv[])
{
    char* whoami = 0;
    char* p = 0;
    int n = 0;
    char const* infile;
    char const* password;
    char const* outfile;
    void (*fn)(char const*, char const*, char const*) = 0;

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
    if (argc != 5)
    {
	fprintf(stderr, "usage: %s n infile password outfile\n", whoami);
	exit(2);
    }

    n = atoi(argv[1]);
    infile = argv[2];
    password = argv[3];
    outfile = argv[4];

    fn = ((n == 1) ? test01 :
	  0);

    if (fn == 0)
    {
	fprintf(stderr, "%s: invalid test number %d\n", whoami, n);
	exit(2);
    }

    qpdf = qpdf_init();
    fn(infile, password, outfile);
    qpdf_cleanup(&qpdf);
    assert(qpdf == 0);

    return 0;
}
