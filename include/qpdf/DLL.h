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

#ifndef QPDF_DLL_HH
#define QPDF_DLL_HH

/* The first version of qpdf to include the version constants is 10.6.0. */
#define QPDF_MAJOR_VERSION 11
#define QPDF_MINOR_VERSION 2
#define QPDF_PATCH_VERSION 0
#define QPDF_VERSION "11.2.0"

/*
 * This file defines symbols that control the which functions,
 * classes, and methods are exposed to the public ABI (application
 * binary interface). See below for a detailed explanation.
 */

#if defined _WIN32 || defined __CYGWIN__
# ifdef libqpdf_EXPORTS
#  define QPDF_DLL __declspec(dllexport)
# else
#  define QPDF_DLL
# endif
# define QPDF_DLL_PRIVATE
#elif defined __GNUC__
# define QPDF_DLL __attribute__((visibility("default")))
# define QPDF_DLL_PRIVATE __attribute__((visibility("hidden")))
#else
# define QPDF_DLL
# define QPDF_DLL_PRIVATE
#endif
#ifdef __GNUC__
# define QPDF_DLL_CLASS QPDF_DLL
#else
# define QPDF_DLL_CLASS
#endif

/*

Here's what's happening. See also https://gcc.gnu.org/wiki/Visibility
for a more in-depth discussion.

* Everything in the public ABI must be exported. Things not in the
  public ABI should not be exported.

* A class's runtime type information is need if the class is going to
  be used as an exception, inherited from, or tested with
  dynamic_class. To do these things across a shared object boundary,
  runtime type information must be exported.

* On Windows:

  * For a symbol (function, method, etc.) to be exported into the
    public ABI, it must be explicitly marked for export.

  * If you mark a class for export, all symbols in the class,
    including private methods, are exported into the DLL, and there is
    no way to exclude something from export.

  * A class's run-time type information is made available based on the
    presence of a compiler flag (with MSVC), which is always on for
    qpdf builds.

  * Marking classes for export should be done only when *building* the
    DLL, not when *using* the DLL.

  * It is possible to mark symbols for import for DLL users, but it is
    not necessary, and doing it right is complex in our case of being
    multi-platform and building both static and shared libraries that
    use the same headers, so we don't bother.

  * If we don't export base classes with mingw, the vtables don't end
    up in the DLL.

* On Linux (and other similar systems):

  * Common compilers such as gcc and clang export all symbols into the
    public ABI by default. The qpdf build overrides this by using
    "visibility=hidden", which makes it behave more like Windows in
    that things have to be explicitly exported to appear in the public
    ABI.

  * As with Windows, marking a class for export causes everything in
    the class, including private methods, the be exported. However,
    unlike in Windows:

    * It is possible to explicitly mark symbols as private

    * The only way to get the runtime type and vtable information into
      the ABI is to mark the class as exported.

    * It is harmless and sometimes necessary to include the visibility
      marks when using the library as well as when building it. In
      particular, clang on MacOS requires the visibility marks to
      match in both cases.

What does this mean:

* On Windows, we never have to export a class, and while there is no
  way to "unexport" something, we also have no need to do it.

* On non-Windows, we have to export some classes, and when we do, we
  have to "unexport" some of their parts.

* We only use the libqpdf_EXPORTS as a conditional for defining the
  symbols for Windows builds.

To achieve this, we use QPDF_DLL_CLASS to export classes, QPDF_DLL to
export methods, and QPDF_DLL_PRIVATE to unexport private methods in
exported classes.

*/

#endif /* QPDF_DLL_HH */
