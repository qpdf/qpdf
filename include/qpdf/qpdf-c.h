#ifndef __QPDF_C_H__
#define __QPDF_C_H__

/*
 * This file defines a basic "C" API for qpdf.  It provides access to
 * a subset of the QPDF library's capabilities to make them accessible
 * to callers who can't handle calling C++ functions or working with
 * C++ classes.  This may be especially useful to Windows users who
 * are accessing the qpdf DLL directly or to other people programming
 * in non-C/C++ languages that can call C code but not C++ code.
 *
 * There are several things to keep in mind when using the C API.
 *
 *     The C API is not as rich as the C++ API.  For any operations
 *     that involve actually manipulating PDF objects, you must use
 *     the C++ API.  The C API is primarily useful for doing basic
 *     transformations on PDF files similar to what you might do with
 *     the qpdf command-line tool.
 *
 *     These functions store their state in a qpdf_data object.
 *     Individual instances of qpdf_data are not thread-safe: although
 *     you may access different qpdf_data objects from different
 *     threads, you may not access one qpdf_data simultaneously from
 *     multiple threads.
 *
 *     All dynamic memory, except for that of the qpdf_data object
 *     itself, is managed by the library.  You must create a qpdf_data
 *     object using qpdf_init and free it using qpdf_cleanup.
 *
 *     Many functions return char*.  In all cases, the char* values
 *     returned are pointers to data inside the qpdf_data object.  As
 *     such, they are always freed by qpdf_cleanup.  In most cases,
 *     strings returned by functions here may be invalidated by
 *     subsequent function calls, sometimes even to different
 *     functions.  If you want a string to last past the next qpdf
 *     call or after a call to qpdf_cleanup, you should make a copy of
 *     it.
 *
 *     Many functions defined here merely set parameters and therefore
 *     never return error conditions.  Functions that may cause PDF
 *     files to be read or written may return error conditions.  Such
 *     functions return an error code.  If there were no errors or
 *     warnings, they return QPDF_SUCCESS.  If there were warnings,
 *     the return value has the QPDF_WARNINGS bit set.  If there
 *     errors, the QPDF_ERRORS bit is set.  In other words, if there
 *     are both warnings and errors, then the return status will be
 *     QPDF_WARNINGS | QPDF_ERRORS.  You may also call the
 *     qpdf_more_warnings and qpdf_more_errors functions to test
 *     whether there are unseen warning or error conditions.  By
 *     default, warnings are written to stderr when detected, but this
 *     behavior can be suppressed.  In all cases, errors and warnings
 *     may be retrieved by calling qpdf_next_warning and
 *     qpdf_next_error.  All exceptions thrown by the C++ interface
 *     are caught and converted into error messages by the C
 *     interface.
 *
 *     Most functions defined here have obvious counterparts that are
 *     methods to either QPDF or QPDFWriter.  Please see comments in
 *     QPDF.hh and QPDFWriter.hh for details on their use.  In order
 *     to avoid duplication of information, comments here focus
 *     primarily on differences between the C and C++ API.
 */

#include <qpdf/DLL.hh>

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct _qpdf_data* qpdf_data;

    /* Many functions return an integer error code.  Codes are defined
     * below.  See comments at the top of the file for details.  Note
     * that the values below can be logically orred together.
     */
    typedef int QPDF_ERROR_CODE;
#   define QPDF_SUCCESS 0
#   define QPDF_WARNINGS 1 << 0
#   define QPDF_ERRORS 1 << 1

    typedef int QPDF_BOOL;
