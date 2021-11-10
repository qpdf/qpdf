# QPDF

[![QPDF](logo/qpdf.svg)](https://qpdf.sourceforge.io)

[![QPDF Build](https://github.com/qpdf/qpdf/workflows/QPDF%20Build/badge.svg)](https://github.com/qpdf/qpdf/actions)
[![Total lgtm alerts](https://img.shields.io/lgtm/alerts/g/qpdf/qpdf.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/qpdf/qpdf/alerts/)
[![Language grade on lgtm: C/C++](https://img.shields.io/lgtm/grade/cpp/g/qpdf/qpdf.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/qpdf/qpdf/context:cpp)

This is the QPDF package. QPDF is a command-line tool and C++ library that performs content-preserving transformations on PDF files. It supports linearization, encryption, and numerous other features. It can also be used for splitting and merging files, creating PDF files (but you have to supply all the content yourself), and inspecting files for study or analysis. QPDF does not render PDFs or perform text extraction, and it does not contain higher-level interfaces for working with page contents. It is a low-level tool for working with the structure of PDF files and can be a valuable tool for anyone who wants to do programmatic or command-line-based manipulation of PDF files.

Additional information about it can be found at https://qpdf.sourceforge.io.  The source code repository is hosted at GitHub: https://github.com/qpdf/qpdf.

# Verifying Distributions

The public key used to sign qpdf source distributions has fingerprint `C2C9 6B10 011F E009 E6D1  DF82 8A75 D109 9801 2C7E` and can be found at https://q.ql.org/pubkey.asc or downloaded from a public key server.

# Copyright, License

QPDF is copyright (c) 2005-2021 Jay Berkenbilt

Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at

  https://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.

You may also see the license in the file [LICENSE.txt](LICENSE.txt) in the source distribution.

Versions of qpdf prior to version 7 were released under the terms of version 2.0 of the Artistic License. At your option, you may continue to consider qpdf to be licensed under those terms. Please see the manual for additional information. The Artistic License appears in the file [Artistic-2.0](Artistic-2.0) in the source distribution.

# Prerequisites

QPDF requires a C++ compiler that supports C++-14.

QPDF depends on the external libraries [zlib](https://www.zlib.net/) and [jpeg](https://www.ijg.org/files/). The [libjpeg-turbo](https://libjpeg-turbo.org/) library is also known to work since it is compatible with the regular jpeg library, and QPDF doesn't use any interfaces that aren't present in the straight jpeg8 API. These are part of every Linux distribution and are readily available. Download information appears in the documentation. For Windows, you can download pre-built binary versions of these libraries for some compilers; see [README-windows.md](README-windows.md) for additional details.

Depending on which crypto providers are enabled, then [GnuTLS](https://www.gnutls.org/) and [OpenSSL](https://openssl.org) may also be required. This is discussed more in `Crypto providers` below.

# Licensing terms of embedded software

QPDF makes use of zlib and jpeg libraries for its functionality. These packages can be downloaded separately from their own download locations. If the optional GnuTLS or OpenSSL crypto providers are enabled, then GnuTLS and/or OpenSSL are also required.

Please see the [NOTICE](NOTICE.md) file for information on licenses of embedded software.

# Crypto providers

As of version 9.1.0, qpdf can use different crypto implementations. These can be selected at compile time or at runtime. The native crypto implementations that were used in all versions prior to 9.1.0 are still present and enabled by default.

Initially, the following providers are available:
* `native`: a native implementation where all the source is embedded in qpdf and no external dependencies are required
* `openssl`: an implementation that can use the OpenSSL (or BoringSSL) libraries to provide crypto; causes libqpdf to link with the OpenSSL library
* `gnutls`: an implementation that uses the GnuTLS library to provide crypto; causes libqpdf to link with the GnuTLS library

The default behavior is for ./configure to discover which other crypto providers can be supported based on available external libraries, to build all available crypto providers, and to use an external provider as the default over the native one. This behavior can be changed with the following flags to ./configure:

* `--enable-crypto-x` -- (where `x` is a supported crypto provider): enable the `x` crypto provider, requiring any external dependencies it needs
* `--disable-crypto-x` -- disable the `x` provider, and do not link against its dependencies even if they are available
* `--with-default-crypto=x` -- make `x` the default provider even if a higher priority one is available
* `--disable-implicit-crypto` -- only build crypto providers that are explicitly requested with an `--enable-crypto-x` option

For example, if you want to guarantee that the GnuTLS crypto provider is used, you could run ./configure with `--enable-crypto-gnutls --disable-implicit-crypto`.

Please see the section on crypto providers in the manual for more details.

## Note about weak cryptographic algorithms

The PDF file format used to rely on RC4 for encryption. Using 256-bit keys always uses AES instead, and with 128-bit keys, you can elect to use AES. qpdf does its best to warn when someone is writing a file with weak cryptographic algorithms, but qpdf must always retain support for being able to read and even write files with weak encryption to be able to fully support older PDF files and older PDF readers.

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

# Building without wchar_t

Executive summary: manually define -DQPDF_NO_WCHAR_T in your build if you are building on a system without wchar_t. For details, read the rest of this section.

While wchar_t is part of the C++ standard library and should be present on virtually every system, there are some stripped down systems, such as those targeting certain embedded environments, that lack wchar_t. Internally, qpdf uses UTF-8 encoding for everything, so there is nothing important in qpdf's API that uses wchar_t. However, there is a helper method for converting between wchar_t* and char* that uses wchar_t.

If you are building in an environment that does not support wchar_t, you can define the preprocessor symbol QPDF_NO_WCHAR_T in your build. This will work whether you are building qpdf and need to avoid compiling the code that uses wchar_t or whether you are building client code that uses qpdf.

For example, to build qpdf on a system without wchar_t, be sure that -DQPDF_NO_WCHAR_T is part of your CXXFLAGS. Similar techniques will work in other places.

Note that, when you build code with libqpdf, it is *not necessary* to have the definition of QPDF_NO_WCHAR_T in your build match what was defined when the library was built as long as you are not calling QUtil::call_main_from_wmain in your code. In other words, if your qpdf library was built on a system without wchar_t and you are using that system to build at some later time after wchar_t was available, as long as you don't call the function that uses it, you can just build normally.

Note qpdf will never define QPDF_NO_WCHAR_T using autoconf or any other automated method in spite of the fact that it would be easy to do so. That is because there is a hard rule in qpdf that values determined by autoconf are not available in the public API. This is because there is never a guarantee or even expectation that those values will match between the system on which qpdf was build and the system on which a user is building code with libqpdf, and qpdf's include directory should look the same across all systems.

# Building on Windows

QPDF is known to build and pass its test suite with mingw (latest version tested: gcc 7.2.0), mingw64 (latest version tested: 7.2.0) and Microsoft Visual C++ 2015, both 32-bit and 64-bit versions.  MSYS2 is required to build as well in order to get make and other related tools.  See [README-windows.md](README-windows.md) for details on how to build under Windows.

# Additional Notes on Build

QPDF's build system, inspired by [abuild](https://www.qbilt.org/abuild), can optionally use its own built-in rules rather than using libtool and obeying the compiler specified with configure.  This can be enabled by passing `--with-buildrules=buildrules` where buildrules corresponds to one of the `.mk` files (other than `rules.mk`) in the make directory. This should never be necessary on a UNIX system, but may be necessary on a Windows system.  See [README-windows.md](README-windows.md) for details.

The QPDF package provides some executables and a software library.  A user manual can be found in the "doc" directory.  The docbook sources to the user manual can be found in the `manual` directory.

The software library is just `libqpdf`, and all the header files are in the `qpdf` subdirectories of `include` and `libqpdf`. If you link statically with `-lqpdf`, then you will also need to link with `-lz` and `-ljpeg`. The shared qpdf library is linked with `-lz` and `-ljpeg`, none of qpdf's public header files directly include files from `libz`, and only `Pl_DCT.hh` includes files from `libjpeg`, so for most cases, qpdf's development files are self contained. If you need to use `Pl_DCT` in your application code, you will need to have the header files for some libjpeg distribution in your include path.

To learn about using the library, please read comments in the header files in `include/qpdf`, especially `QPDF.hh`, `QPDFObjectHandle.hh`, and
`QPDFWriter.hh`. These are the best sources of documentation on the API.  You can also study the code of `qpdf/qpdf.cc`, which exercises most of the public interface.  There are additional example programs in the examples directory.  Reading all the source files in the `qpdf` directory (including the qpdf command-line tool and some test drivers) along with the code in the examples directory will give you a complete picture of every aspect of the public interface.

# Additional Notes on Test Suite

By default, slow tests and tests that require dependencies beyond those needed to build qpdf are disabled.  Slow tests include image comparison tests and large file tests.  Image comparison tests can be enabled by passing `--enable-test-compare-images` to ./configure.  This was on by default in qpdf versions prior to 3.0, but is now off by default.  Large file tests can be enabled by passing `--with-large-file-test-path=path` to `./configure` or by setting the `QPDF_LARGE_FILE_TEST_PATH` environment variable.  On Windows, this should be a Windows path.  Run `./configure --help` for additional options.  The test suite provides nearly full coverage even without these tests.  Unless you are making deep changes to the library that would impact the contents of the generated PDF files or testing this on a new platform for the first time, there is no real reason to run these tests.  If you're just running the test suite to make sure that qpdf works for your build, the default tests are adequate.  The configure rules for these tests do nothing other than setting variables in `autoconf.mk`, so you can feel free to turn these on and off directly in `autoconf.mk` rather than rerunning configure.

If you are packaging qpdf for a distribution and preparing a build that is run by an autobuilder, you may want to add the `--enable-show-failed-test-output` to configure options.  This way, if the test suite fails, test failure detail will be included in the build output.  Otherwise, you will have to have access to the `qtest.log` file from the build to view test failures.  The Debian packages for qpdf enable this option.

# Random Number Generation

By default, qpdf uses the crypto provider for generating random numbers. The rest of this applies only if you are using the native crypto provider.

If the native crypto provider is in use, then, when `qpdf` detects either the Windows cryptography API or the existence of `/dev/urandom`, `/dev/arandom`, or `/dev/random`, it uses them to generate cryptographically secure random numbers.  If none of these conditions are true, the build will fail with an error.  This behavior can be modified in several ways:
* If you configure with `--disable-os-secure-random` or define `SKIP_OS_SECURE_RANDOM`, qpdf will not attempt to use Windows cryptography or the random device.  You must either supply your own random data provider or allow use of insecure random numbers.
* If you configure qpdf with the `--enable-insecure-random` option or define `USE_INSECURE_RANDOM`, qpdf will try insecure random numbers if OS-provided secure random numbers are disabled.  This is not a fallback.  In order for insecure random numbers to be used, you must also disable OS secure random numbers since, otherwise, failure to find OS secure random numbers is a compile error.  The insecure random number source is stdlib's `random()` or `rand()` calls. These random numbers are not cryptography secure, but the qpdf library is fully functional using them.  Using non-secure random numbers means that it's easier in some cases to guess encryption keys.  If you're not generating encrypted files, there's no advantage to using secure random numbers.
* In all cases, you may supply your own random data provider.  To do this, derive a class from `qpdf/RandomDataProvider` (since version 5.1.0) and call `QUtil::setRandomDataProvider` before you create any `QPDF` objects.  If you supply your own random data provider, it will always be used even if support for one of the other random data providers is compiled in.  If you wish to avoid any possibility of your build of qpdf from using anything but a user-supplied random data provider, you can define `SKIP_OS_SECURE_RANDOM` and not `USE_INSECURE_RANDOM`.  In this case, qpdf will throw a runtime error if any attempt is made to generate random numbers and no random data provider has been supplied.

If you are building qpdf on a platform that qpdf doesn't know how to generate secure random numbers on, a patch would be welcome.
