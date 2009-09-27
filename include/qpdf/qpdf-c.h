#ifndef __QPDF_C_H__
#define __QPDF_C_H__

/*
 * This file defines a basic "C" API for qpdf.  It provides access to
 * a subset of the QPDF library's capabilities to make them accessible
 * to callers who can't handle calling C++ functions or working with
 * C++ classes.  This may be especially useful to Windows users who
 * are accessing the qpdflib DLL directly or to other people
 * programming in non-C/C++ languages that can call C code but not C++
 * code.
 */

#include <qpdf/DLL.hh>

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct _qpdf_data* qpdf_data;

    /* Many functions return an integer error code.  Codes are defined
     * below.  See ERROR REPORTING below.
     */
    typedef int QPDF_ERROR_CODE;
#   define QPDF_SUCCESS 0
#   define QPDF_WARNINGS 1
#   define QPDF_ERRORS 2

    typedef int QPDF_BOOL;
#   define QPDF_TRUE 1
#   define QPDF_FALSE 0

    /* Returns dynamically allocated qpdf_data pointer; must be freed
     * by calling qpdf_cleanup.  Note that qpdf_data is not
     * thread-safe: although you may access different qpdf_data
     * objects from different threads, you may not access one
     * qpdf_data simultaneously from multiple threads.  Many functions
     * defined below return char*.  In all cases, the char* values
     * returned are pointers to data inside the qpdf_data object.  As
     * such, they are always freed by qpdf_cleanup.  In some cases,
     * strings returned by functions here may be overwritten by
     * additional function calls, so if you really want a string to
     * last past the next qpdf call, you should make a copy of it.
     */
    DLL_EXPORT
    qpdf_data qpdf_init();

    /* Pass a pointer to the qpdf_data pointer created by qpdf_init to
     * clean up resoures.
     */
    DLL_EXPORT
    void qpdf_cleanup(qpdf_data* qpdf);

    /* ERROR REPORTING */

    /* Returns 1 if there are any errors or warnings, and zero
     * otherwise.
     */
    DLL_EXPORT
    QPDF_BOOL qpdf_more_errors(qpdf_data qpdf);

    /* If there are any errors, returns a pointer to the next error.
     * Otherwise returns a null pointer.  The error value returned is
     * a pointer to data inside the qpdf_data object.  It will become
     * valid the next time a qpdf function that returns a string is
     * called or after a call to qpdf_cleanup.
     */
    DLL_EXPORT
    char const* qpdf_next_error(qpdf_data qpdf);

    /* These functions are analogous to the "error" counterparts but
     * apply to warnings.
     */

    DLL_EXPORT
    QPDF_BOOL qpdf_more_warnings(qpdf_data qpdf);
    DLL_EXPORT
    char const* qpdf_next_warning(qpdf_data qpdf);

    /* READ PARAMETER FUNCTIONS */

    /* By default, warnings are written to stderr.  Passing true to
    this function will prevent warnings from being written to stderr.
    They will still be available by calls to qpdf_next_warning.
    */
    DLL_EXPORT
    void qpdf_set_suppress_warnings(qpdf_data qpdf, QPDF_BOOL val);

    /* READ FUNCTIONS */

    /* POST-READ QUERY FUNCTIONS */

    /* These functions are invalid until after qpdf_read has been
     * called. */

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
    QPDF_ERROR_CODE qpdf_init_write(qpdf_data data, char const* filename);

    /* XXX Get public interface from QPDFWriter */

    /* Perform the actual write operation. */
    DLL_EXPORT
    QPDF_ERROR_CODE qpdf_write();

#ifdef __cplusplus
}
#endif


#endif /* __QPDF_C_H__ */
