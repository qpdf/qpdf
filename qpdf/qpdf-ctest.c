#include <qpdf/qpdf-c.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static char* whoami = 0;
static qpdf_data qpdf = 0;

static FILE* safe_fopen(char const* filename, char const* mode)
{
    // This function is basically a "C" port of QUtil::safe_fopen.
    FILE* f = 0;
#ifdef _MSC_VER
    errno_t err = fopen_s(&f, filename, mode);
    if (err != 0)
    {
        char buf[94];
        strerror_s(buf, sizeof(buf), errno);
	fprintf(stderr, "%s: unable to open %s: %s\n",
		whoami, filename, buf);
	exit(2);
    }
#else
    f = fopen(filename, mode);
    if (f == NULL)
    {
	fprintf(stderr, "%s: unable to open %s: %s\n",
		whoami, filename, strerror(errno));
	exit(2);
    }
#endif
    return f;
}

static void report_errors()
{
#ifdef _WIN32
# define POS_FMT "  pos : %I64d\n"
#else
/* If your compiler doesn't support lld, change to ld and lose
   precision on offsets in error messages. */
# define POS_FMT "  pos : %lld\n"
#endif
    qpdf_error e = 0;
    while (qpdf_more_warnings(qpdf))
    {
	e = qpdf_next_warning(qpdf);
	printf("warning: %s\n", qpdf_get_error_full_text(qpdf, e));
	printf("  code: %d\n", qpdf_get_error_code(qpdf, e));
	printf("  file: %s\n", qpdf_get_error_filename(qpdf, e));
	printf(POS_FMT, qpdf_get_error_file_position(qpdf, e));
	printf("  text: %s\n", qpdf_get_error_message_detail(qpdf, e));
    }
    if (qpdf_has_error(qpdf))
    {
	e = qpdf_get_error(qpdf);
	assert(qpdf_has_error(qpdf) == QPDF_FALSE);
	printf("error: %s\n", qpdf_get_error_full_text(qpdf, e));
	printf("  code: %d\n", qpdf_get_error_code(qpdf, e));
	printf("  file: %s\n", qpdf_get_error_filename(qpdf, e));
	printf(POS_FMT, qpdf_get_error_file_position(qpdf, e));
	printf("  text: %s\n", qpdf_get_error_message_detail(qpdf, e));
    }
    else
    {
	e = qpdf_get_error(qpdf);
	assert(e == 0);
	assert(qpdf_get_error_code(qpdf, e) == qpdf_e_success);
	// Call these to ensure that they can be called on a null
	// error pointer.
	(void)qpdf_get_error_full_text(qpdf, e);
	(void)qpdf_get_error_filename(qpdf, e);
	(void)qpdf_get_error_file_position(qpdf, e);
	(void)qpdf_get_error_message_detail(qpdf, e);
    }
}

static void read_file_into_memory(char const* filename,
				  char** buf, unsigned long* size)
{
    char* buf_p = 0;
    FILE* f = NULL;
    size_t bytes_read = 0;
    size_t len = 0;

    f = safe_fopen(filename, "rb");
    fseek(f, 0, SEEK_END);
    *size = (unsigned long) ftell(f);
    fseek(f, 0, SEEK_SET);
    *buf = malloc(*size);
    if (*buf == NULL)
    {
	fprintf(stderr, "%s: unable to allocate %lu bytes\n",
		whoami, *size);
	exit(2);
    }
    buf_p = *buf;
    bytes_read = 0;
    len = 0;
    while ((len = fread(buf_p + bytes_read, 1, *size - bytes_read, f)) > 0)
    {
	bytes_read += len;
    }
    if (bytes_read != *size)
    {
	if (ferror(f))
	{
	    fprintf(stderr, "%s: failure reading file %s into memory:",
		    whoami, filename);
	}
	else
	{
	    fprintf(stderr, "%s: premature EOF reading file %s:",
		    whoami, filename);
	}
	fprintf(stderr, " read %lu, wanted %lu\n",
		(unsigned long) bytes_read, (unsigned long) *size);
	exit(2);
    }
    fclose(f);
}

