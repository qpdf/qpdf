/*
 * This is an example program to demonstrate use of object handle
 * functions in the C API.
 */

#include <qpdf/qpdf-c.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char const* whoami = 0;

static void
usage()
{
    fprintf(stderr, "Usage: %s infile infile-password outfile\n", whoami);
    exit(2);
}

QPDF_BOOL
modify_file(qpdf_data qpdf)
{
    /* This small example performs the following operation on the
     * document catalog (a.k.a. root):
     * - Remove PageLayout
     * - Remove OpenAction
     * - If there are outlines, set PageMode to UseOutlines; otherwise,
     *   remove it.
     */

    qpdf_oh root = qpdf_get_root(qpdf);
    qpdf_oh_remove_key(qpdf, root, "/PageLayout");
    qpdf_oh_remove_key(qpdf, root, "/OpenAction");
    /* 0 is never a valid qpdf_oh */
    qpdf_oh pagemode = 0;
    if (qpdf_oh_is_dictionary(
            qpdf, qpdf_oh_get_key(qpdf, root, "/PageLabels"))) {
        pagemode = qpdf_oh_new_name(qpdf, "/UseOutlines");
    } else {
        pagemode = qpdf_oh_new_null(qpdf);
    }
    qpdf_oh_replace_or_remove_key(qpdf, root, "/PageMode", pagemode);

    return QPDF_TRUE;
}

int
main(int argc, char* argv[])
{
    char* infile = NULL;
    char* password = NULL;
    char* outfile = NULL;
    qpdf_data qpdf = qpdf_init();
    int warnings = 0;
    int errors = 0;
    char* p = 0;

    if ((p = strrchr(argv[0], '/')) != NULL) {
        whoami = p + 1;
    } else if ((p = strrchr(argv[0], '\\')) != NULL) {
        whoami = p + 1;
    } else {
        whoami = argv[0];
    }

    if (argc != 4) {
        usage();
    }

    infile = argv[1];
    password = argv[2];
    outfile = argv[3];

    if (((qpdf_read(qpdf, infile, password) & QPDF_ERRORS) == 0) &&
        modify_file(qpdf) &&
        ((qpdf_init_write(qpdf, outfile) & QPDF_ERRORS) == 0)) {
        /* Use static ID for testing only. For production, a
         * non-static ID is used. See also
         * qpdf_set_deterministic_ID. */
        qpdf_set_static_ID(qpdf, QPDF_TRUE); /* for testing only */
        qpdf_write(qpdf);
    }
    while (qpdf_more_warnings(qpdf)) {
        warnings = 1;
        printf(
            "warning: %s\n",
            qpdf_get_error_full_text(qpdf, qpdf_next_warning(qpdf)));
    }
    if (qpdf_has_error(qpdf)) {
        errors = 1;
        printf(
            "error: %s\n",
            qpdf_get_error_full_text(qpdf, qpdf_get_error(qpdf)));
    }
    qpdf_cleanup(&qpdf);
    if (errors) {
        return 2;
    } else if (warnings) {
        return 3;
    }

    return 0;
}