#   define QPDF_TRUE 1
#   define QPDF_FALSE 0

    /* Returns dynamically allocated qpdf_data pointer; must be freed
     * by calling qpdf_cleanup.
     */
    DLL_EXPORT
    qpdf_data qpdf_init();

    /* Pass a pointer to the qpdf_data pointer created by qpdf_init to
     * clean up resources.
     */
    DLL_EXPORT
    void qpdf_cleanup(qpdf_data* qpdf);

    /* ERROR REPORTING */

    /* Returns 1 if there are any errors or warnings, and zero
     * otherwise.
     */
    DLL_EXPORT
    QPDF_BOOL qpdf_more_errors(qpdf_data qpdf);
    DLL_EXPORT
    QPDF_BOOL qpdf_more_warnings(qpdf_data qpdf);

    /* If there are any errors/warnings, returns a pointer to the next
     * error or warning.  Otherwise returns a null pointer.
     */
    DLL_EXPORT
    char const* qpdf_next_error(qpdf_data qpdf);
    DLL_EXPORT
    char const* qpdf_next_warning(qpdf_data qpdf);

    /* By default, warnings are written to stderr.  Passing true to
     * this function will prevent warnings from being written to
     * stderr.  They will still be available by calls to
     * qpdf_next_warning.
     */
    DLL_EXPORT
    void qpdf_set_suppress_warnings(qpdf_data qpdf, QPDF_BOOL value);

    /* READ FUNCTIONS */

    /* READ PARAMETER FUNCTIONS -- must be called before qpdf_read */

    DLL_EXPORT
    void qpdf_set_ignore_xref_streams(qpdf_data qpdf, QPDF_BOOL value);

    DLL_EXPORT
    void qpdf_set_attempt_recovery(qpdf_data qpdf, QPDF_BOOL value);

    /* Calling qpdf_read causes processFile to be called in the C++
     * API.  Basic parsing is performed, but data from the file is
     * only read as needed.  For files without passwords, pass a null
     * pointer as the password.
     */
    DLL_EXPORT
    QPDF_ERROR_CODE qpdf_read(qpdf_data qpdf, char const* filename,
			      char const* password);

    /* Read functions below must be called after qpdf_read. */

    /* Return the version of the PDF file. */
    DLL_EXPORT
    char const* qpdf_get_pdf_version(qpdf_data qpdf);

    /* Return the user password.  If the file is opened using the
     * owner password, the user password may be retrieved using this
     * function.  If the file is opened using the user password, this
     * function will return that user password.
     */
    DLL_EXPORT
    char const* qpdf_get_user_password(qpdf_data qpdf);

    /* Indicate whether the input file is linearized. */
    DLL_EXPORT
    QPDF_BOOL qpdf_is_linearized(qpdf_data qpdf);

    /* Indicate whether the input file is encrypted. */
    DLL_EXPORT
    QPDF_BOOL qpdf_is_encrypted(qpdf_data qpdf);

    /* WRITE FUNCTIONS */

    /* Set up for writing.  No writing is actually performed until the
     * call to qpdf_write().
     */

    /* Supply the name of the file to be written and initialize the
     * qpdf_data object to handle writing operations.  This function
     * also attempts to create the file.  The PDF data is not written
     * until the call to qpdf_write.
     */
    DLL_EXPORT
    QPDF_ERROR_CODE qpdf_init_write(qpdf_data qpdf, char const* filename);

#   define QPDF_OBJECT_STREAM_DISABLE 0
#   define QPDF_OBJECT_STREAM_PRESERVE 1
#   define QPDF_OBJECT_STREAM_GENERATE 2

    /* For mode, pass one of the QPDF_OBJECT_STREAM constants. */
    DLL_EXPORT
    void qpdf_set_object_stream_mode(qpdf_data qpdf, int mode);

#   define QPDF_STREAM_DATA_UNCOMPRESS 0
#   define QPDF_STREAM_DATA_PRESERVE 1
#   define QPDF_STREAM_DATA_COMPRESS 2
    /* For mode, pass one of the QPDF_STREAM_DATA constants. */
    DLL_EXPORT
    void qpdf_set_stream_data_mode(qpdf_data qpdf, int mode);

    DLL_EXPORT
    void qpdf_set_content_normalization(qpdf_data qpdf, QPDF_BOOL value);

    DLL_EXPORT
    void qpdf_set_qdf_mode(qpdf_data qpdf, QPDF_BOOL value);

    /* Never use qpdf_set_static_ID except in test suites to suppress
     * generation of a random /ID.
     */
    DLL_EXPORT
    void qpdf_set_static_ID(qpdf_data qpdf, QPDF_BOOL value);

    DLL_EXPORT
    void qpdf_set_suppress_original_object_IDs(
	qpdf_data qpdf, QPDF_BOOL value);

    DLL_EXPORT
    void qpdf_set_preserve_encryption(qpdf_data qpdf, QPDF_BOOL value);

    DLL_EXPORT
    void qpdf_set_r2_encryption_parameters(
	qpdf_data qpdf, char const* user_password, char const* owner_password,
	QPDF_BOOL allow_print, QPDF_BOOL allow_modify,
	QPDF_BOOL allow_extract, QPDF_BOOL allow_annotate);

#   define QPDF_R3_PRINT_FULL 0
#   define QPDF_R3_PRINT_LOW 1
#   define QPDF_R3_PRINT_NONE 2

#   define QPDF_R3_MODIFY_ALL 0
#   define QPDF_R3_MODIFY_ANNOTATE 1
#   define QPDF_R3_MODIFY_FORM 2
#   define QPDF_R3_MODIFY_ASSEMBLY 3
#   define QPDF_R3_MODIFY_NONE 4

    /* Value of print should be one of the QPDF_R3_PRINT constants.
     * Value of modify should be one of the QPDF_R3_MODIFY constants.
     */
    DLL_EXPORT
    void qpdf_set_r3_encryption_parameters(
	qpdf_data qpdf, char const* user_password, char const* owner_password,
	QPDF_BOOL allow_accessibility, QPDF_BOOL allow_extract,
	int print, int modify);

    DLL_EXPORT
    void qpdf_set_linearization(qpdf_data qpdf, QPDF_BOOL value);

    /* Do actual write operation. */
    DLL_EXPORT
    QPDF_ERROR_CODE qpdf_write(qpdf_data qpdf);

#ifdef __cplusplus
}
#endif


#endif /* __QPDF_C_H__ */
