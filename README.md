
# QPDF

[![QPDF](logo/qpdf.svg)](http://qpdf.sourceforge.net)

[![Azure Pipeline Build Status](https://dev.azure.com/qpdf/qpdf/_apis/build/status/qpdf.qpdf)](https://dev.azure.com/qpdf/qpdf/_build/latest?definitionId=5) [![Travis Build Status](https://travis-ci.org/qpdf/qpdf.svg?branch=master)](https://travis-ci.org/qpdf/qpdf)

This is the QPDF package.  Information about it can be found at http://qpdf.sourceforge.net.  The source code repository is hosted at github: https://github.com/qpdf/qpdf.

# Verifying Distributions

The public key used to sign qpdf source distributions has fingerprint `C2C9 6B10 011F E009 E6D1  DF82 8A75 D109 9801 2C7E` and can be found at https://q.ql.org/pubkey.asc or downloaded from a public key server.

# Copyright, License

QPDF is copyright (c) 2005-2019 Jay Berkenbilt

Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.

You may also see the license in the file [LICENSE.txt](LICENSE.txt) in the source distribution.

Versions of qpdf prior to version 7 were released under the terms of version 2.0 of the Artistic License. At your option, you may continue to consider qpdf to be licensed under those terms. Please see the manual for additional information. The Artistic License appears in the file [Artistic-2.0](Artistic-2.0) in the source distribution.

# Prerequisites

QPDF depends on the external libraries [zlib](http://www.zlib.net/) and [jpeg](http://www.ijg.org/files/). The [libjpeg-turbo](https://libjpeg-turbo.org/) library is also known to work since it is compatible with the regular jpeg library, and QPDF doesn't use any interfaces that aren't present in the straight jpeg8 API. These are part of every Linux distribution and are readily available. Download information appears in the documentation. For Windows, you can download pre-built binary versions of these libraries for some compilers; see [README-windows.md](README-windows.md) for additional details.

QPDF requires a C++ compiler that works with STL.  Your compiler must also support `long long`.  Almost all modern compilers do.  If you are trying to port qpdf to a compiler that doesn't support `long long`, you could change all occurrences of `long long` to `long` in the source code, noting that this would break binary compatibility with other builds of qpdf.  Doing so would certainly prevent qpdf from working with files larger than 2 GB, but remaining functionality would most likely work fine.  If you built qpdf this way and it passed its test suite with large file support disabled, you could be confident that you had an otherwise working qpdf.

# Licensing terms of embedded software

QPDF makes use of zlib and jpeg libraries for its functionality. These packages can be downloaded separately from their own download locations, or they can be downloaded in the external-libs area of the qpdf download site.

Please see the [NOTICE](NOTICE.md) file for information on licenses of embedded software.

# Building from a pristine checkout

When building qpdf from a pristine checkout from version control, generated documentation files are not present. You may either generate them (by passing `--enable-doc-maintenance` to `./configure` and satisfying the extra build-time dependencies) or obtain them from a released source package, which includes them. If you want to grab just the files that are in the source distribution but not in the repository, extract a source distribution in a temporary directory, and run `make CLEAN=1 distfiles.zip`. This will create a file called `distfiles.zip`, which can you can extract in a checkout of the source repository. This step is optional unless you are running make install and want the html and PDF versions of the documentation to be installed.

# Building from source distribution on UNIX/Linux

For UNIX and UNIX-like systems, you can usually get by with just

```
./configure
make
make install
```

Packagers may set DESTDIR, in which case make install will install inside of DESTDIR, as is customary with many packages.  For more detailed general information, see the "INSTALL" file in this directory.  If you are already accustomed to building and installing software that uses autoconf, there's nothing new for you in the INSTALL file. Note that qpdf uses `autoconf` but not `automake`. We have our own system of Makefiles that allows cross-directory dependencies, doesn't use recursive make, and works better on non-UNIX platforms.

# Building on Windows

QPDF is known to build and pass its test suite with mingw (latest version tested: gcc 7.2.0), mingw64 (latest version tested: 7.2.0) and Microsoft Visual C++ 2015, both 32-bit and 64-bit versions.  MSYS2 is required to build as well in order to get make and other related tools.  See [README-windows.md](README-windows.md) for details on how to build under Windows.

# Additional Notes on Build

QPDF's build system, inspired by [abuild](http://www.abuild.org), can optionally use its own built-in rules rather than using libtool and obeying the compiler specified with configure.  This can be enabled by passing `--with-buildrules=buildrules` where buildrules corresponds to one of the `.mk` files (other than `rules.mk`) in the make directory. This should never be necessary on a UNIX system, but may be necessary on a Windows system.  See [README-windows.md](README-windows.md) for details.  There is a `gcc-linux.mk` file enable `gcc-linux` build rules, but it is intended to help test the build system; Linux users should build with the `libtools` rules, which are enabled by default.

The QPDF package provides some executables and a software library.  A user manual can be found in the "doc" directory.  The docbook sources to the user manual can be found in the `manual` directory.

The software library is just `libqpdf`, and all the header files are in the `qpdf` subdirectories of `include` and `libqpdf`. If you link statically with `-lqpdf`, then you will also need to link with `-lz` and `-ljpeg`. The shared qpdf library is linked with `-lz` and `-ljpeg`, none of qpdf's public header files directly include files from `libz`, and only `Pl_DCT.hh` includes files from `libjpeg`, so for most cases, qpdf's development files are self contained. If you need to use `Pl_DCT` in your application code, you will need to have the header files for some libjpeg distribution in your include path.

To learn about using the library, please read comments in the header files in `include/qpdf`, especially `QPDF.hh`, `QPDFObjectHandle.hh`, and
`QPDFWriter.hh`. These are the best sources of documentation on the API.  You can also study the code of `qpdf/qpdf.cc`, which exercises most of the public interface.  There are additional example programs in the examples directory.  Reading all the source files in the `qpdf` directory (including the qpdf command-line tool and some test drivers) along with the code in the examples directory will give you a complete picture of every aspect of the public interface.

# Additional Notes on Test Suite

By default, slow tests and tests that require dependencies beyond those needed to build qpdf are disabled.  Slow tests include image comparison tests and large file tests.  Image comparison tests can be enabled by passing `--enable-test-compare-images` to ./configure.  This was on by default in qpdf versions prior to 3.0, but is now off by default.  Large file tests can be enabled by passing `--with-large-file-test-path=path` to `./configure` or by setting the `QPDF_LARGE_FILE_TEST_PATH` environment variable.  On Windows, this should be a Windows path.  Run `./configure --help` for additional options.  The test suite provides nearly full coverage even without these tests.  Unless you are making deep changes to the library that would impact the contents of the generated PDF files or testing this on a new platform for the first time, there is no real reason to run these tests.  If you're just running the test suite to make sure that qpdf works for your build, the default tests are adequate.  The configure rules for these tests do nothing other than setting variables in `autoconf.mk`, so you can feel free to turn these on and off directly in `autoconf.mk` rather than rerunning configure.

If you are packaging qpdf for a distribution and preparing a build that is run by an autobuilder, you may want to add the `--enable-show-failed-test-output` to configure options.  This way, if the test suite fails, test failure detail will be included in the build output.  Otherwise, you will have to have access to the `qtest.log` file from the build to view test failures.  The debian packages for qpdf enable this option.

# Random Number Generation

By default, when `qpdf` detects either the Windows cryptography API or the existence of `/dev/urandom`, `/dev/arandom`, or `/dev/random`, it uses them to generate cryptography secure random numbers.  If none of these conditions are true, the build will fail with an error.  This behavior can be modified in several ways:
* If you configure with `--disable-os-secure-random` or define `SKIP_OS_SECURE_RANDOM`, qpdf will not attempt to use Windows cryptography or the random device.  You must either supply your own random data provider or allow use of insecure random numbers.
* If you configure qpdf with the `--enable-insecure-random` option or define `USE_INSECURE_RANDOM`, qpdf will try insecure random numbers if OS-provided secure random numbers are disabled.  This is not a fallback.  In order for insecure random numbers to be used, you must also disable OS secure random numbers since, otherwise, failure to find OS secure random numbers is a compile error.  The insecure random number source is stdlib's `random()` or `rand()` calls. These random numbers are not cryptography secure, but the qpdf library is fully functional using them.  Using non-secure random numbers means that it's easier in some cases to guess encryption keys.  If you're not generating encrypted files, there's no advantage to using secure random numbers.
* In all cases, you may supply your own random data provider.  To do this, derive a class from `qpdf/RandomDataProvider` (since version 5.1.0) and call `QUtil::setRandomDataProvider` before you create any `QPDF` objects.  If you supply your own random data provider, it will always be used even if support for one of the other random data providers is compiled in.  If you wish to avoid any possibility of your build of qpdf from using anything but a user-supplied random data provider, you can define `SKIP_OS_SECURE_RANDOM` and not `USE_INSECURE_RANDOM`.  In this case, qpdf will throw a runtime error if any attempt is made to generate random numbers and no random data provider has been supplied.

If you are building qpdf on a platform that qpdf doesn't know how to generate secure random numbers on, a patch would be welcome.
