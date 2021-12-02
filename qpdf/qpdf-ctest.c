#include <qpdf/qpdf-c.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "../libqpdf/qpdf/qpdf-config.h" // for LL_FMT

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
#define POS_FMT "  pos : " LL_FMT "\n"
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
    free(buf);
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
    qpdf_set_r3_encryption_parameters2(
	qpdf, "user2", "owner2", QPDF_TRUE, QPDF_TRUE,
        QPDF_TRUE, QPDF_TRUE, QPDF_TRUE, QPDF_TRUE,
	qpdf_r3p_low);
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
    qpdf_set_r4_encryption_parameters2(
	qpdf, "user2", "owner2", QPDF_TRUE, QPDF_TRUE,
        QPDF_TRUE, QPDF_TRUE, QPDF_TRUE, QPDF_TRUE,
	qpdf_r3p_low, QPDF_TRUE, QPDF_TRUE);
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
    buflen = (unsigned long)(qpdf_get_buffer_length(qpdf));
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
    qpdf_set_r5_encryption_parameters2(
	qpdf, "user3", "owner3", QPDF_TRUE, QPDF_TRUE,
        QPDF_TRUE, QPDF_TRUE, QPDF_TRUE, QPDF_TRUE,
	qpdf_r3p_low, QPDF_TRUE);
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
    qpdf_set_r6_encryption_parameters2(
	qpdf, "user4", "owner4", QPDF_TRUE, QPDF_TRUE,
        QPDF_TRUE, QPDF_TRUE, QPDF_TRUE, QPDF_TRUE,
	qpdf_r3p_low, QPDF_TRUE);
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

static void test20(char const* infile,
		   char const* password,
		   char const* outfile,
		   char const* outfile2)
{
    qpdf_read(qpdf, infile, password);
    qpdf_init_write(qpdf, outfile);
    qpdf_set_static_ID(qpdf, QPDF_TRUE);
    qpdf_set_static_aes_IV(qpdf, QPDF_TRUE);
    qpdf_set_compress_streams(qpdf, QPDF_FALSE);
    qpdf_set_decode_level(qpdf, qpdf_dl_specialized);
    qpdf_write(qpdf);
    report_errors();
}

static void test21(char const* infile,
		   char const* password,
		   char const* outfile,
		   char const* outfile2)
{
    qpdf_read(qpdf, infile, password);
    qpdf_init_write(qpdf, outfile);
    qpdf_set_static_ID(qpdf, QPDF_TRUE);
    qpdf_set_static_aes_IV(qpdf, QPDF_TRUE);
    qpdf_set_preserve_unreferenced_objects(qpdf, QPDF_TRUE);
    qpdf_write(qpdf);
    report_errors();
}

static void test22(char const* infile,
		   char const* password,
		   char const* outfile,
		   char const* outfile2)
{
    qpdf_read(qpdf, infile, password);
    qpdf_init_write(qpdf, outfile);
    qpdf_set_static_ID(qpdf, QPDF_TRUE);
    qpdf_set_static_aes_IV(qpdf, QPDF_TRUE);
    qpdf_set_compress_streams(qpdf, QPDF_FALSE);
    qpdf_set_newline_before_endstream(qpdf, QPDF_TRUE);
    qpdf_write(qpdf);
    report_errors();
}

static void test23(char const* infile,
		   char const* password,
		   char const* outfile,
		   char const* outfile2)
{
    QPDF_ERROR_CODE status = 0;
    qpdf_read(qpdf, infile, password);
    status = qpdf_check_pdf(qpdf);
    printf("status: %d\n", status);
    report_errors();
}

