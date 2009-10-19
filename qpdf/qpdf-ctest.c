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
		   char const* outfile,
		   char const* outfile2)
{
    qpdf_read(qpdf, infile, password);
    printf("version: %s\n", qpdf_get_pdf_version(qpdf));
    printf("linearized: %d\n", qpdf_is_linearized(qpdf));
    printf("encrypted: %d\n", qpdf_is_encrypted(qpdf));
    if (qpdf_is_encrypted(qpdf))
    {
	printf("user password: %s\n", qpdf_get_user_password(qpdf));
	printf("extract for accessibility: %d\n",
	       qpdf_allow_accessibility(qpdf));
	printf("extract for any purpose: %d\n",
	       qpdf_allow_extract_all(qpdf));
	printf("print low resolution: %d\n",
	       qpdf_allow_print_low_res(qpdf));
	printf("print high resolution: %d\n",
	       qpdf_allow_print_high_res(qpdf));
	printf("modify document assembly: %d\n",
	       qpdf_allow_modify_assembly(qpdf));
	printf("modify forms: %d\n",
	       qpdf_allow_modify_form(qpdf));
	printf("modify annotations: %d\n",
	       qpdf_allow_modify_annotation(qpdf));
	printf("modify other: %d\n",
	       qpdf_allow_modify_other(qpdf));
	printf("modify anything: %d\n",
	       qpdf_allow_modify_all(qpdf));
    }
    report_errors();
}

static void test02(char const* infile,
		   char const* password,
		   char const* outfile,
		   char const* outfile2)
{
    qpdf_set_suppress_warnings(qpdf, QPDF_TRUE);
    if (((qpdf_read(qpdf, infile, password) & QPDF_ERRORS) == 0) &&
	((qpdf_init_write(qpdf, outfile) & QPDF_ERRORS) == 0))
    {
	qpdf_set_static_ID(qpdf, QPDF_TRUE);
	qpdf_write(qpdf);
    }
    report_errors();
}

static void test03(char const* infile,
		   char const* password,
		   char const* outfile,
		   char const* outfile2)
{
    qpdf_read(qpdf, infile, password);
    qpdf_init_write(qpdf, outfile);
    qpdf_set_static_ID(qpdf, QPDF_TRUE);
    qpdf_set_content_normalization(qpdf, QPDF_TRUE);
    qpdf_write(qpdf);
    report_errors();
}

static void test04(char const* infile,
		   char const* password,
		   char const* outfile,
		   char const* outfile2)
{
    qpdf_set_ignore_xref_streams(qpdf, QPDF_TRUE);
    qpdf_read(qpdf, infile, password);
    qpdf_init_write(qpdf, outfile);
    qpdf_set_static_ID(qpdf, QPDF_TRUE);
    qpdf_write(qpdf);
    report_errors();
}

static void test05(char const* infile,
		   char const* password,
		   char const* outfile,
		   char const* outfile2)
{
    qpdf_read(qpdf, infile, password);
    qpdf_init_write(qpdf, outfile);
    qpdf_set_static_ID(qpdf, QPDF_TRUE);
    qpdf_set_linearization(qpdf, QPDF_TRUE);
    qpdf_write(qpdf);
    report_errors();
}

static void test06(char const* infile,
		   char const* password,
		   char const* outfile,
		   char const* outfile2)
{
    qpdf_read(qpdf, infile, password);
    qpdf_init_write(qpdf, outfile);
    qpdf_set_static_ID(qpdf, QPDF_TRUE);
    qpdf_set_object_stream_mode(qpdf, qpdf_o_generate);
    qpdf_write(qpdf);
    report_errors();
}

static void test07(char const* infile,
		   char const* password,
		   char const* outfile,
		   char const* outfile2)
{
    qpdf_read(qpdf, infile, password);
    qpdf_init_write(qpdf, outfile);
    qpdf_set_static_ID(qpdf, QPDF_TRUE);
    qpdf_set_qdf_mode(qpdf, QPDF_TRUE);
    qpdf_write(qpdf);
    report_errors();
}

static void test08(char const* infile,
		   char const* password,
		   char const* outfile,
		   char const* outfile2)
{
    qpdf_read(qpdf, infile, password);
    qpdf_init_write(qpdf, outfile);
    qpdf_set_static_ID(qpdf, QPDF_TRUE);
    qpdf_set_qdf_mode(qpdf, QPDF_TRUE);
    qpdf_set_suppress_original_object_IDs(qpdf, QPDF_TRUE);
    qpdf_write(qpdf);
    report_errors();
}

static void test09(char const* infile,
		   char const* password,
		   char const* outfile,
		   char const* outfile2)
{
    qpdf_read(qpdf, infile, password);
    qpdf_init_write(qpdf, outfile);
    qpdf_set_static_ID(qpdf, QPDF_TRUE);
    qpdf_set_stream_data_mode(qpdf, qpdf_s_uncompress);
    qpdf_write(qpdf);
    report_errors();
}

