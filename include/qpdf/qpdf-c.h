/* Copyright (c) 2005-2021 Jay Berkenbilt
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

#include <qpdf/DLL.h>
#include <qpdf/Types.h>
#include <qpdf/Constants.h>
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
#   define QPDF_SUCCESS 0
#   define QPDF_WARNINGS 1 << 0
#   define QPDF_ERRORS 1 << 1

    typedef int QPDF_BOOL;
#   define QPDF_TRUE 1
#   define QPDF_FALSE 0

    /* Returns the version of the qpdf software */
    QPDF_DLL
    char const* qpdf_get_qpdf_version();

    /* Returns dynamically allocated qpdf_data pointer; must be freed
     * by calling qpdf_cleanup.
     */
    QPDF_DLL
    qpdf_data qpdf_init();

    /* Pass a pointer to the qpdf_data pointer created by qpdf_init to
     * clean up resources.
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
     * this function, qpdf_next_warning, or qpdf_destroy.  After this
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

    /* CHECK FUNCTIONS */

    /* Attempt to read the entire PDF file to see if there are any
     * errors qpdf can detect.
     */
    QPDF_DLL
    QPDF_ERROR_CODE qpdf_check_pdf(qpdf_data qpdf);

    /* READ FUNCTIONS */

    /* READ PARAMETER FUNCTIONS -- must be called before qpdf_read */

    QPDF_DLL
    void qpdf_set_ignore_xref_streams(qpdf_data qpdf, QPDF_BOOL value);

    QPDF_DLL
    void qpdf_set_attempt_recovery(qpdf_data qpdf, QPDF_BOOL value);

    /* Calling qpdf_read causes processFile to be called in the C++
     * API.  Basic parsing is performed, but data from the file is
     * only read as needed.  For files without passwords, pass a null
     * pointer as the password.
     */
    QPDF_DLL
    QPDF_ERROR_CODE qpdf_read(qpdf_data qpdf, char const* filename,
			      char const* password);

    /* Calling qpdf_read_memory causes processMemoryFile to be called
     * in the C++ API.  Otherwise, it behaves in the same way as
     * qpdf_read.  The description argument will be used in place of
     * the file name in any error or warning messages generated by the
     * library.
     */
    QPDF_DLL
    QPDF_ERROR_CODE qpdf_read_memory(qpdf_data qpdf,
				     char const* description,
				     char const* buffer,
				     unsigned long long size,
				     char const* password);

    /* Read functions below must be called after qpdf_read or
     * qpdf_read_memory. */

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
    void qpdf_set_object_stream_mode(qpdf_data qpdf,
				     enum qpdf_object_stream_e mode);

    QPDF_DLL
    void qpdf_set_stream_data_mode(qpdf_data qpdf,
				   enum qpdf_stream_data_e mode);

    QPDF_DLL
    void qpdf_set_compress_streams(qpdf_data qpdf, QPDF_BOOL value);


    QPDF_DLL
    void qpdf_set_decode_level(qpdf_data qpdf,
                               enum qpdf_stream_decode_level_e level);

    QPDF_DLL
    void qpdf_set_preserve_unreferenced_objects(
        qpdf_data qpdf, QPDF_BOOL value);

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
    void qpdf_set_suppress_original_object_IDs(
	qpdf_data qpdf, QPDF_BOOL value);

    QPDF_DLL
    void qpdf_set_preserve_encryption(qpdf_data qpdf, QPDF_BOOL value);

    QPDF_DLL
    void qpdf_set_r2_encryption_parameters(
	qpdf_data qpdf, char const* user_password, char const* owner_password,
	QPDF_BOOL allow_print, QPDF_BOOL allow_modify,
	QPDF_BOOL allow_extract, QPDF_BOOL allow_annotate);

    QPDF_DLL
    void qpdf_set_r3_encryption_parameters2(
	qpdf_data qpdf, char const* user_password, char const* owner_password,
	QPDF_BOOL allow_accessibility, QPDF_BOOL allow_extract,
        QPDF_BOOL allow_assemble, QPDF_BOOL allow_annotate_and_form,
        QPDF_BOOL allow_form_filling, QPDF_BOOL allow_modify_other,
	enum qpdf_r3_print_e print);

    QPDF_DLL
    void qpdf_set_r4_encryption_parameters2(
	qpdf_data qpdf, char const* user_password, char const* owner_password,
	QPDF_BOOL allow_accessibility, QPDF_BOOL allow_extract,
        QPDF_BOOL allow_assemble, QPDF_BOOL allow_annotate_and_form,
        QPDF_BOOL allow_form_filling, QPDF_BOOL allow_modify_other,
	enum qpdf_r3_print_e print,
        QPDF_BOOL encrypt_metadata, QPDF_BOOL use_aes);

    QPDF_DLL
    void qpdf_set_r5_encryption_parameters2(
	qpdf_data qpdf, char const* user_password, char const* owner_password,
	QPDF_BOOL allow_accessibility, QPDF_BOOL allow_extract,
        QPDF_BOOL allow_assemble, QPDF_BOOL allow_annotate_and_form,
        QPDF_BOOL allow_form_filling, QPDF_BOOL allow_modify_other,
	enum qpdf_r3_print_e print, QPDF_BOOL encrypt_metadata);

    QPDF_DLL
    void qpdf_set_r6_encryption_parameters2(
	qpdf_data qpdf, char const* user_password, char const* owner_password,
	QPDF_BOOL allow_accessibility, QPDF_BOOL allow_extract,
        QPDF_BOOL allow_assemble, QPDF_BOOL allow_annotate_and_form,
        QPDF_BOOL allow_form_filling, QPDF_BOOL allow_modify_other,
	enum qpdf_r3_print_e print, QPDF_BOOL encrypt_metadata);

    /* Pre 8.4.0 encryption API */
    QPDF_DLL
    void qpdf_set_r3_encryption_parameters(
	qpdf_data qpdf, char const* user_password, char const* owner_password,
	QPDF_BOOL allow_accessibility, QPDF_BOOL allow_extract,
	enum qpdf_r3_print_e print, enum qpdf_r3_modify_e modify);

    QPDF_DLL
    void qpdf_set_r4_encryption_parameters(
	qpdf_data qpdf, char const* user_password, char const* owner_password,
	QPDF_BOOL allow_accessibility, QPDF_BOOL allow_extract,
	enum qpdf_r3_print_e print, enum qpdf_r3_modify_e modify,
	QPDF_BOOL encrypt_metadata, QPDF_BOOL use_aes);

    QPDF_DLL
    void qpdf_set_r5_encryption_parameters(
	qpdf_data qpdf, char const* user_password, char const* owner_password,
	QPDF_BOOL allow_accessibility, QPDF_BOOL allow_extract,
	enum qpdf_r3_print_e print, enum qpdf_r3_modify_e modify,
	QPDF_BOOL encrypt_metadata);

    QPDF_DLL
    void qpdf_set_r6_encryption_parameters(
	qpdf_data qpdf, char const* user_password, char const* owner_password,
	QPDF_BOOL allow_accessibility, QPDF_BOOL allow_extract,
	enum qpdf_r3_print_e print, enum qpdf_r3_modify_e modify,
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
     * These methods take and return a qpdf_oh, which is just an
     * unsigned integer. The value 0 is never returned, which makes it
     * usable as an uninitialized value.
     *
     * Each function below, starting with qpdf_oh, corresponds to a
     * specific method of QPDFObjectHandler. For example,
     * qpdf_oh_is_bool corresponds to QPDFObjectHandle::isBool. If the
     * C++ method is overloaded, the C function's name will be
     * disambiguated. If the C++ method takes optional argumens, the C
     * method will have required arguments in those positions. For
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
     * of these methods is returned when qpdf_cleanup is called.
     *
     * Regarding memory, the same rules apply as the above functions.
     * Specifically, if a method returns a char*, the memory is
     * managed by the library and, unless otherwise specified, is not
     * expected to be valid after the next qpdf call.
     *
     * The qpdf_data object keeps a cache of objects returned by these
     * methods. Once you are finished referencing an object, you can
     * optionally release it. Releasing objects is optional since they
     * will all get released by qpdf_cleanup, but it can help to
     * reduce the memory footprint of the qpdf_data object to release
     * them when you're done. Releasing an object does not destroy the
     * object. All QPDFObjectHandle objects are deleted when they are
     * no longer referenced. Releasing an object simply invalidates
     * the qpdf_oh handle to it. For example, if you create an object,
     * add it to an existing dictionary or array, and then release it,
     * the object is safely part of the dictionary or array.
     * Explicitly releasing an object is essentially the same as
     * letting a QPDFObjectHandle go out of scope in the C++ API.
     */

    /* For examples of using this API, see examples/pdf-c-objects.c */

    typedef unsigned int qpdf_oh;

    /* Releasing objects -- see comments above. These methods have no
     * equivalent in the C++ API.
     */
    QPDF_DLL
    void qpdf_oh_release(qpdf_data data, qpdf_oh oh);
    QPDF_DLL
    void qpdf_oh_release_all(qpdf_data data);

    /* Get trailer and root objects */
    QPDF_DLL
    qpdf_oh qpdf_get_trailer(qpdf_data data);
    QPDF_DLL
    qpdf_oh qpdf_get_root(qpdf_data data);

    /* Wrappers around QPDFObjectHandle methods. Be sure to read
     * corresponding comments in QPDFObjectHandle.hh to understand
     * what each function does and what kinds of objects it applies
     * to.
     */

    QPDF_DLL
    QPDF_BOOL qpdf_oh_is_bool(qpdf_data data, qpdf_oh oh);
    QPDF_DLL
    QPDF_BOOL qpdf_oh_is_null(qpdf_data data, qpdf_oh oh);
    QPDF_DLL
    QPDF_BOOL qpdf_oh_is_integer(qpdf_data data, qpdf_oh oh);
    QPDF_DLL
    QPDF_BOOL qpdf_oh_is_real(qpdf_data data, qpdf_oh oh);
    QPDF_DLL
    QPDF_BOOL qpdf_oh_is_name(qpdf_data data, qpdf_oh oh);
    QPDF_DLL
    QPDF_BOOL qpdf_oh_is_string(qpdf_data data, qpdf_oh oh);
    QPDF_DLL
    QPDF_BOOL qpdf_oh_is_operator(qpdf_data data, qpdf_oh oh);
    QPDF_DLL
    QPDF_BOOL qpdf_oh_is_inline_image(qpdf_data data, qpdf_oh oh);
    QPDF_DLL
    QPDF_BOOL qpdf_oh_is_array(qpdf_data data, qpdf_oh oh);
    QPDF_DLL
    QPDF_BOOL qpdf_oh_is_dictionary(qpdf_data data, qpdf_oh oh);
    QPDF_DLL
    QPDF_BOOL qpdf_oh_is_stream(qpdf_data data, qpdf_oh oh);
    QPDF_DLL
    QPDF_BOOL qpdf_oh_is_indirect(qpdf_data data, qpdf_oh oh);
    QPDF_DLL
    QPDF_BOOL qpdf_oh_is_scalar(qpdf_data data, qpdf_oh oh);

    QPDF_DLL
    qpdf_oh qpdf_oh_wrap_in_array(qpdf_data data, qpdf_oh oh);

    QPDF_DLL
    qpdf_oh qpdf_oh_parse(qpdf_data data, char const* object_str);

    QPDF_DLL
    QPDF_BOOL qpdf_oh_get_bool_value(qpdf_data data, qpdf_oh oh);

    QPDF_DLL
    long long qpdf_oh_get_int_value(qpdf_data data, qpdf_oh oh);
    QPDF_DLL
    int qpdf_oh_get_int_value_as_int(qpdf_data data, qpdf_oh oh);
    QPDF_DLL
    unsigned long long qpdf_oh_get_uint_value(qpdf_data data, qpdf_oh oh);
    QPDF_DLL
    unsigned int qpdf_oh_get_uint_value_as_uint(qpdf_data data, qpdf_oh oh);

    QPDF_DLL
    char const* qpdf_oh_get_real_value(qpdf_data data, qpdf_oh oh);

    QPDF_DLL
    QPDF_BOOL qpdf_oh_is_number(qpdf_data data, qpdf_oh oh);
    QPDF_DLL
    double qpdf_oh_get_numeric_value(qpdf_data data, qpdf_oh oh);

    QPDF_DLL
    char const* qpdf_oh_get_name(qpdf_data data, qpdf_oh oh);

    QPDF_DLL
    char const* qpdf_oh_get_string_value(qpdf_data data, qpdf_oh oh);
    QPDF_DLL
    char const* qpdf_oh_get_utf8_value(qpdf_data data, qpdf_oh oh);

    QPDF_DLL
    int qpdf_oh_get_array_n_items(qpdf_data data, qpdf_oh oh);
    QPDF_DLL
    qpdf_oh qpdf_oh_get_array_item(qpdf_data data, qpdf_oh oh, int n);

    /* "C"-specific dictionary key iteration */

    /* Iteration is allowed on only one dictionary at a time. */
    QPDF_DLL
    void qpdf_oh_begin_dict_key_iter(qpdf_data data, qpdf_oh dict);
    QPDF_DLL
    QPDF_BOOL qpdf_oh_dict_more_keys(qpdf_data data);
    /* The memory returned by qpdf_oh_dict_next_key is owned by
     * qpdf_data. It is good until the next call to
     * qpdf_oh_dict_next_key with the same qpdf_data object. Calling
     * the method again, even with a different dict, invalidates
     * previous return values.
     */
    QPDF_DLL
    char const* qpdf_oh_dict_next_key(qpdf_data data);

    /* end "C"-specific dictionary key iteration */

    QPDF_DLL
    QPDF_BOOL qpdf_oh_has_key(qpdf_data data, qpdf_oh oh, char const* key);
    QPDF_DLL
    qpdf_oh qpdf_oh_get_key(qpdf_data data, qpdf_oh oh, char const* key);

    QPDF_DLL
    QPDF_BOOL qpdf_oh_is_or_has_name(
        qpdf_data data, qpdf_oh oh, char const* key);

    QPDF_DLL
    qpdf_oh qpdf_oh_new_null(qpdf_data data);
    QPDF_DLL
    qpdf_oh qpdf_oh_new_bool(qpdf_data data, QPDF_BOOL value);
    QPDF_DLL
    qpdf_oh qpdf_oh_new_integer(qpdf_data data, long long value);
    QPDF_DLL
    qpdf_oh qpdf_oh_new_real_from_string(qpdf_data data, char const* value);
    QPDF_DLL
    qpdf_oh qpdf_oh_new_real_from_double(qpdf_data data,
                                         double value, int decimal_places);
    QPDF_DLL
    qpdf_oh qpdf_oh_new_name(qpdf_data data, char const* name);
    QPDF_DLL
    qpdf_oh qpdf_oh_new_string(qpdf_data data, char const* str);
    QPDF_DLL
    qpdf_oh qpdf_oh_new_unicode_string(qpdf_data data, char const* utf8_str);
    QPDF_DLL
    qpdf_oh qpdf_oh_new_array(qpdf_data data);
    QPDF_DLL
    qpdf_oh qpdf_oh_new_dictionary(qpdf_data data);

    QPDF_DLL
    void qpdf_oh_make_direct(qpdf_data data, qpdf_oh oh);

    QPDF_DLL
    void qpdf_oh_set_array_item(qpdf_data data, qpdf_oh oh,
                                int at, qpdf_oh item);
    QPDF_DLL
    void qpdf_oh_insert_item(qpdf_data data, qpdf_oh oh, int at, qpdf_oh item);
    QPDF_DLL
    void qpdf_oh_append_item(qpdf_data data, qpdf_oh oh, qpdf_oh item);
    QPDF_DLL
    void qpdf_oh_erase_item(qpdf_data data, qpdf_oh oh, int at);

    QPDF_DLL
    void qpdf_oh_replace_key(qpdf_data data, qpdf_oh oh,
                             char const* key, qpdf_oh item);
    QPDF_DLL
    void qpdf_oh_remove_key(qpdf_data data, qpdf_oh oh, char const* key);
    QPDF_DLL
    void qpdf_oh_replace_or_remove_key(qpdf_data data, qpdf_oh oh,
                                       char const* key, qpdf_oh item);

    QPDF_DLL
    qpdf_oh qpdf_oh_get_dict(qpdf_data data, qpdf_oh oh);

    QPDF_DLL
    int qpdf_oh_get_object_id(qpdf_data data, qpdf_oh oh);
    QPDF_DLL
    int qpdf_oh_get_generation(qpdf_data data, qpdf_oh oh);

    QPDF_DLL
    char const* qpdf_oh_unparse(qpdf_data data, qpdf_oh oh);
    QPDF_DLL
    char const* qpdf_oh_unparse_resolved(qpdf_data data, qpdf_oh oh);
    QPDF_DLL
    char const* qpdf_oh_unparse_binary(qpdf_data data, qpdf_oh oh);
#ifdef __cplusplus
}
#endif


#endif /* QPDF_C_H */
