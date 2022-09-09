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

#ifndef QPDFJOB_C_H
#define QPDFJOB_C_H

/*
 * This file defines a basic "C" API for QPDFJob. See also qpdf-c.h,
 * which defines an API that exposes more of the library's API. This
 * API is primarily intended to make it simpler for programs in
 * languages other than C++ to incorporate functionality that could be
 * run directly from the command-line.
 */

#include <qpdf/DLL.h>
#include <qpdf/qpdflogger-c.h>
#include <string.h>
#ifndef QPDF_NO_WCHAR_T
# include <wchar.h>
#endif

/*
 * This file provides a minimal wrapper around QPDFJob. See
 * examples/qpdfjob-c.c for an example of its use.
 */

#ifdef __cplusplus
extern "C" {
#endif
    /* SHORT INTERFACE -- These functions are single calls that take
     * care of the whole life cycle of QPDFJob. They can be used for
     * one-shot operations where no additional configuration is
     * needed. See FULL INTERFACE below. */

    /* This function does the equivalent of running the qpdf
     * command-line with the given arguments and returns the exit code
     * that qpdf would use. argv must be a null-terminated array of
     * null-terminated UTF8-encoded strings. If calling this from
     * wmain on Windows, use qpdfjob_run_from_wide_argv instead. Exit
     * code values are defined in Constants.h in the qpdf_exit_code_e
     * type.
     */
    QPDF_DLL
    int qpdfjob_run_from_argv(char const* const argv[]);

#ifndef QPDF_NO_WCHAR_T
    /* This function is the same as qpdfjob_run_from_argv except argv
     * is encoded with wide characters. This would be suitable for
     * calling from a Windows wmain function.
     */
    QPDF_DLL
    int qpdfjob_run_from_wide_argv(wchar_t const* const argv[]);
#endif /* QPDF_NO_WCHAR_T */

    /* This function runs QPDFJob from a job JSON file. See the "QPDF
     * Job" section of the manual for details. The JSON string must be
     * UTF8-encoded. It returns the error code that qpdf would return
     * with the equivalent command-line invocation. Exit code values
     * are defined in Constants.h in the qpdf_exit_code_e type.
     */
    QPDF_DLL
    int qpdfjob_run_from_json(char const* json);

    /* FULL INTERFACE -- new in qpdf11. Similar to the qpdf-c.h API,
     * you must call qpdfjob_init to get a qpdfjob_handle and, when
     * done, call qpdfjob_cleanup to free resources. Remaining methods
     * take qpdfjob_handle as an argument. This interface requires
     * more calls but also offers greater flexibility.
     */
    typedef struct _qpdfjob_handle* qpdfjob_handle;
    QPDF_DLL
    qpdfjob_handle qpdfjob_init();

    QPDF_DLL
    void qpdfjob_cleanup(qpdfjob_handle* j);

    /* Set or get the current logger. You need to call
     * qpdflogger_cleanup on the logger handles when you are done with
     * the handles. The underlying logger is cleaned up automatically
     * and persists if needed after the logger handle is destroyed.
     * See comments in qpdflogger-c.h for details.
     */

    QPDF_DLL
    void qpdfjob_set_logger(qpdfjob_handle j, qpdflogger_handle logger);
    QPDF_DLL
    qpdflogger_handle qpdfjob_get_logger(qpdfjob_handle j);

    /* This function wraps QPDFJob::initializeFromArgv. The return
     * value is the same as qpdfjob_run. If this returns an error, it
     * is invalid to call any other functions this job handle.
     */
    QPDF_DLL
    int
    qpdfjob_initialize_from_argv(qpdfjob_handle j, char const* const argv[]);

#ifndef QPDF_NO_WCHAR_T
    /* This function is the same as qpdfjob_initialize_from_argv
     * except argv is encoded with wide characters. This would be
     * suitable for calling from a Windows wmain function.
     */
    QPDF_DLL
    int qpdfjob_initialize_from_wide_argv(
        qpdfjob_handle j, wchar_t const* const argv[]);
#endif /* QPDF_NO_WCHAR_T */

    /* This function wraps QPDFJob::initializeFromJson. The return
     * value is the same as qpdfjob_run. If this returns an error, it
     * is invalid to call any other functions this job handle.
     */
    QPDF_DLL
    int qpdfjob_initialize_from_json(qpdfjob_handle j, char const* json);

    /* This function wraps QPDFJob::run. It returns the error code
     * that qpdf would return with the equivalent command-line
     * invocation. Exit code values are defined in Constants.h in the
     * qpdf_exit_code_e type.
     */
    QPDF_DLL
    int qpdfjob_run(qpdfjob_handle j);

    /* Allow specification of a custom progress reporter. The progress
     * reporter is only used if progress is otherwise requested (with
     * the --progress option or "progress": "" in the JSON).
     */
    QPDF_DLL
    void qpdfjob_register_progress_reporter(
        qpdfjob_handle j,
        void (*report_progress)(int percent, void* data),
        void* data);

#ifdef __cplusplus
}
#endif

#endif /* QPDFJOB_C_H */