static void test10(char const* infile,
		   char const* password,
		   char const* outfile,
		   char const* outfile2)
{
    qpdf_set_attempt_recovery(qpdf, QPDF_FALSE);
    qpdf_read(qpdf, infile, password);
    report_errors();
}

static void test11(char const* infile,
		   char const* password,
		   char const* outfile,
		   char const* outfile2)
{
    qpdf_read(qpdf, infile, password);
    qpdf_init_write(qpdf, outfile);
    qpdf_set_static_ID(qpdf, QPDF_TRUE);
    qpdf_set_r2_encryption_parameters(
	qpdf, "user1", "owner1", QPDF_FALSE, QPDF_TRUE, QPDF_TRUE, QPDF_TRUE);
    qpdf_write(qpdf);
    report_errors();
}

static void test12(char const* infile,
		   char const* password,
		   char const* outfile,
		   char const* outfile2)
{
    qpdf_read(qpdf, infile, password);
    qpdf_init_write(qpdf, outfile);
    qpdf_set_static_ID(qpdf, QPDF_TRUE);
    qpdf_set_r3_encryption_parameters(
	qpdf, "user2", "owner2", QPDF_TRUE, QPDF_TRUE,
	qpdf_r3p_low, qpdf_r3m_all);
    qpdf_write(qpdf);
    report_errors();
}

static void test13(char const* infile,
		   char const* password,
		   char const* outfile,
		   char const* outfile2)
{
    qpdf_read(qpdf, infile, password);
    printf("user password: %s\n", qpdf_get_user_password(qpdf));
    qpdf_init_write(qpdf, outfile);
    qpdf_set_static_ID(qpdf, QPDF_TRUE);
    qpdf_set_preserve_encryption(qpdf, QPDF_FALSE);
    qpdf_write(qpdf);
    report_errors();
}

static void test14(char const* infile,
		   char const* password,
		   char const* outfile,
		   char const* outfile2)
{
    qpdf_read(qpdf, infile, password);
    qpdf_init_write(qpdf, outfile);
    qpdf_set_static_ID(qpdf, QPDF_TRUE);
    qpdf_set_minimum_pdf_version(qpdf, "1.6");
    qpdf_write(qpdf);
    qpdf_init_write(qpdf, outfile2);
    qpdf_set_static_ID(qpdf, QPDF_TRUE);
    qpdf_force_pdf_version(qpdf, "1.4");
    qpdf_write(qpdf);
    report_errors();
}

static void test15(char const* infile,
		   char const* password,
		   char const* outfile,
		   char const* outfile2)
{
    qpdf_read(qpdf, infile, password);
    qpdf_init_write(qpdf, outfile);
    qpdf_set_static_ID(qpdf, QPDF_TRUE);
    qpdf_set_static_aes_IV(qpdf, QPDF_TRUE);
    qpdf_set_r4_encryption_parameters(
	qpdf, "user2", "owner2", QPDF_TRUE, QPDF_TRUE,
	qpdf_r3p_low, qpdf_r3m_all, QPDF_TRUE, QPDF_TRUE);
    qpdf_write(qpdf);
    report_errors();
}

int main(int argc, char* argv[])
{
    char* whoami = 0;
    char* p = 0;
    int n = 0;
    char const* infile = 0;
    char const* password = 0;
    char const* outfile = 0;
    char const* outfile2 = 0;
    void (*fn)(char const*, char const*, char const*, char const*) = 0;

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
    if (argc < 5)
    {
	fprintf(stderr, "usage: %s n infile password outfile\n", whoami);
	exit(2);
    }

    n = atoi(argv[1]);
    infile = argv[2];
    password = argv[3];
    outfile = argv[4];
    outfile2 = (argc > 5 ? argv[5] : 0);

    fn = ((n == 1) ? test01 :
	  (n == 2) ? test02 :
	  (n == 3) ? test03 :
	  (n == 4) ? test04 :
	  (n == 5) ? test05 :
	  (n == 6) ? test06 :
	  (n == 7) ? test07 :
	  (n == 8) ? test08 :
	  (n == 9) ? test09 :
	  (n == 10) ? test10 :
	  (n == 11) ? test11 :
	  (n == 12) ? test12 :
	  (n == 13) ? test13 :
	  (n == 14) ? test14 :
	  (n == 15) ? test15 :
	  0);

    if (fn == 0)
    {
	fprintf(stderr, "%s: invalid test number %d\n", whoami, n);
	exit(2);
    }

    qpdf = qpdf_init();
    fn(infile, password, outfile, outfile2);
    qpdf_cleanup(&qpdf);
    assert(qpdf == 0);

    return 0;
}
