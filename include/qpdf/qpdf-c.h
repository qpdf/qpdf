/* Copyright (c) 2005-2022 Jay Berkenbilt
 *
 * This file is part of qpdf.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Versions of qpdf prior to version 7 were released under the terms
 * of version 2.0 of the Artistic License. At your option, you may
 * continue to consider qpdf to be licensed under those terms. Please
 * see the manual for additional information.
 */

#ifndef QPDF_C_H
#define QPDF_C_H

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
 *     Error handling is tricky because the underlying C++ API uses
 *     exception handling. See "ERROR HANDLING" below for a detailed
 *     explanation.
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
 *     itself, is managed by the library unless otherwise noted. You
 *     must create a qpdf_data object using qpdf_init and free it
 *     using qpdf_cleanup.
 *
 *     Many functions return char*. In all cases, the char* values
 *     returned are pointers to data inside the qpdf_data object. As
 *     such, they are always freed by qpdf_cleanup. In most cases,
 *     strings returned by functions here may be invalidated by
 *     subsequent function calls, sometimes even to different
 *     functions. If you want a string to last past the next qpdf call
 *     or after a call to qpdf_cleanup, you should make a copy of it.
 *
 *     Since it is possible for a PDF string to contain null
 *     characters, a function that returns data originating from a PDF
 *     string may also contain null characters. To handle that case,
 *     you call qpdf_get_last_string_length() to get the length of
 *     whatever string was just returned. See STRING FUNCTIONS below.
 *
 *     Most functions defined here have obvious counterparts that are
 *     methods to either QPDF or QPDFWriter.  Please see comments in
 *     QPDF.hh and QPDFWriter.hh for details on their use.  In order
 *     to avoid duplication of information, comments here focus
 *     primarily on differences between the C and C++ API.
 */

/* ERROR HANDLING -- changed in qpdf 10.5 */

/* SUMMARY: The only way to know whether a function that does not
 * return an error code has encountered an error is to call
 * qpdf_has_error after each function. You can do this even for
 * functions that do return error codes. You can also call
 * qpdf_silence_errors to prevent qpdf from writing these errors to
 * stderr.
 *
 * DETAILS:
 *
 * The data type underlying qpdf_data maintains a list of warnings and
 * a single error. To retrieve warnings, call qpdf_next_warning while
 * qpdf_more_warnings is true. To retrieve the error, call
 * qpdf_get_error when qpdf_has_error is true.
 *
 * There are several things that are important to understand.
 *
 * Some functions return an error code. The value of the error code is
 * made up of a bitwise-OR of QPDF_WARNINGS and QPDF_ERRORS. The
 * QPDF_ERRORS bit is set if there was an error during the *most
 * recent call* to the API. The QPDF_WARNINGS bit is set if there are
 * any warnings that have not yet been retrieved by calling
 * qpdf_more_warnings. It is possible for both its or neither bit to
 * be set.
 *
 * The expected mode of operation is to go through a series of
 * operations, checking for errors after each call, but only checking
 * for warnings at the end. This is similar to how it works in the C++
 * API where warnings are handled in exactly this way but errors
 * result in exceptions being thrown. However, in both the C and C++
 * API, it is possible to check for and handle warnings as they arise.
 *
 * Some functions return values (or void) rather than an error code.
 * This is especially true with the object handling functions. Those
 * functions can still generate errors. To handle errors in those
 * cases, you should explicitly call qpdf_has_error(). Note that, if
 * you want to avoid the inconsistencies in the interface, you can
 * always check for error conditions in this way rather than looking
 * at status return codes.
 *
 * Prior to qpdf 10.5, if one of the functions that does not return an
 * error code encountered an exception, it would cause the entire
 * program to crash. Starting in qpdf 10.5, the default response to an
 * error condition in these situations is to print the error to
 * standard error, issue exactly one warning indicating that such an
 * error occurred, and return a sensible fallback value (0 for
 * numbers, QPDF_FALSE for booleans, "" for strings, or a null or
 * uninitialized object handle). This is better than the old behavior
 * but still undesirable as the best option is to explicitly check for
 * error conditions.
 *
 * To prevent qpdf from writing error messages to stderr in this way,
 * you can call qpdf_silence_errors(). This signals to the qpdf
 * library that you intend to check the error codes yourself.
 *
 * If you encounter a situation where an exception from the C++ code
 * is not properly converted to an error as described above, it is a
 * bug in qpdf, which should be reported at
 * https://github.com/qpdf/qpdf/issues/new.
 */

#include <qpdf/Constants.h>
#include <qpdf/DLL.h>
#include <qpdf/Types.h>
#include <qpdf/qpdflogger-c.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct _qpdf_data* qpdf_data;
    typedef struct _qpdf_error* qpdf_error;

    /* Many functions return an integer error code.  Codes are defined
     * below.  See comments at the top of the file for details.  Note
     * that the values below can be logically orred together.
     */
    typedef int QPDF_ERROR_CODE;
#define QPDF_SUCCESS 0
#define QPDF_WARNINGS 1 << 0
#define QPDF_ERRORS 1 << 1

    typedef int QPDF_BOOL;