static void test01(char const* infile,
		   char const* password,
		   char const* outfile,
		   char const* outfile2)
{
    qpdf_read(qpdf, infile, password);
    printf("version: %s\n", qpdf_get_pdf_version(qpdf));
    if (qpdf_get_pdf_extension_level(qpdf) > 0)
    {
        printf("extension level: %d\n", qpdf_get_pdf_extension_level(qpdf));
    }
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
    char* buf = NULL;
    unsigned long size = 0;
    read_file_into_memory(infile, &buf, &size);
    qpdf_read_memory(qpdf, infile, buf, size, password);
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
    qpdf_set_minimum_pdf_version_and_extension(qpdf, "1.7", 8);
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

static void print_info(char const* key)
{
    char const* value = qpdf_get_info_key(qpdf, key);
    printf("Info key %s: %s\n",
	   key, (value ? value : "(null)"));
}

static void test16(char const* infile,
		   char const* password,
		   char const* outfile,
		   char const* outfile2)
{
    unsigned long buflen = 0L;
    unsigned char const* buf = 0;
    FILE* f = 0;

    qpdf_read(qpdf, infile, password);
    print_info("/Author");
    print_info("/Producer");
    print_info("/Creator");
    qpdf_set_info_key(qpdf, "/Author", "Mr. Potato Head");
    qpdf_set_info_key(qpdf, "/Producer", "QPDF library");
    qpdf_set_info_key(qpdf, "/Creator", 0);
    print_info("/Author");
    print_info("/Producer");
    print_info("/Creator");
    qpdf_init_write_memory(qpdf);
    qpdf_set_static_ID(qpdf, QPDF_TRUE);
    qpdf_set_static_aes_IV(qpdf, QPDF_TRUE);
    qpdf_set_stream_data_mode(qpdf, qpdf_s_uncompress);
    qpdf_write(qpdf);
    f = safe_fopen(outfile, "wb");
    buflen = qpdf_get_buffer_length(qpdf);
    buf = qpdf_get_buffer(qpdf);
    fwrite(buf, 1, buflen, f);
    fclose(f);
    report_errors();
}

static void test17(char const* infile,
		   char const* password,
		   char const* outfile,
		   char const* outfile2)
{
    qpdf_read(qpdf, infile, password);
    qpdf_init_write(qpdf, outfile);
    qpdf_set_static_ID(qpdf, QPDF_TRUE);
    qpdf_set_static_aes_IV(qpdf, QPDF_TRUE);
    qpdf_set_r5_encryption_parameters(
	qpdf, "user3", "owner3", QPDF_TRUE, QPDF_TRUE,
	qpdf_r3p_low, qpdf_r3m_all, QPDF_TRUE);
    qpdf_write(qpdf);
    report_errors();
}

static void test18(char const* infile,
		   char const* password,
		   char const* outfile,
		   char const* outfile2)
{
    qpdf_read(qpdf, infile, password);
    qpdf_init_write(qpdf, outfile);
    qpdf_set_static_ID(qpdf, QPDF_TRUE);
    qpdf_set_static_aes_IV(qpdf, QPDF_TRUE);
    qpdf_set_r6_encryption_parameters(
	qpdf, "user4", "owner4", QPDF_TRUE, QPDF_TRUE,
	qpdf_r3p_low, qpdf_r3m_all, QPDF_TRUE);
    qpdf_write(qpdf);
    report_errors();
}

static void test19(char const* infile,
		   char const* password,
		   char const* outfile,
		   char const* outfile2)
{
    qpdf_read(qpdf, infile, password);
    qpdf_init_write(qpdf, outfile);
    qpdf_set_deterministic_ID(qpdf, QPDF_TRUE);
    qpdf_write(qpdf);
    report_errors();
}

int main(int argc, char* argv[])
{
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
    if ((argc == 2) && (strcmp(argv[1], "--version") == 0))
    {
	printf("qpdf-ctest version %s\n", qpdf_get_qpdf_version());
	return 0;
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
	  (n == 16) ? test16 :
	  (n == 17) ? test17 :
	  (n == 18) ? test18 :
	  (n == 19) ? test19 :
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