static void test24(char const* infile,
		   char const* password,
		   char const* outfile,
		   char const* outfile2)
{
    /* This test case is designed for minimal.pdf. */
    qpdf_read(qpdf, infile, password);
    qpdf_oh trailer = qpdf_get_trailer(qpdf);
    /* The library never returns 0 */
    assert(trailer == 1);
    qpdf_oh root = qpdf_get_root(qpdf);
    assert(qpdf_oh_get_generation(qpdf, root) == 0);
    qpdf_oh root_from_trailer = qpdf_oh_get_key(qpdf, trailer, "/Root");
    assert(qpdf_oh_get_object_id(qpdf, root) ==
           qpdf_oh_get_object_id(qpdf, root_from_trailer));
    qpdf_oh pages = qpdf_oh_get_key(qpdf, root, "/Pages");
    assert(qpdf_oh_is_dictionary(qpdf, pages));
    qpdf_oh kids = qpdf_oh_get_key(qpdf, pages, "/Kids");
    assert(qpdf_oh_is_array(qpdf, kids));
    assert(qpdf_oh_get_array_n_items(qpdf, kids) == 1);
    qpdf_oh page1 = qpdf_oh_get_array_item(qpdf, kids, 0);
    qpdf_oh_begin_dict_key_iter(qpdf, page1);
    while (qpdf_oh_dict_more_keys(qpdf))
    {
        printf("page dictionary key: %s\n", qpdf_oh_dict_next_key(qpdf));
    }
    qpdf_oh type = qpdf_oh_get_key(qpdf, page1, "/Type");
    assert(qpdf_oh_is_name(qpdf, type));
    assert(strcmp(qpdf_oh_get_name(qpdf, type), "/Page") == 0);
    qpdf_oh parent = qpdf_oh_get_key(qpdf, page1, "/Parent");
    assert(qpdf_oh_is_indirect(qpdf, parent));
    qpdf_oh mediabox = qpdf_oh_get_key(qpdf, page1, "/MediaBox");
    assert(! qpdf_oh_is_scalar(qpdf, mediabox));
    assert(qpdf_oh_is_array(qpdf, mediabox));
    assert(qpdf_oh_get_array_n_items(qpdf, mediabox) == 4);
    qpdf_oh wrapped_mediabox = qpdf_oh_wrap_in_array(qpdf, mediabox);
    qpdf_oh cloned_mediabox = qpdf_oh_new_object(qpdf, mediabox);
    assert(wrapped_mediabox != mediabox);
    assert(cloned_mediabox != mediabox);
    assert(qpdf_oh_get_array_n_items(qpdf, wrapped_mediabox) == 4);
    for (int i = 0; i < 4; ++i)
    {
        qpdf_oh item = qpdf_oh_get_array_item(qpdf, mediabox, i);
        qpdf_oh item2 = qpdf_oh_get_array_item(qpdf, wrapped_mediabox, i);
        qpdf_oh item3 = qpdf_oh_get_array_item(qpdf, cloned_mediabox, i);
        assert(qpdf_oh_get_int_value_as_int(qpdf, item) ==
               qpdf_oh_get_int_value_as_int(qpdf, item2));
        assert(qpdf_oh_get_int_value_as_int(qpdf, item) ==
               qpdf_oh_get_int_value_as_int(qpdf, item3));
        printf("item %d: %d %.2f\n",
               i, qpdf_oh_get_int_value_as_int(qpdf, item),
               qpdf_oh_get_numeric_value(qpdf, item));
        qpdf_oh_release(qpdf, item);
    }
    qpdf_oh i2 = qpdf_oh_get_array_item(qpdf, mediabox, 2);
    assert(qpdf_oh_get_int_value_as_int(qpdf, i2) == 612);
    assert(qpdf_oh_get_int_value(qpdf, i2) == 612ll);
    assert(qpdf_oh_get_uint_value_as_uint(qpdf, i2) == 612u);
    assert(qpdf_oh_get_uint_value(qpdf, i2) == 612ull);
    assert(! qpdf_oh_is_operator(qpdf, i2));
    assert(! qpdf_oh_is_inline_image(qpdf, i2));
    qpdf_oh encoding = qpdf_oh_get_key(
        qpdf, qpdf_oh_get_key(
            qpdf, qpdf_oh_get_key(
                qpdf, qpdf_oh_get_key(
                    qpdf, page1, "/Resources"),
                "/Font"),
            "/F1"),
        "/Encoding");
    assert(strcmp(qpdf_oh_get_name(qpdf, encoding), "/WinAnsiEncoding") == 0);
    qpdf_oh contents = qpdf_oh_get_key(qpdf, page1, "/Contents");
    assert(qpdf_oh_is_stream(qpdf, contents));
    qpdf_oh contents_dict = qpdf_oh_get_dict(qpdf, contents);
    assert(! qpdf_oh_is_scalar(qpdf, contents));
    assert(! qpdf_oh_is_scalar(qpdf, contents_dict));
    qpdf_oh contents_length = qpdf_oh_get_key(qpdf, contents_dict, "/Length");
    assert(qpdf_oh_is_integer(qpdf, contents_length));
    assert(qpdf_oh_is_scalar(qpdf, contents_length));
    assert(qpdf_oh_is_number(qpdf, contents_length));
    qpdf_oh contents_array = qpdf_oh_wrap_in_array(qpdf, contents);
    assert(qpdf_oh_get_array_n_items(qpdf, contents_array) == 1);
    assert(qpdf_oh_get_object_id(
               qpdf, qpdf_oh_get_array_item(qpdf, contents_array, 0)) ==
           qpdf_oh_get_object_id(qpdf, contents));
    qpdf_oh wrapped_contents_array =
        qpdf_oh_wrap_in_array(qpdf, contents_array);
    assert(qpdf_oh_get_array_n_items(qpdf, wrapped_contents_array) == 1);
    assert(qpdf_oh_get_object_id(
               qpdf, qpdf_oh_get_array_item(qpdf, wrapped_contents_array, 0)) ==
           qpdf_oh_get_object_id(qpdf, contents));
    qpdf_oh resources = qpdf_oh_get_key(qpdf, page1, "/Resources");
    qpdf_oh procset = qpdf_oh_get_key(qpdf, resources, "/ProcSet");
    assert(strcmp(qpdf_oh_unparse(qpdf, procset),
           "5 0 R") == 0);
    assert(strcmp(qpdf_oh_unparse_resolved(qpdf, procset),
           "[ /PDF /Text ]") == 0);
    qpdf_oh_make_direct(qpdf, procset);
    assert(strcmp(qpdf_oh_unparse(qpdf, procset),
           "[ /PDF /Text ]") == 0);
    qpdf_oh_replace_key(qpdf, resources, "/ProcSet", procset);

    qpdf_oh parsed = qpdf_oh_parse(
        qpdf, "[ 1 2.0 (3\xf7) << /Four [/Five] >> null true ]");
    qpdf_oh p_int = qpdf_oh_get_array_item(qpdf, parsed, 0);
    qpdf_oh p_real = qpdf_oh_get_array_item(qpdf, parsed, 1);
    qpdf_oh p_string = qpdf_oh_get_array_item(qpdf, parsed, 2);
    qpdf_oh p_dict = qpdf_oh_get_array_item(qpdf, parsed, 3);
    qpdf_oh p_null = qpdf_oh_get_array_item(qpdf, parsed, 4);
    qpdf_oh p_bool = qpdf_oh_get_array_item(qpdf, parsed, 5);
    assert(qpdf_oh_is_integer(qpdf, p_int) &&
           qpdf_oh_get_int_value_as_int(qpdf, p_int) == 1);
    assert(qpdf_oh_is_real(qpdf, p_real) &&
           (strcmp(qpdf_oh_get_real_value(qpdf, p_real), "2.0") == 0) &&
           qpdf_oh_get_numeric_value(qpdf, p_real) == 2.0);
    assert(qpdf_oh_is_string(qpdf, p_string) &&
           (strcmp(qpdf_oh_get_string_value(qpdf, p_string), "3\xf7") == 0) &&
           (strcmp(qpdf_oh_get_utf8_value(qpdf, p_string), "3\xc3\xb7") == 0) &&
           (strcmp(qpdf_oh_unparse_binary(qpdf, p_string), "<33f7>") == 0));
    assert(qpdf_oh_is_dictionary(qpdf, p_dict));
    qpdf_oh p_five = qpdf_oh_get_key(qpdf, p_dict, "/Four");
    assert(qpdf_oh_is_or_has_name(qpdf, p_five, "/Five"));
    assert(qpdf_oh_is_or_has_name(
               qpdf, qpdf_oh_get_array_item(qpdf, p_five, 0), "/Five"));
    assert(qpdf_oh_is_null(qpdf, p_null));
    assert(qpdf_oh_is_bool(qpdf, p_bool) &&
           (qpdf_oh_get_bool_value(qpdf, p_bool) == QPDF_TRUE));
    qpdf_oh_erase_item(qpdf, parsed, 4);
    qpdf_oh_insert_item(
        qpdf, parsed, 2,
        qpdf_oh_parse(qpdf, "<</A 1 /B 2 /C 3 /D 4>>"));
    qpdf_oh new_dict = qpdf_oh_get_array_item(qpdf, parsed, 2);
    assert(qpdf_oh_has_key(qpdf, new_dict, "/A"));
    assert(qpdf_oh_has_key(qpdf, new_dict, "/D"));
    qpdf_oh new_array = qpdf_oh_new_array(qpdf);
    qpdf_oh_replace_or_remove_key(
        qpdf, new_dict, "/A", qpdf_oh_new_null(qpdf));
    qpdf_oh_replace_or_remove_key(
        qpdf, new_dict, "/B", new_array);
    qpdf_oh_replace_key(
        qpdf, new_dict, "/C", qpdf_oh_new_dictionary(qpdf));
    qpdf_oh_remove_key(qpdf, new_dict, "/D");
    assert(! qpdf_oh_has_key(qpdf, new_dict, "/A"));
    assert(! qpdf_oh_has_key(qpdf, new_dict, "/D"));
    qpdf_oh_append_item(
        qpdf, new_array, qpdf_oh_new_string(qpdf, "potato"));
    qpdf_oh_append_item(
        qpdf, new_array,
        qpdf_oh_new_unicode_string(qpdf, "qww\xc3\xb7\xcf\x80"));
    qpdf_oh_append_item(qpdf, new_array, qpdf_oh_new_null(qpdf)); /* 2 */
    qpdf_oh_append_item(qpdf, new_array, qpdf_oh_new_null(qpdf)); /* 3 */
    qpdf_oh_set_array_item(
        qpdf, new_array, 2,
        qpdf_oh_new_name(qpdf, "/Quack"));
    qpdf_oh_append_item(
        qpdf, new_array,
        qpdf_oh_new_real_from_double(qpdf, 4.123, 2));
    qpdf_oh_append_item(
        qpdf, new_array,
        qpdf_oh_new_real_from_string(qpdf, "5.0"));
    qpdf_oh_append_item(
        qpdf, new_array,
        qpdf_oh_new_integer(qpdf, 6));
    qpdf_oh_append_item(
        qpdf, new_array, qpdf_oh_new_bool(qpdf, QPDF_TRUE));
    qpdf_oh_replace_key(qpdf, root, "/QTest", new_dict);

    /* Release and access to exercise warnings */
    qpdf_oh_release(qpdf, page1);
    contents = qpdf_oh_get_key(qpdf, page1, "/Contents");
    assert(qpdf_oh_is_null(qpdf, contents));
    assert(qpdf_oh_is_array(qpdf, mediabox));
    qpdf_oh_release_all(qpdf);
    assert(! qpdf_oh_is_null(qpdf, mediabox));
    assert(! qpdf_oh_is_array(qpdf, mediabox));
    /* Make sure something is assigned when we exit so we check that
     * it gets properl freed.
     */
    qpdf_get_root(qpdf);

    qpdf_init_write(qpdf, outfile);
    qpdf_set_static_ID(qpdf, QPDF_TRUE);
    qpdf_set_qdf_mode(qpdf, QPDF_TRUE);
    qpdf_set_suppress_original_object_IDs(qpdf, QPDF_TRUE);
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
	  (n == 20) ? test20 :
	  (n == 21) ? test21 :
	  (n == 22) ? test22 :
	  (n == 23) ? test23 :
	  (n == 24) ? test24 :
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