#define QPDF_TRUE 1
#define QPDF_FALSE 0

    /* From qpdf 10.5: call this method to signal to the library that
     * you are explicitly handling errors from functions that don't
     * return error codes. Otherwise, the library will print these
     * error conditions to stderr and issue a warning. Prior to 10.5,
     * the program would have crashed from an unhandled exception.
     */
    QPDF_DLL
    void qpdf_silence_errors(qpdf_data qpdf);

    /* Returns the version of the qpdf software. This is guaranteed to
     * be a static value.
     */
    QPDF_DLL
    char const* qpdf_get_qpdf_version();

    /* Returns dynamically allocated qpdf_data pointer; must be freed
     * by calling qpdf_cleanup. You must call qpdf_read, one of the
     * other qpdf_read_* functions, or qpdf_empty_pdf before calling
     * any function that would need to operate on the PDF file.
     */
    QPDF_DLL
    qpdf_data qpdf_init();

    /* Pass a pointer to the qpdf_data pointer created by qpdf_init to
     * clean up resources. This does not include buffers initialized
     * by functions that return stream data but it otherwise includes
     * all data associated with the QPDF object or any object handles.
     */
    QPDF_DLL
    void qpdf_cleanup(qpdf_data* qpdf);

    /* ERROR REPORTING */

    /* Returns 1 if there is an error condition.  The error condition
     * can be retrieved by a single call to qpdf_get_error.
     */
    QPDF_DLL
    QPDF_BOOL qpdf_has_error(qpdf_data qpdf);

    /* Returns the error condition, if any.  The return value is a
     * pointer to data that will become invalid after the next call to
     * this function, qpdf_next_warning, or qpdf_cleanup.  After this
     * function is called, qpdf_has_error will return QPDF_FALSE until
     * the next error condition occurs.  If there is no error
     * condition, this function returns a null pointer.
     */
    QPDF_DLL
    qpdf_error qpdf_get_error(qpdf_data qpdf);

    /* Returns 1 if there are any unretrieved warnings, and zero
     * otherwise.
     */
    QPDF_DLL
    QPDF_BOOL qpdf_more_warnings(qpdf_data qpdf);

    /* If there are any warnings, returns a pointer to the next
     * warning.  Otherwise returns a null pointer.
     */
    QPDF_DLL
    qpdf_error qpdf_next_warning(qpdf_data qpdf);

    /* Extract fields of the error. */

    /* Use this function to get a full error message suitable for
     * showing to the user. */
    QPDF_DLL
    char const* qpdf_get_error_full_text(qpdf_data q, qpdf_error e);

    /* Use these functions to extract individual fields from the
     * error; see QPDFExc.hh for details. */
    QPDF_DLL
    enum qpdf_error_code_e qpdf_get_error_code(qpdf_data q, qpdf_error e);
    QPDF_DLL
    char const* qpdf_get_error_filename(qpdf_data q, qpdf_error e);
    QPDF_DLL
    unsigned long long qpdf_get_error_file_position(qpdf_data q, qpdf_error e);
    QPDF_DLL
    char const* qpdf_get_error_message_detail(qpdf_data q, qpdf_error e);

    /* By default, warnings are written to stderr.  Passing true to
     * this function will prevent warnings from being written to
     * stderr.  They will still be available by calls to
     * qpdf_next_warning.
     */
    QPDF_DLL
    void qpdf_set_suppress_warnings(qpdf_data qpdf, QPDF_BOOL value);

    /* LOG FUNCTIONS */

    /* Set or get the current logger. You need to call
     * qpdflogger_cleanup on the logger handles when you are done with
     * the handles. The underlying logger is cleaned up automatically
     * and persists if needed after the logger handle is destroyed.
     * See comments in qpdflogger-c.h for details.
     */

    QPDF_DLL
    void qpdf_set_logger(qpdf_data qpdf, qpdflogger_handle logger);
    QPDF_DLL
    qpdflogger_handle qpdf_get_logger(qpdf_data qpdf);

    /* CHECK FUNCTIONS */

    /* Attempt to read the entire PDF file to see if there are any
     * errors qpdf can detect.
     */
    QPDF_DLL
    QPDF_ERROR_CODE qpdf_check_pdf(qpdf_data qpdf);

    /* READ PARAMETER FUNCTIONS -- must be called before qpdf_read */

    QPDF_DLL
    void qpdf_set_ignore_xref_streams(qpdf_data qpdf, QPDF_BOOL value);

    QPDF_DLL
    void qpdf_set_attempt_recovery(qpdf_data qpdf, QPDF_BOOL value);

    /* PROCESS FUNCTIONS */

    /* This functions process a PDF or JSON input source. */

    /* Calling qpdf_read causes processFile to be called in the C++
     * API.  Basic parsing is performed, but data from the file is
     * only read as needed.  For files without passwords, pass a null
     * pointer or an empty string as the password.
     */
    QPDF_DLL
    QPDF_ERROR_CODE
    qpdf_read(qpdf_data qpdf, char const* filename, char const* password);

    /* Calling qpdf_read_memory causes processMemoryFile to be called
     * in the C++ API.  Otherwise, it behaves in the same way as
     * qpdf_read.  The description argument will be used in place of
     * the file name in any error or warning messages generated by the
     * library.
     */
    QPDF_DLL
    QPDF_ERROR_CODE qpdf_read_memory(
        qpdf_data qpdf,
        char const* description,
        char const* buffer,
        unsigned long long size,
        char const* password);

    /* Calling qpdf_empty_pdf initializes this qpdf object with an
     * empty PDF, making it possible to create a PDF from scratch
     * using the C API. Added in 10.6.
     */
    QPDF_DLL
    QPDF_ERROR_CODE qpdf_empty_pdf(qpdf_data qpdf);

    /* Create a PDF from a JSON file. This calls createFromJSON in the
     * C++ API.
     */
    QPDF_DLL
    QPDF_ERROR_CODE
    qpdf_create_from_json_file(qpdf_data qpdf, char const* filename);

    /* Create a PDF from JSON data in a null-terminated string. This
     * calls createFromJSON in the C++ API.
     */
    QPDF_DLL
    QPDF_ERROR_CODE
    qpdf_create_from_json_data(
        qpdf_data qpdf, char const* buffer, unsigned long long size);

    /* JSON UPDATE FUNCTIONS */

    /* Update a QPDF object from a JSON file or buffer. These
     * functions call updateFromJSON. One of the other processing
     * functions has to be called first so that the QPDF object is
     * initialized with PDF data.
     */
    QPDF_DLL
    QPDF_ERROR_CODE
    qpdf_update_from_json_file(qpdf_data qpdf, char const* filename);
    QPDF_DLL
    QPDF_ERROR_CODE
    qpdf_update_from_json_data(
        qpdf_data qpdf, char const* buffer, unsigned long long size);

    /* READ FUNCTIONS */

    /* Read functions below must be called after qpdf_read or any of
     * the other functions that process a PDF. */

    /*
     * NOTE: Functions that return char* are returning a pointer to an
     * internal buffer that will be reused for each call to a function
     * that returns a char*.  You must use or copy the value before
     * calling any other qpdf library functions.
     */

    /* Return the version of the PDF file.  See warning above about
     * functions that return char*. */
    QPDF_DLL
    char const* qpdf_get_pdf_version(qpdf_data qpdf);

    /* Return the extension level of the PDF file. */
    QPDF_DLL
    int qpdf_get_pdf_extension_level(qpdf_data qpdf);

    /* Return the user password.  If the file is opened using the
     * owner password, the user password may be retrieved using this
     * function.  If the file is opened using the user password, this
     * function will return that user password.  See warning above
     * about functions that return char*.
     */
    QPDF_DLL
    char const* qpdf_get_user_password(qpdf_data qpdf);

    /* Return the string value of a key in the document's Info
     * dictionary.  The key parameter should include the leading
     * slash, e.g. "/Author".  If the key is not present or has a
     * non-string value, a null pointer is returned.  Otherwise, a
     * pointer to an internal buffer is returned.  See warning above
     * about functions that return char*.
     */
    QPDF_DLL
    char const* qpdf_get_info_key(qpdf_data qpdf, char const* key);

    /* Set a value in the info dictionary, possibly replacing an
     * existing value.  The key must include the leading slash
     * (e.g. "/Author").  Passing a null pointer as a value will
     * remove the key from the info dictionary.  Otherwise, a copy
     * will be made of the string that is passed in.
     */
    QPDF_DLL
    void qpdf_set_info_key(qpdf_data qpdf, char const* key, char const* value);

    /* Indicate whether the input file is linearized. */
    QPDF_DLL
    QPDF_BOOL qpdf_is_linearized(qpdf_data qpdf);

    /* Indicate whether the input file is encrypted. */
    QPDF_DLL
    QPDF_BOOL qpdf_is_encrypted(qpdf_data qpdf);

    QPDF_DLL
    QPDF_BOOL qpdf_allow_accessibility(qpdf_data qpdf);
    QPDF_DLL
    QPDF_BOOL qpdf_allow_extract_all(qpdf_data qpdf);
    QPDF_DLL
    QPDF_BOOL qpdf_allow_print_low_res(qpdf_data qpdf);
    QPDF_DLL
    QPDF_BOOL qpdf_allow_print_high_res(qpdf_data qpdf);
    QPDF_DLL
    QPDF_BOOL qpdf_allow_modify_assembly(qpdf_data qpdf);
    QPDF_DLL
    QPDF_BOOL qpdf_allow_modify_form(qpdf_data qpdf);
    QPDF_DLL
    QPDF_BOOL qpdf_allow_modify_annotation(qpdf_data qpdf);
    QPDF_DLL
    QPDF_BOOL qpdf_allow_modify_other(qpdf_data qpdf);
    QPDF_DLL
    QPDF_BOOL qpdf_allow_modify_all(qpdf_data qpdf);

    /* JSON WRITE FUNCTIONS */

    /* This function serializes the PDF to JSON. This calls writeJSON
     * from the C++ API.
     *
     * - version: the JSON version, currently must be 2
     * - fn: a function that will be called with blocks of JSON data;
     *   will be called with data, a length, and the value of the
     *   udata parameter to this function
     * - udata: will be passed as the third argument to fn with each
     *   call; use this for your own tracking or pass a null pointer
     *   if you don't need it
     * - For decode_level, json_stream_data, file_prefix, and
     *   wanted_objects, see comments in QPDF.hh. For this API,
     *   wanted_objects should be a null-terminated array of
     *   null-terminated strings. Pass a null pointer if you want all
     *   objects.
     */

    /* Function should return 0 on success. */
    typedef int (*qpdf_write_fn_t)(char const* data, size_t len, void* udata);

    QPDF_DLL
    QPDF_ERROR_CODE qpdf_write_json(
        qpdf_data qpdf,
        int version,
        qpdf_write_fn_t fn,
        void* udata,
        enum qpdf_stream_decode_level_e decode_level,
        enum qpdf_json_stream_data_e json_stream_data,
        char const* file_prefix,
        char const* const* wanted_objects);

    /* WRITE FUNCTIONS */

    /* Set up for writing.  No writing is actually performed until the
     * call to qpdf_write().
     */

    /* Supply the name of the file to be written and initialize the
     * qpdf_data object to handle writing operations.  This function
     * also attempts to create the file.  The PDF data is not written
     * until the call to qpdf_write.  qpdf_init_write may be called
     * multiple times for the same qpdf_data object.  When
     * qpdf_init_write is called, all information from previous calls
     * to functions that set write parameters (qpdf_set_linearization,
     * etc.) is lost, so any write parameter functions must be called
     * again.
     */
    QPDF_DLL
    QPDF_ERROR_CODE qpdf_init_write(qpdf_data qpdf, char const* filename);

    /* Initialize for writing but indicate that the PDF file should be
     * written to memory.  Call qpdf_get_buffer_length and
     * qpdf_get_buffer to retrieve the resulting buffer.  The memory
     * containing the PDF file will be destroyed when qpdf_cleanup is
     * called.
     */
    QPDF_DLL
    QPDF_ERROR_CODE qpdf_init_write_memory(qpdf_data qpdf);

    /* Retrieve the buffer used if the file was written to memory.
     * qpdf_get_buffer returns a null pointer if data was not written
     * to memory.  The memory is freed when qpdf_cleanup is called or
     * if a subsequent call to qpdf_init_write or
     * qpdf_init_write_memory is called. */
    QPDF_DLL
    size_t qpdf_get_buffer_length(qpdf_data qpdf);
    QPDF_DLL
    unsigned char const* qpdf_get_buffer(qpdf_data qpdf);

    QPDF_DLL
    void
    qpdf_set_object_stream_mode(qpdf_data qpdf, enum qpdf_object_stream_e mode);

    QPDF_DLL
    void
    qpdf_set_stream_data_mode(qpdf_data qpdf, enum qpdf_stream_data_e mode);

    QPDF_DLL
    void qpdf_set_compress_streams(qpdf_data qpdf, QPDF_BOOL value);

    QPDF_DLL
    void qpdf_set_decode_level(
        qpdf_data qpdf, enum qpdf_stream_decode_level_e level);

    QPDF_DLL
    void
    qpdf_set_preserve_unreferenced_objects(qpdf_data qpdf, QPDF_BOOL value);

    QPDF_DLL
    void qpdf_set_newline_before_endstream(qpdf_data qpdf, QPDF_BOOL value);

    QPDF_DLL
    void qpdf_set_content_normalization(qpdf_data qpdf, QPDF_BOOL value);

    QPDF_DLL
    void qpdf_set_qdf_mode(qpdf_data qpdf, QPDF_BOOL value);

    QPDF_DLL
    void qpdf_set_deterministic_ID(qpdf_data qpdf, QPDF_BOOL value);

    /* Never use qpdf_set_static_ID except in test suites to suppress
     * generation of a random /ID.  See also qpdf_set_deterministic_ID.
     */
    QPDF_DLL
    void qpdf_set_static_ID(qpdf_data qpdf, QPDF_BOOL value);

    /* Never use qpdf_set_static_aes_IV except in test suites to
     * create predictable AES encrypted output.
     */
    QPDF_DLL
    void qpdf_set_static_aes_IV(qpdf_data qpdf, QPDF_BOOL value);

    QPDF_DLL
    void qpdf_set_suppress_original_object_IDs(qpdf_data qpdf, QPDF_BOOL value);

    QPDF_DLL
    void qpdf_set_preserve_encryption(qpdf_data qpdf, QPDF_BOOL value);

    /* The *_insecure functions are identical to the old versions but
     * have been renamed as a an alert to the caller that they are
     * insecure. See "Weak Cryptographic" in the manual for
     * details.
     */
    QPDF_DLL
    void qpdf_set_r2_encryption_parameters_insecure(
        qpdf_data qpdf,
        char const* user_password,
        char const* owner_password,
        QPDF_BOOL allow_print,
        QPDF_BOOL allow_modify,
        QPDF_BOOL allow_extract,
        QPDF_BOOL allow_annotate);

    QPDF_DLL
    void qpdf_set_r3_encryption_parameters_insecure(
        qpdf_data qpdf,
        char const* user_password,
        char const* owner_password,
        QPDF_BOOL allow_accessibility,
        QPDF_BOOL allow_extract,
        QPDF_BOOL allow_assemble,
        QPDF_BOOL allow_annotate_and_form,
        QPDF_BOOL allow_form_filling,
        QPDF_BOOL allow_modify_other,
        enum qpdf_r3_print_e print);

    QPDF_DLL
    void qpdf_set_r4_encryption_parameters_insecure(
        qpdf_data qpdf,
        char const* user_password,
        char const* owner_password,
        QPDF_BOOL allow_accessibility,
        QPDF_BOOL allow_extract,
        QPDF_BOOL allow_assemble,
        QPDF_BOOL allow_annotate_and_form,
        QPDF_BOOL allow_form_filling,
        QPDF_BOOL allow_modify_other,
        enum qpdf_r3_print_e print,
        QPDF_BOOL encrypt_metadata,
        QPDF_BOOL use_aes);

    QPDF_DLL
    void qpdf_set_r5_encryption_parameters2(
        qpdf_data qpdf,
        char const* user_password,
        char const* owner_password,
        QPDF_BOOL allow_accessibility,
        QPDF_BOOL allow_extract,
        QPDF_BOOL allow_assemble,
        QPDF_BOOL allow_annotate_and_form,
        QPDF_BOOL allow_form_filling,
        QPDF_BOOL allow_modify_other,
        enum qpdf_r3_print_e print,
        QPDF_BOOL encrypt_metadata);

    QPDF_DLL
    void qpdf_set_r6_encryption_parameters2(
        qpdf_data qpdf,
        char const* user_password,
        char const* owner_password,
        QPDF_BOOL allow_accessibility,
        QPDF_BOOL allow_extract,
        QPDF_BOOL allow_assemble,
        QPDF_BOOL allow_annotate_and_form,
        QPDF_BOOL allow_form_filling,
        QPDF_BOOL allow_modify_other,
        enum qpdf_r3_print_e print,
        QPDF_BOOL encrypt_metadata);

    QPDF_DLL
    void qpdf_set_linearization(qpdf_data qpdf, QPDF_BOOL value);

    QPDF_DLL
    void qpdf_set_minimum_pdf_version(qpdf_data qpdf, char const* version);

    QPDF_DLL
    void qpdf_set_minimum_pdf_version_and_extension(
        qpdf_data qpdf, char const* version, int extension_level);

    QPDF_DLL
    void qpdf_force_pdf_version(qpdf_data qpdf, char const* version);

    QPDF_DLL
    void qpdf_force_pdf_version_and_extension(
        qpdf_data qpdf, char const* version, int extension_level);

    /* During write, your report_progress function will be called with
     * a value between 0 and 100 representing the approximate write
     * progress. The data object you pass to
     * qpdf_register_progress_reporter will be handed back to your
     * function. This function must be called after qpdf_init_write
     * (or qpdf_init_write_memory) and before qpdf_write. The
     * registered progress reporter applies only to a single write, so
     * you must call it again if you perform a subsequent write with a
     * new writer.
     */
    QPDF_DLL
    void qpdf_register_progress_reporter(
        qpdf_data qpdf,
        void (*report_progress)(int percent, void* data),
        void* data);

    /* Do actual write operation. */
    QPDF_DLL
    QPDF_ERROR_CODE qpdf_write(qpdf_data qpdf);

    /* Object handling.
     *
     * These functions take and return a qpdf_oh object handle, which
     * is just an unsigned integer. The value 0 is never returned, which
     * makes it usable as an uninitialized value. The handles returned by
     * these functions are guaranteed to be unique, i.e. two calls to
     * (the same of different) functions will return distinct handles
     * even when they refer to the same object.
     *
     * Each function below, starting with qpdf_oh, corresponds to a
     * specific method of QPDFObjectHandler. For example,
     * qpdf_oh_is_bool corresponds to QPDFObjectHandle::isBool. If the
     * C++ method is overloaded, the C function's name will be
     * disambiguated. If the C++ method takes optional arguments, the C
     * function will have required arguments in those positions. For
     * details about the method, please see comments in
     * QPDFObjectHandle.hh. Comments here only explain things that are
     * specific to the "C" API.
     *
     * Only a fraction of the methods of QPDFObjectHandle are
     * available here. Most of the basic methods for creating,
     * accessing, and modifying most types of objects are present.
     * Most of the higher-level functions are not implemented.
     * Functions for dealing with content streams as well as objects
     * that only exist in content streams (operators and inline
     * images) are mostly not provided.
     *
     * To refer to a specific QPDFObjectHandle, you need a pair
     * consisting of a qpdf_data and a qpdf_oh, which is just an index
     * into an internal table of objects. All memory allocated by any
     * of these functions is returned when qpdf_cleanup is called.
     *
     * Regarding memory, the same rules apply as the above functions.
     * Specifically, if a function returns a char*, the memory is
     * managed by the library and, unless otherwise specified, is not
     * expected to be valid after the next qpdf call.
     *
     * The qpdf_data object keeps a cache of handles returned by these
     * functions. Once you are finished referencing a handle, you can
     * optionally release it. Releasing handles is optional since they
     * will all get released by qpdf_cleanup, but it can help to
     * reduce the memory footprint of the qpdf_data object to release
     * them when you're done. Releasing a handle does not destroy the
     * object. All QPDFObjectHandle objects are deleted when they are
     * no longer referenced. Releasing an object handle simply
     * invalidates it. For example, if you create an object,
     * add it to an existing dictionary or array, and then release its
     * handle, the object is safely part of the dictionary or array.
     * Similarly, any other object handle refering to the object remains
     * valid. Explicitly releasing an object handle is essentially the
     * same as letting a QPDFObjectHandle go out of scope in the C++
     * API.
     *
     * Please see "ERROR HANDLING" above for details on how error
     * conditions are handled.
     */

    /* For examples of using this API, see examples/pdf-c-objects.c */

    typedef unsigned int qpdf_oh;

    /* Releasing objects -- see comments above. These functions have no
     * equivalent in the C++ API.
     */
    QPDF_DLL
    void qpdf_oh_release(qpdf_data qpdf, qpdf_oh oh);
    QPDF_DLL
    void qpdf_oh_release_all(qpdf_data qpdf);

    /* Clone an object handle */
    QPDF_DLL
    qpdf_oh qpdf_oh_new_object(qpdf_data qpdf, qpdf_oh oh);

    /* Get trailer and root objects */
    QPDF_DLL
    qpdf_oh qpdf_get_trailer(qpdf_data qpdf);
    QPDF_DLL
    qpdf_oh qpdf_get_root(qpdf_data qpdf);

    /* Retrieve and replace indirect objects */
    QPDF_DLL
    qpdf_oh qpdf_get_object_by_id(qpdf_data qpdf, int objid, int generation);
    QPDF_DLL
    qpdf_oh qpdf_make_indirect_object(qpdf_data qpdf, qpdf_oh oh);
    QPDF_DLL
    void
    qpdf_replace_object(qpdf_data qpdf, int objid, int generation, qpdf_oh oh);

    /* Wrappers around QPDFObjectHandle methods. Be sure to read
     * corresponding comments in QPDFObjectHandle.hh to understand
     * what each function does and what kinds of objects it applies
     * to. Note that names are to appear in a canonicalized form
     * starting with a leading slash and with all PDF escaping
     * resolved. See comments for getName() in QPDFObjectHandle.hh for
     * details.
     */

    QPDF_DLL
    QPDF_BOOL qpdf_oh_is_initialized(qpdf_data qpdf, qpdf_oh oh);
    QPDF_DLL
    QPDF_BOOL qpdf_oh_is_bool(qpdf_data qpdf, qpdf_oh oh);
    QPDF_DLL
    QPDF_BOOL qpdf_oh_is_null(qpdf_data qpdf, qpdf_oh oh);
    QPDF_DLL
    QPDF_BOOL qpdf_oh_is_integer(qpdf_data qpdf, qpdf_oh oh);
    QPDF_DLL
    QPDF_BOOL qpdf_oh_is_real(qpdf_data qpdf, qpdf_oh oh);
    QPDF_DLL
    QPDF_BOOL qpdf_oh_is_name(qpdf_data qpdf, qpdf_oh oh);
    QPDF_DLL
    QPDF_BOOL qpdf_oh_is_string(qpdf_data qpdf, qpdf_oh oh);
    QPDF_DLL
    QPDF_BOOL qpdf_oh_is_operator(qpdf_data qpdf, qpdf_oh oh);
    QPDF_DLL
    QPDF_BOOL qpdf_oh_is_inline_image(qpdf_data qpdf, qpdf_oh oh);
    QPDF_DLL
    QPDF_BOOL qpdf_oh_is_array(qpdf_data qpdf, qpdf_oh oh);
    QPDF_DLL
    QPDF_BOOL qpdf_oh_is_dictionary(qpdf_data qpdf, qpdf_oh oh);
    QPDF_DLL
    QPDF_BOOL qpdf_oh_is_stream(qpdf_data qpdf, qpdf_oh oh);
    QPDF_DLL
    QPDF_BOOL qpdf_oh_is_indirect(qpdf_data qpdf, qpdf_oh oh);
    QPDF_DLL
    QPDF_BOOL qpdf_oh_is_scalar(qpdf_data qpdf, qpdf_oh oh);

    QPDF_DLL
    QPDF_BOOL
    qpdf_oh_is_name_and_equals(qpdf_data qpdf, qpdf_oh oh, char const* name);

    QPDF_DLL
    QPDF_BOOL qpdf_oh_is_dictionary_of_type(
        qpdf_data qpdf, qpdf_oh oh, char const* type, char const* subtype);

    QPDF_DLL
    enum qpdf_object_type_e qpdf_oh_get_type_code(qpdf_data qpdf, qpdf_oh oh);
    QPDF_DLL
    char const* qpdf_oh_get_type_name(qpdf_data qpdf, qpdf_oh oh);

    QPDF_DLL
    qpdf_oh qpdf_oh_wrap_in_array(qpdf_data qpdf, qpdf_oh oh);

    QPDF_DLL
    qpdf_oh qpdf_oh_parse(qpdf_data qpdf, char const* object_str);

    QPDF_DLL
    QPDF_BOOL qpdf_oh_get_bool_value(qpdf_data qpdf, qpdf_oh oh);
    QPDF_DLL
    QPDF_BOOL
    qpdf_oh_get_value_as_bool(qpdf_data qpdf, qpdf_oh oh, QPDF_BOOL* value);

    QPDF_DLL
    long long qpdf_oh_get_int_value(qpdf_data qpdf, qpdf_oh oh);
    QPDF_DLL
    QPDF_BOOL
    qpdf_oh_get_value_as_longlong(qpdf_data qpdf, qpdf_oh oh, long long* value);
    QPDF_DLL
    int qpdf_oh_get_int_value_as_int(qpdf_data qpdf, qpdf_oh oh);
    QPDF_DLL
    QPDF_BOOL qpdf_oh_get_value_as_int(qpdf_data qpdf, qpdf_oh oh, int* value);
    QPDF_DLL
    unsigned long long qpdf_oh_get_uint_value(qpdf_data qpdf, qpdf_oh oh);
    QPDF_DLL
    QPDF_BOOL qpdf_oh_get_value_as_ulonglong(
        qpdf_data qpdf, qpdf_oh oh, unsigned long long* value);
    QPDF_DLL
    unsigned int qpdf_oh_get_uint_value_as_uint(qpdf_data qpdf, qpdf_oh oh);
    QPDF_DLL
    QPDF_BOOL
    qpdf_oh_get_value_as_uint(qpdf_data qpdf, qpdf_oh oh, unsigned int* value);

    QPDF_DLL
    char const* qpdf_oh_get_real_value(qpdf_data qpdf, qpdf_oh oh);
    QPDF_DLL
    QPDF_BOOL qpdf_oh_get_value_as_real(
        qpdf_data qpdf, qpdf_oh oh, char const** value, size_t* length);

    QPDF_DLL
    QPDF_BOOL qpdf_oh_is_number(qpdf_data qpdf, qpdf_oh oh);
    QPDF_DLL
    double qpdf_oh_get_numeric_value(qpdf_data qpdf, qpdf_oh oh);
    QPDF_DLL
    QPDF_BOOL
    qpdf_oh_get_value_as_number(qpdf_data qpdf, qpdf_oh oh, double* value);

    QPDF_DLL
    char const* qpdf_oh_get_name(qpdf_data qpdf, qpdf_oh oh);
    QPDF_DLL
    QPDF_BOOL qpdf_oh_get_value_as_name(
        qpdf_data qpdf, qpdf_oh oh, char const** value, size_t* length);

    /* Return the length of the last string returned. This enables you
     * to retrieve the entire string for cases in which a char*
     * returned by one of the functions below points to a string with
     * embedded null characters. The function
     * qpdf_oh_get_binary_string_value takes a length pointer, which
     * can be useful if you are retrieving the value of a string that
     * is expected to contain binary data, such as a checksum or
     * document ID. It is always valid to call
     * qpdf_get_last_string_length, but it is usually not necessary as
     * C strings returned by the library are only expected to be able
     * to contain null characters if their values originate from PDF
     * strings in the input.
     */
    QPDF_DLL
    size_t qpdf_get_last_string_length(qpdf_data qpdf);

    QPDF_DLL
    char const* qpdf_oh_get_string_value(qpdf_data qpdf, qpdf_oh oh);
    QPDF_DLL
    QPDF_BOOL qpdf_oh_get_value_as_string(
        qpdf_data qpdf, qpdf_oh oh, char const** value, size_t* length);
    QPDF_DLL
    char const* qpdf_oh_get_utf8_value(qpdf_data qpdf, qpdf_oh oh);
    QPDF_DLL
    QPDF_BOOL qpdf_oh_get_value_as_utf8(
        qpdf_data qpdf, qpdf_oh oh, char const** value, size_t* length);
    QPDF_DLL
    char const*
    qpdf_oh_get_binary_string_value(qpdf_data qpdf, qpdf_oh oh, size_t* length);
    QPDF_DLL
    char const*
    qpdf_oh_get_binary_utf8_value(qpdf_data qpdf, qpdf_oh oh, size_t* length);

    QPDF_DLL
    int qpdf_oh_get_array_n_items(qpdf_data qpdf, qpdf_oh oh);
    QPDF_DLL
    qpdf_oh qpdf_oh_get_array_item(qpdf_data qpdf, qpdf_oh oh, int n);

    /* In all dictionary APIs, keys are specified/represented as
     * canonicalized name strings starting with / and with all PDF
     * escaping resolved. See comments for getName() in
     * QPDFObjectHandle for details.
     */

    /* "C"-specific dictionary key iteration */

    /* Iteration is allowed on only one dictionary at a time. */
    QPDF_DLL
    void qpdf_oh_begin_dict_key_iter(qpdf_data qpdf, qpdf_oh dict);
    QPDF_DLL
    QPDF_BOOL qpdf_oh_dict_more_keys(qpdf_data qpdf);
    /* The memory returned by qpdf_oh_dict_next_key is owned by
     * qpdf_data. It is good until the next call to
     * qpdf_oh_dict_next_key with the same qpdf_data object. Calling
     * the function again, even with a different dict, invalidates
     * previous return values.
     */
    QPDF_DLL
    char const* qpdf_oh_dict_next_key(qpdf_data qpdf);

    /* end "C"-specific dictionary key iteration */

    QPDF_DLL
    QPDF_BOOL qpdf_oh_has_key(qpdf_data qpdf, qpdf_oh oh, char const* key);
    QPDF_DLL
    qpdf_oh qpdf_oh_get_key(qpdf_data qpdf, qpdf_oh oh, char const* key);
    QPDF_DLL
    qpdf_oh
    qpdf_oh_get_key_if_dict(qpdf_data qpdf, qpdf_oh oh, char const* key);

    QPDF_DLL
    QPDF_BOOL
    qpdf_oh_is_or_has_name(qpdf_data qpdf, qpdf_oh oh, char const* key);

    QPDF_DLL
    qpdf_oh qpdf_oh_new_uninitialized(qpdf_data qpdf);
    QPDF_DLL
    qpdf_oh qpdf_oh_new_null(qpdf_data qpdf);
    QPDF_DLL
    qpdf_oh qpdf_oh_new_bool(qpdf_data qpdf, QPDF_BOOL value);
    QPDF_DLL
    qpdf_oh qpdf_oh_new_integer(qpdf_data qpdf, long long value);
    QPDF_DLL
    qpdf_oh qpdf_oh_new_real_from_string(qpdf_data qpdf, char const* value);
    QPDF_DLL
    qpdf_oh qpdf_oh_new_real_from_double(
        qpdf_data qpdf, double value, int decimal_places);
    QPDF_DLL
    qpdf_oh qpdf_oh_new_name(qpdf_data qpdf, char const* name);
    QPDF_DLL
    qpdf_oh qpdf_oh_new_string(qpdf_data qpdf, char const* str);
    QPDF_DLL
    qpdf_oh qpdf_oh_new_unicode_string(qpdf_data qpdf, char const* utf8_str);
    /* Use qpdf_oh_new_binary_string for creating a string that may
     * contain atrbitary binary data including embedded null characters.
     */
    QPDF_DLL
    qpdf_oh
    qpdf_oh_new_binary_string(qpdf_data qpdf, char const* str, size_t length);
    QPDF_DLL
    qpdf_oh qpdf_oh_new_binary_unicode_string(
        qpdf_data qpdf, char const* str, size_t length);
    QPDF_DLL
    qpdf_oh qpdf_oh_new_array(qpdf_data qpdf);
    QPDF_DLL
    qpdf_oh qpdf_oh_new_dictionary(qpdf_data qpdf);

    /* Create a new stream. Use qpdf_oh_get_dict to get (and
     * subsequently modify) the stream dictionary if needed. See
     * comments in QPDFObjectHandle.hh for newStream() for additional
     * notes. You must call qpdf_oh_replace_stream_data to provide
     * data for the stream. See STREAM FUNCTIONS below.
     */
    QPDF_DLL
    qpdf_oh qpdf_oh_new_stream(qpdf_data qpdf);

    QPDF_DLL
    void qpdf_oh_make_direct(qpdf_data qpdf, qpdf_oh oh);

    QPDF_DLL
    void
    qpdf_oh_set_array_item(qpdf_data qpdf, qpdf_oh oh, int at, qpdf_oh item);
    QPDF_DLL
    void qpdf_oh_insert_item(qpdf_data qpdf, qpdf_oh oh, int at, qpdf_oh item);
    QPDF_DLL
    void qpdf_oh_append_item(qpdf_data qpdf, qpdf_oh oh, qpdf_oh item);
    QPDF_DLL
    void qpdf_oh_erase_item(qpdf_data qpdf, qpdf_oh oh, int at);

    QPDF_DLL
    void qpdf_oh_replace_key(
        qpdf_data qpdf, qpdf_oh oh, char const* key, qpdf_oh item);
    QPDF_DLL
    void qpdf_oh_remove_key(qpdf_data qpdf, qpdf_oh oh, char const* key);
    QPDF_DLL
    void qpdf_oh_replace_or_remove_key(
        qpdf_data qpdf, qpdf_oh oh, char const* key, qpdf_oh item);

    QPDF_DLL
    qpdf_oh qpdf_oh_get_dict(qpdf_data qpdf, qpdf_oh oh);

    QPDF_DLL
    int qpdf_oh_get_object_id(qpdf_data qpdf, qpdf_oh oh);
    QPDF_DLL
    int qpdf_oh_get_generation(qpdf_data qpdf, qpdf_oh oh);

    QPDF_DLL
    char const* qpdf_oh_unparse(qpdf_data qpdf, qpdf_oh oh);
    QPDF_DLL
    char const* qpdf_oh_unparse_resolved(qpdf_data qpdf, qpdf_oh oh);
    QPDF_DLL
    char const* qpdf_oh_unparse_binary(qpdf_data qpdf, qpdf_oh oh);

    /* Note about foreign objects: the C API does not have enough
     * information in the value of a qpdf_oh to know what QPDF object
     * it belongs to. To uniquely specify a qpdf object handle from a
     * specific qpdf_data instance, you always pair the qpdf_oh with
     * the correct qpdf_data. Otherwise, you are likely to get
     * completely the wrong object if you are not lucky enough to get
     * an error about the object being invalid.
     */

    /* Copy foreign object: the qpdf_oh returned belongs to `qpdf`,
     * while `foreign_oh` belongs to `other_qpdf`.
     */
    QPDF_DLL
    qpdf_oh qpdf_oh_copy_foreign_object(
        qpdf_data qpdf, qpdf_data other_qpdf, qpdf_oh foreign_oh);

    /* STREAM FUNCTIONS */

    /* These functions provide basic access to streams and stream
     * data. They are not as comprehensive as what is in
     * QPDFObjectHandle, but they do allow for working with streams
     * and stream data as caller-managed memory.
     */

    /* Get stream data as a buffer. The buffer is allocated with
     * malloc and must be freed by the caller. The size of the buffer
     * is stored in *len. The arguments are similar to those in
     * QPDFObjectHandle::pipeStreamData. To get raw stream data, pass
     * qpdf_dl_none as decode_level. Otherwise, filtering is attempted
     * and *filtered is set to indicate whether it was successful. If
     * *filtered is QPDF_FALSE, then raw, unfiltered stream data was
     * returned. You may pass a null pointer as filtered if you don't
     * care about the result. If you pass a null pointer as bufp (and
     * len), the value of filtered will be set to whether the stream
     * can be filterable.
     */
    QPDF_DLL
    QPDF_ERROR_CODE qpdf_oh_get_stream_data(
        qpdf_data qpdf,
        qpdf_oh stream_oh,
        enum qpdf_stream_decode_level_e decode_level,
        QPDF_BOOL* filtered,
        unsigned char** bufp,
        size_t* len);

    /* This function returns the concatenation of all of a page's
     * content streams as a single, dynamically allocated buffer. As
     * with qpdf_oh_get_stream_data, the buffer is allocated with
     * malloc and must be freed by the caller.
     */
    QPDF_DLL
    QPDF_ERROR_CODE qpdf_oh_get_page_content_data(
        qpdf_data qpdf, qpdf_oh page_oh, unsigned char** bufp, size_t* len);

    /* The data pointed to by bufp will be copied by the library. It
     * does not need to remain valid after the call returns.
     */
    QPDF_DLL
    void qpdf_oh_replace_stream_data(
        qpdf_data qpdf,
        qpdf_oh stream_oh,
        unsigned char const* buf,
        size_t len,
        qpdf_oh filter,
        qpdf_oh decode_parms);

    /* PAGE FUNCTIONS */

    /* The first time a page function is called, qpdf will traverse
     * the /Pages tree. Subsequent calls to retrieve the number of
     * pages or a specific page run in constant time as they are
     * accessing the pages cache. If you manipulate the page tree
     * outside of these functions, you should call
     * qpdf_update_all_pages_cache. See comments for getAllPages() and
     * updateAllPagesCache() in QPDF.hh.
     */

    /* For each function, the corresponding method in QPDF.hh is
     * referenced. Please see comments in QPDF.hh for details.
     */

    /* calls getAllPages(). On error, returns -1 and sets error for
     * qpdf_get_error. */
    QPDF_DLL
    int qpdf_get_num_pages(qpdf_data qpdf);
    /* returns uninitialized object if out of range */
    QPDF_DLL
    qpdf_oh qpdf_get_page_n(qpdf_data qpdf, size_t zero_based_index);

    /* updateAllPagesCache() */
    QPDF_DLL
    QPDF_ERROR_CODE qpdf_update_all_pages_cache(qpdf_data qpdf);

    /* findPage() -- return zero-based index. If page is not found,
     * return -1 and save the error to be retrieved with
     * qpdf_get_error.
     */
    QPDF_DLL
    int qpdf_find_page_by_id(qpdf_data qpdf, int objid, int generation);
    QPDF_DLL
    int qpdf_find_page_by_oh(qpdf_data qpdf, qpdf_oh oh);

    /* pushInheritedAttributesToPage() */
    QPDF_DLL
    QPDF_ERROR_CODE qpdf_push_inherited_attributes_to_page(qpdf_data qpdf);

    /* Functions that add pages may add pages from other files. If
     * adding a page from the same file, newpage_qpdf and qpdf are the
     * same.
     /*/

    /* addPage() */
    QPDF_DLL
    QPDF_ERROR_CODE qpdf_add_page(
        qpdf_data qpdf,
        qpdf_data newpage_qpdf,
        qpdf_oh newpage,
        QPDF_BOOL first);
    /* addPageAt() */
    QPDF_DLL
    QPDF_ERROR_CODE qpdf_add_page_at(
        qpdf_data qpdf,
        qpdf_data newpage_qpdf,
        qpdf_oh newpage,
        QPDF_BOOL before,
        qpdf_oh refpage);
    /* removePage() */
    QPDF_DLL
    QPDF_ERROR_CODE qpdf_remove_page(qpdf_data qpdf, qpdf_oh page);
#ifdef __cplusplus
}
#endif

#endif /* QPDF_C_H */
