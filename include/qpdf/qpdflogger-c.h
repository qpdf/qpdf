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

#ifndef QPDFLOGGER_H
#define QPDFLOGGER_H

/*
 * This file provides a C API for QPDFLogger. See QPDFLogger.hh for
 * information about the logger and
 * examples/qpdfjob-c-save-attachment.c for an example.
 */

#include <qpdf/DLL.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

    /* To operate on a logger, you need a handle to it. call
     * qpdflogger_default_logger to get a handle for the default
     * logger. There are functions in qpdf-c.h and qpdfjob-c.h that
     * also take or return logger handles. When you're done with the
     * logger handler, call qpdflogger_cleanup. This cleans up the
     * handle but leaves the underlying log object intact. (It uses a
     * shared pointer and will be cleaned up automatically when it is
     * no longer in use.) That means you can create a logger with
     * qpdflogger_create(), pass the logger handle to a function in
     * qpdf-c.h or qpdfjob-c.h, and then clean it up, subject to
     * constraints imposed by the other function.
     */

    typedef struct _qpdflogger_handle* qpdflogger_handle;
    QPDF_DLL
    qpdflogger_handle qpdflogger_default_logger();

    /* Calling cleanup on the handle returned by qpdflogger_create
     * destroys the handle but not the underlying logger. See comments
     * above.
     */
    QPDF_DLL
    qpdflogger_handle qpdflogger_create();

    QPDF_DLL
    void qpdflogger_cleanup(qpdflogger_handle* l);

    enum qpdf_log_dest_e {
        qpdf_log_dest_default = 0,
        qpdf_log_dest_stdout = 1,
        qpdf_log_dest_stderr = 2,
        qpdf_log_dest_discard = 3,
        qpdf_log_dest_custom = 4,
    };

    /* Function should return 0 on success. */
    typedef int (*qpdf_log_fn_t)(char const* data, size_t len, void* udata);

    QPDF_DLL
    void qpdflogger_set_info(
        qpdflogger_handle l,
        enum qpdf_log_dest_e dest,
        qpdf_log_fn_t fn,
        void* udata);
    QPDF_DLL
    void qpdflogger_set_warn(
        qpdflogger_handle l,
        enum qpdf_log_dest_e dest,
        qpdf_log_fn_t fn,
        void* udata);
    QPDF_DLL
    void qpdflogger_set_error(
        qpdflogger_handle l,
        enum qpdf_log_dest_e dest,
        qpdf_log_fn_t fn,
        void* udata);

    /* A non-zero value for only_if_not_set means that the save
     * pipeline will only be changed if it is not already set.
     */
    QPDF_DLL
    void qpdflogger_set_save(
        qpdflogger_handle l,
        enum qpdf_log_dest_e dest,
        qpdf_log_fn_t fn,
        void* udata,
        int only_if_not_set);
    QPDF_DLL
    void qpdflogger_save_to_standard_output(
        qpdflogger_handle l, int only_if_not_set);

    /* For testing */
    QPDF_DLL
    int qpdflogger_equal(qpdflogger_handle l1, qpdflogger_handle l2);

#ifdef __cplusplus
}
#endif

#endif // QPDFLOGGER_H
