# QPDF

[![QPDF](logo/qpdf.svg)](https://qpdf.sourceforge.io)

[![QPDF Build](https://github.com/qpdf/qpdf/workflows/QPDF%20Build/badge.svg)](https://github.com/qpdf/qpdf/actions)
[![Total lgtm alerts](https://img.shields.io/lgtm/alerts/g/qpdf/qpdf.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/qpdf/qpdf/alerts/)
[![Language grade on lgtm: C/C++](https://img.shields.io/lgtm/grade/cpp/g/qpdf/qpdf.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/qpdf/qpdf/context:cpp)
[![Documentation Status](https://readthedocs.org/projects/qpdf/badge/?version=latest)](https://qpdf.readthedocs.io/en/latest/?badge=latest)

QPDF is a command-line tool and C++ library that performs content-preserving transformations on PDF files. It supports linearization, encryption, and numerous other features. It can also be used for splitting and merging files, creating PDF files (but you have to supply all the content yourself), and inspecting files for study or analysis. QPDF does not render PDFs or perform text extraction, and it does not contain higher-level interfaces for working with page contents. It is a low-level tool for working with the structure of PDF files and can be a valuable tool for anyone who wants to do programmatic or command-line-based manipulation of PDF files.

The [QPDF Manual](https://qpdf.readthedocs.io) is hosted online at https://qpdf.readthedocs.io. The project website is https://qpdf.sourceforge.io. The source code repository is hosted at GitHub: https://github.com/qpdf/qpdf.

# Verifying Distributions

The public key used to sign qpdf source distributions has fingerprint `C2C9 6B10 011F E009 E6D1  DF82 8A75 D109 9801 2C7E` and can be found at https://q.ql.org/pubkey.asc or downloaded from a public key server.

# Copyright, License

QPDF is copyright (c) 2005-2022 Jay Berkenbilt

Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at

  https://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.

You may also see the license in the file [LICENSE.txt](LICENSE.txt) in the source distribution.

Versions of qpdf prior to version 7 were released under the terms of version 2.0 of the Artistic License. At your option, you may continue to consider qpdf to be licensed under those terms. Please see the manual for additional information. The Artistic License appears in the file [Artistic-2.0](Artistic-2.0) in the source distribution.

# Prerequisites

QPDF requires a C++ compiler that supports C++-14.

To compile and link something with qpdf, you can use `pkg-config` with package name `libqpdf` or `cmake` with package name `qpdf`. Here's an example of a `CMakeLists.txt` file that builds a program with the qpdf library:

```
cmake_minimum_required(VERSION 3.16)
project(some-application LANGUAGES CXX)
find_package(qpdf)
add_executable(some-application some-application.cc)
target_link_libraries(some-application qpdf::libqpdf)
```

QPDF depends on the external libraries [zlib](https://www.zlib.net/) and [jpeg](https://www.ijg.org/files/). The [libjpeg-turbo](https://libjpeg-turbo.org/) library is also known to work since it is compatible with the regular jpeg library, and QPDF doesn't use any interfaces that aren't present in the straight jpeg8 API. These are part of every Linux distribution and are readily available. Download information appears in the documentation. For Windows, you can download pre-built binary versions of these libraries for some compilers; see [README-windows.md](README-windows.md) for additional details.

Depending on which crypto providers are enabled, then [GnuTLS](https://www.gnutls.org/) and [OpenSSL](https://openssl.org) may also be required. This is discussed more in `Crypto providers` below.

Detailed information appears in the [manual](https://qpdf.readthedocs.io/en/latest/installation.html).

# Licensing terms of embedded software

QPDF makes use of zlib and jpeg libraries for its functionality. These packages can be downloaded separately from their own download locations. If the optional GnuTLS or OpenSSL crypto providers are enabled, then GnuTLS and/or OpenSSL are also required.

Please see the [NOTICE](NOTICE.md) file for information on licenses of embedded software.

# Crypto providers

qpdf can use different crypto implementations. These can be selected at compile time or at runtime. The native crypto implementations that were used in all versions prior to 9.1.0 are still present, but they are not built into qpdf by default if any external providers are available at build time.

The following providers are available:
* `gnutls`: an implementation that uses the GnuTLS library to provide crypto; causes libqpdf to link with the GnuTLS library
* `openssl`: an implementation that can use the OpenSSL (or BoringSSL) libraries to provide crypto; causes libqpdf to link with the OpenSSL library
* `native`: a native implementation where all the source is embedded in qpdf and no external dependencies are required

The default behavior is for cmake to discover which other crypto providers can be supported based on available external libraries, to build all available external crypto providers, and to use an external provider as the default over the native one. By default, the native crypto provider will be used only if no external providers are available. This behavior can be changed with various cmake options as [described in the manual](https://qpdf.readthedocs.io/en/latest/installation.html#build-time-crypto-selection).

## Note about weak cryptographic algorithms

The PDF file format used to rely on RC4 for encryption. Using 256-bit keys always uses AES instead, and with 128-bit keys, you can elect to use AES. qpdf does its best to warn when someone is writing a file with weak cryptographic algorithms, but qpdf must always retain support for being able to read and even write files with weak encryption to be able to fully support older PDF files and older PDF readers.

# Building from source distribution on UNIX/Linux

Starting with version 11, qpdf builds with cmake. The default configuration with cmake works on most systems. On Windows, you can build qpdf with Visual Studio using cmake without having any additional tools installed. However, to run the test suite, you need MSYS2, and you also need MSYS2 to build with mingw.

Example UNIX/Linux build:
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

Example mingw build from an MSYS2 mingw shell:
```
cmake -S . -B build -G 'MSYS Makefiles' -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

Example MSVC build from an MSYS shell or from a Windows command shell with Visual Studio command-line tools in the path:
```
cmake -S . -B build
cmake --build build --config Release
```

Installation can be done with `cmake --install`. Packages can be made with `cpack`.

The tests use `qtest`, and the test driver is invoked by `ctest`. To see the real underlying tests, run `ctest --verbose` so that you can see `qtest`'s output. If you need to turn off qtest's color output, pass `-DQTEST_COLOR=0` to cmake.

For additional information, please refer to the [manual](https://qpdf.readthedocs.io/en/latest/installation.html).

# Building on Windows

qpdf is known to build and pass its test suite with mingw and Microsoft Visual C++. Both 32-bit and 64-bit versions work. In addition to the manual, see [README-windows.md](README-windows.md) for more details on how to build under Windows.

# Building Documentation

The QPDF manual is written in reStructured Text format and is build with [sphinx](https://www.sphinx-doc.org). The sources to the user manual can be found in the `manual` directory. For more detailed information, consult the [Building and Installing QPDF section of the manual](https://qpdf.readthedocs.io/en/latest/installation.html) or consult the [build-doc script](build-scripts/build-doc).

# Additional Notes on Build

qpdf provides cmake configuration files and pkg-config files. They support static and dynamic linking. In general, you do not need the header files from qpdf's dependencies to be available to builds that _use_ qpdf. The only exception to this is that, if you include `Pl_DCT.hh`, you need header files from `libjpeg`. Since this is a rare case, qpdf's cmake and pkg-config files do not automatically add a JPEG include path to the build. If you are using `Pl_DCT` explicitly, you probably already have that configured in your build.

To learn about using the library, please read comments in the header files in [include/qpdf](include/qpdf/), especially [QPDF.hh](include/qpdf/QPDF.hh), [QPDFObjectHandle.hh](include/qpdf/QPDFObjectHandle.hh), and
[QPDFWriter.hh](include/qpdf/QPDFWriter.hh). These are the best sources of documentation on the API.  You can also study the code of [QPDFJob.cc](libqpdf/QPDFJob.cc), which exercises most of the public interface.  There are additional example programs in the [examples](examples/) directory.

# Additional Notes on Test Suite

By default, slow tests and tests that require dependencies beyond those needed to build qpdf are disabled.  Slow tests include image comparison tests and large file tests.  Image comparison tests can be enabled by setting the `QPDF_TEST_COMPARE_IMAGES` environment variable to `1`. Large file tests can be enabled setting the `QPDF_LARGE_FILE_TEST_PATH` environment variable to the absolute path of a directory with at least 11 GB of free space that can handle files over 4 GB in size.  On Windows, this should be a Windows path (e.g. `C:\LargeFileTemp` even if the build is being run from an MSYS2 environment.  The test suite provides nearly full coverage even without these tests.  Unless you are making deep changes to the library that would impact the contents of the generated PDF files or testing this on a new platform for the first time, there is no real reason to run these tests.  If you're just running the test suite to make sure that qpdf works for your build, the default tests are adequate.

If you are packaging qpdf for a distribution and preparing a build that is run by an autobuilder, you may want to pass `-DSHOW_FAILED_TEST_OUTPUT=1` to `cmake` and run `ctest` with the `--verbose` or `--output-on-failure` option.  This way, if the test suite fails, test failure detail will be included in the build output.  Otherwise, you will have to have access to the `qtest.log` file from the build to view test failures.  The Debian packages for qpdf enable this option. More notes for packagers can be found in [the manual](https://qpdf.readthedocs.io/en/latest/packaging.html).

# Random Number Generation

By default, qpdf uses the crypto provider for generating random numbers. The rest of this applies only if you are using the native crypto provider.

If the native crypto provider is in use, then, when `qpdf` detects either the Windows cryptography API or the existence of `/dev/urandom`, `/dev/arandom`, or `/dev/random`, it uses them to generate cryptographically secure random numbers.  If none of these conditions are true, the build will fail with an error.  This behavior can be modified in several ways:
* If you use the cmake option `SKIP_OS_SECURE_RANDOM` or define the `SKIP_OS_SECURE_RANDOM` preprocessor symbol, qpdf will not attempt to use Windows cryptography or the random device.  You must either supply your own random data provider or allow use of insecure random numbers.
* If you turn on the cmake option `USE_INSECURE_RANDOM` or define the `USE_INSECURE_RANDOM` preprocessor symbol, qpdf will try insecure random numbers if OS-provided secure random numbers are disabled.  This is not a fallback.  In order for insecure random numbers to be used, you must also disable OS secure random numbers since, otherwise, failure to find OS secure random numbers is a compile error.  The insecure random number source is stdlib's `random()` or `rand()` calls. These random numbers are not cryptography secure, but the qpdf library is fully functional using them.  Using non-secure random numbers means that it's easier in some cases to guess encryption keys.
* In all cases, you may supply your own random data provider.  To do this, derive a class from `qpdf/RandomDataProvider` (since version 5.1.0) and call `QUtil::setRandomDataProvider` before you create any `QPDF` objects.  If you supply your own random data provider, it will always be used even if support for one of the other random data providers is compiled in.  If you wish to avoid any possibility of your build of qpdf from using anything but a user-supplied random data provider, you can define `SKIP_OS_SECURE_RANDOM` and not `USE_INSECURE_RANDOM`.  In this case, qpdf will throw a runtime error if any attempt is made to generate random numbers and no random data provider has been supplied.
