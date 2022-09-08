#include <qpdf/Constants.h>
#include <qpdf/qpdfjob-c.h>
#include <qpdf/qpdflogger-c.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// This example demonstrates how we can redirect where saved output
// goes by calling the default logger's setSave method before running
// something with QPDFJob. See qpdfjob-c-save-attachment.c for an
// implementation that uses the C API.

static int
save_to_file(char const* data, size_t len, void* udata)
{
    FILE* f = (FILE*)udata;
    return fwrite(data, 1, len, f) != len;
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
main(int argc, char* argv[])
{
    char const* whoami = "qpdfjob-c-save-attachment";
    char const* filename = NULL;
    char const* key = NULL;
    char const* outfilename = NULL;
    char* attachment_arg = NULL;
    char const* attachment_flag = "--show-attachment=";
    size_t flag_len = 0;
    FILE* outfile = NULL;
    int status = 0;
    qpdfjob_handle j = NULL;
    qpdflogger_handle l = qpdflogger_default_logger();

    if (argc != 4) {
        fprintf(stderr, "Usage: %s file attachment-key outfile\n", whoami);
        exit(2);
    }

    filename = argv[1];
    key = argv[2];
    outfilename = argv[3];

    flag_len = strlen(attachment_flag) + strlen(key) + 1;
    attachment_arg = malloc(flag_len);
#ifdef _MSC_VER
    strncpy_s(attachment_arg, flag_len, attachment_flag, flag_len);
    strncat_s(attachment_arg, flag_len, key, flag_len - strlen(attachment_arg));
#else
    strncpy(attachment_arg, attachment_flag, flag_len);
    strncat(attachment_arg, key, flag_len - strlen(attachment_arg));
#endif

    char const* j_argv[5] = {
        whoami,
        filename,
        attachment_arg,
        "--",
        NULL,
    };
    outfile = do_fopen(outfilename);

    /* Use qpdflogger_set_save with a callback function to redirect
     * saved data. You can use other qpdf logger functions to capture
     * informational output, warnings, and errors.
     */
    qpdflogger_set_save(
        l, qpdf_log_dest_custom, save_to_file, (void*)outfile, 0);
    qpdflogger_cleanup(&l);
    j = qpdfjob_init();
    status = (qpdfjob_initialize_from_argv(j, j_argv) || qpdfjob_run(j));
    qpdfjob_cleanup(&j);
    free(attachment_arg);
    fclose(outfile);
    if (status == qpdf_exit_success) {
        printf("%s: wrote attachment to %s\n", whoami, outfilename);
    }
    return 0;
}
