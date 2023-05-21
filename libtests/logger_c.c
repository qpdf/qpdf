#include <qpdf/assert_test.h>

#include <qpdf/qpdflogger-c.h>

#include <qpdf/qpdfjob-c.h>

#include <stdio.h>
#include <stdlib.h>

static int
fn(char const* data, size_t len, void* udata)
{
    FILE* f = (FILE*)udata;
    return fwrite(data, 1, len, f) != len;
}

static void
do_run(char const* json, int exp_status)
{
    int status = qpdfjob_run_from_json(json);
    assert(status == exp_status);
}

static FILE*
do_fopen(char const* filename)
{
    FILE* f = NULL;
#ifdef _MSC_VER
    if (fopen_s(&f, filename, "wb") != 0) {
        f = NULL;
    }
#else
    f = fopen(filename, "wb");
#endif
    if (f == NULL) {
        fprintf(stderr, "unable to open %s\n", filename);
        exit(2);
    }
    return f;
}

int
main()
{
    FILE* info = do_fopen("info");
    FILE* warn = do_fopen("warn");
    FILE* error = do_fopen("error");
    FILE* save = do_fopen("save");
    FILE* save2 = do_fopen("save2");
    qpdflogger_handle l = qpdflogger_default_logger();

    qpdflogger_set_info(l, qpdf_log_dest_custom, fn, (void*)info);
    qpdflogger_set_warn(l, qpdf_log_dest_custom, fn, (void*)warn);
    qpdflogger_set_error(l, qpdf_log_dest_custom, fn, (void*)error);
    qpdflogger_set_save(l, qpdf_log_dest_custom, fn, (void*)save, 0);

    do_run("{\"inputFile\": \"normal.pdf\", \"showNpages\": \"\"}", qpdf_exit_success);
    do_run("{\"inputFile\": \"warning.pdf\", \"showNpages\": \"\"}", qpdf_exit_warning);
    do_run("{\"inputFile\": \"missing.pdf\", \"showNpages\": \"\"}", qpdf_exit_error);
    do_run(
        "{\"inputFile\": \"normal.pdf\","
        " \"staticId\": \"\","
        " \"outputFile\": \"-\"}",
        qpdf_exit_success);

    fclose(info);
    fclose(warn);
    fclose(error);
    fclose(save);

    qpdflogger_set_info(l, qpdf_log_dest_stderr, NULL, NULL);
    qpdflogger_set_warn(l, qpdf_log_dest_stdout, NULL, NULL);
    qpdflogger_set_error(l, qpdf_log_dest_default, NULL, NULL);
    qpdflogger_set_save(l, qpdf_log_dest_custom, fn, (void*)save2, 0);

    do_run("{\"inputFile\": \"2pages.pdf\", \"showNpages\": \"\"}", qpdf_exit_success);
    do_run("{\"inputFile\": \"warning.pdf\", \"showNpages\": \"\"}", qpdf_exit_warning);
    do_run(

        "{\"inputFile\": \"missing.pdf\", \"showNpages\": \"\"}", qpdf_exit_error);
    do_run(
        "{\"inputFile\": \"attach.pdf\","
        " \"showAttachment\": \"a\"}",
        qpdf_exit_success);

    /* This won't change save since it's already set */
    qpdflogger_save_to_standard_output(l, 1);
    do_run(
        "{\"inputFile\": \"attach.pdf\","
        " \"showAttachment\": \"a\"}",
        qpdf_exit_success);

    qpdflogger_cleanup(&l);

    return 0;
}
