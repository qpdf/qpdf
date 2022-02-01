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
    /* This function does the equivalent of running the qpdf
     * command-line with the given arguments and returns the exit code
     * that qpdf would use. Note that arguments must be UTF8-encoded.
     * If calling this from wmain on Windows, use
     * qpdfjob_run_from_wide_argv instead.
     */
    QPDF_DLL
    int qpdfjob_run_from_argv(int argc, char const* const argv[]);

#ifndef QPDF_NO_WCHAR_T
    /* This function is the same as qpdfjob_run_from_argv except argv
     * is encoded with wide characters. This would suitable for
     * calling from a Windows wmain function.
     */
    QPDF_DLL
    int qpdfjob_run_from_wide_argv(int argc, wchar_t const* const argv[]);
#endif /* QPDF_NO_WCHAR_T */

    /* This function runs QPDFJob from a job JSON file. See the "QPDF
     * Job" section of the manual for details. The JSON string must be
     * UTF8-encoded. It returns the error code that qpdf would return
     * with the equivalent command-line invocation.
     */
    QPDF_DLL
    int qpdfjob_run_from_json(char const* json);

#ifdef __cplusplus
}
#endif


#endif /* QPDFJOB_C_H */
