.. _installing:

Building and Installing QPDF
============================

This chapter describes how to build and install qpdf.

.. _prerequisites:

Dependencies
------------

qpdf has few external dependencies. This section describes what you
need to build qpdf in various circumstances.

Basic Dependencies
~~~~~~~~~~~~~~~~~~

- A C++ compiler that supports C++-17

- `CMake <https://www.cmake.org>`__ version 3.16 or later

- `zlib <https://www.zlib.net/>`__ or a compatible zlib implementation

- A libjpeg-compatible library such as `jpeg <https://ijg.org/>`__ or
  `libjpeg-turbo <https://libjpeg-turbo.org/>`__

- *Recommended but not required:* `gnutls <https://www.gnutls.org/>`__
  to be able to use the gnutls crypto provider and/or `openssl
  <https://openssl.org/>`__ to be able to use the openssl crypto
  provider

The qpdf source tree includes a few automatically generated files. The
code generator uses Python 3. Automatic code generation is off by
default. For a discussion, refer to :ref:`build-options`.

Test Dependencies
~~~~~~~~~~~~~~~~~

qpdf's test suite is run by ``ctest``, which is part of CMake, but
the tests themselves are implemented using an embedded copy of `qtest
<https://qtest.sourceforge.io>`__, which is implemented in perl. On
Windows, MSYS2's perl is known to work.

qtest requires `GNU diffutils
<http://www.gnu.org/software/diffutils/>`__ or any other diff that
supports :command:`diff -u`. The default ``diff`` command works on
GNU/Linux and MacOS.

Part of qpdf's test suite does comparisons of the contents PDF files
by converting them to images and comparing the images. The image
comparison tests are disabled by default. Those tests are not required
for determining correctness of a qpdf build since the test suite also
contains expected output files that are compared literally. The image
comparison tests provide an extra check to make sure that any content
transformations don't break the rendering of pages. Transformations
that affect the content streams themselves are off by default and are
only provided to help developers look into the contents of PDF files.
If you are making deep changes to the library that cause changes in
the contents of the files that qpdf generates, then you should enable
the image comparison tests. Enable them by setting the
``QPDF_TEST_COMPARE_IMAGES`` environment variable to ``1`` before
running tests. Image comparison tests add these additional
requirements:

- `libtiff <http://www.simplesystems.org/libtiff/>`__ command-line
  utilities

- `GhostScript <https://www.ghostscript.com/>`__ version 8.60 or newer

Note: prior to qpdf 11, image comparison tests were enabled within
:file:`qpdf.test`, and you had to *disable* them by setting
``QPDF_SKIP_TEST_COMPARE_IMAGES`` to ``1``. This was done
automatically by ``./configure``. Now you have to *enable* image
comparison tests by setting an environment variable. This change was
made because developers have to set the environment variable
themselves now rather than setting it through the build. Either way,
they are off by default.

Additional Requirements on Windows
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- To build qpdf with Visual Studio, there are no additional
  requirements when the default cmake options are used. You can build
  qpdf from a Visual C++ command-line shell.

- To build with mingw, MSYS2 is recommended with the mingw32 and/or
  mingw64 tool chains. You can also build with MSVC from an MSYS2
  environment.

- qpdf's test suite can run within the MSYS2 environment for both
  mingw and MSVC-based builds.

For additional notes, see :file:`README-windows.md` in the source
distribution.

Requirements for Building Documentation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The qpdf manual is written in reStructured Text and built with `Sphinx
<https://www.sphinx-doc.org>`__ using the `Read the Docs Sphinx Theme
<https://sphinx-rtd-theme.readthedocs.io>`__. Versions of sphinx prior
to version 4.3.2 probably won't work. Sphinx requires Python 3. In
order to build the HTML documentation from source, you need to install
sphinx and the theme, which you can typically do with ``pip install
sphinx sphinx_rtd_theme``. To build the PDF version of the
documentation, you need ``pdflatex``, ``latexmk``, and a fairly
complete LaTeX installation. Detailed requirements can be found in the
Sphinx documentation. To see how the documentation is built for the
qpdf distribution, refer to the :file:`build-scripts/build-doc` file
in the qpdf source distribution.

.. _building:

Build Instructions
------------------

Starting with qpdf 11, qpdf is built with `CMake
<https://www.cmake.org>`__.

Basic Build Invocation
~~~~~~~~~~~~~~~~~~~~~~

qpdf uses cmake in an ordinary way, so refer to the CMake
documentation for details about how to run ``cmake``. Here is a
brief summary.

You can usually just run

::

   cmake -S . -B build
   cmake --build build

If you are using a multi-configuration generator such as MSVC, you
should pass ``--config <Config>`` (where ``<Config>`` is ``Release``,
``Debug``, ``RelWithDebInfo``, or ``MinSizeRel`` as discussed in the
CMake documentation) to the *build* command. If you are running a
single configuration generator such as the default Makefile generators
in Linux or MSYS, you may want to pass ``-DCMAKE_BUILD_TYPE=<Config>``
to the original ``cmake`` command.

Run ``ctest`` to run the test suite. Since the real tests are
implemented with `qtest <https://qtest.sourceforge.io/>`__, you will
want to pass ``--verbose`` to ``cmake`` so you can see the individual
test outputs. Otherwise, you will see a small number of ``ctest``
commands that take a very long to run. If you want to run only a
specific test file in a specific test suite, you can set the ``TESTS``
environment variable (used by ``qtest-driver``) and pass the ``-R``
parameter to ``ctest``. For example:

::

   TESTS=qutil ctest --verbose -R libtests

would run only ``qutil.test`` from the ``libtests`` test suite.


.. _installation:

Installation and Packaging
~~~~~~~~~~~~~~~~~~~~~~~~~~

Installation can be performed using ``cmake --install`` or ``cpack``.
For most normal use cases, ``cmake --install`` or ``cpack`` can be run
in the normal way as described in CMake documentation. qpdf follows
all normal installation conventions and uses CMake-defined variables
for standard behavior.

There are several components that can be installed separately:

.. list-table:: Installation Components
   :widths: 5 80
   :header-rows: 0

   - - cli
     - Command-line tools

   - - lib
     - The runtime libraries; required if you built with shared
       libraries

   - - dev
     - Static libraries, header files, and other files needed by
       developers

   - - doc
     - Documentation and, if selected for installation, the manual

   - - examples
     - Example source files

Note that the ``lib`` component installs only runtime libraries, not
header files or other files/links needed to build against qpdf. For
that, you need ``dev``. If you are using shared libraries, the ``dev``
will install files or create symbolic links that depend on files
installed by ``lib``, so you will need to install both. If you wanted
to build software against the qpdf library and only wanted to install
the files you needed for that purpose, here are some examples:

- Install development files with static libraries only:

  ::

     cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_SHARED_LIBS=OFF
     cmake --build build --parallel --target libqpdf
     cmake --install build --component dev

- Install development files with shared libraries only:

  ::

     cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_STATIC_LIBS=OFF
     cmake --build build --parallel --target libqpdf
     cmake --install build --component lib
     cmake --install build --component dev


- Install development files with shared and static libraries:

  ::

     cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
     cmake --build build --parallel --target libqpdf libqpdf_static
     cmake --install build --component lib
     cmake --install build --component dev

There are also separate options, discussed in :ref:`build-options`,
that control how certain specific parts of the software are installed.

.. _build-options:

Build Options
-------------

.. last verified consistent with build: 2022-03-13. The top-level
   CMakeLists.txt contains a comment that references this section.

.. cSpell:ignore ccmake

All available build options are defined in the the top-level
:file:`CMakeLists.txt` file and have help text. You can see them using
any standard cmake front-end (like ``cmake-gui`` or ``ccmake``). This
section describes options that apply to most users. If you are trying
to map autoconf options (from prior to qpdf 11) to cmake options,
please see :ref:`autoconf-to-cmake`.

If you are packaging qpdf for a distribution, you should also read
:ref:`packaging`.

Basic Build Options
~~~~~~~~~~~~~~~~~~~

BUILD_DOC
  Whether to build documentation with sphinx. You must have the
  required tools installed.

BUILD_DOC_HTML
  Visible when BUILD_DOC is selected. This option controls building
  HTML documentation separately from PDF documentation since
  the sphinx theme is only needed for the HTML documentation.

BUILD_DOC_PDF
  Visible when BUILD_DOC is selected. This option controls building
  PDF documentation separately from HTML documentation since
  additional tools are required to build the PDF documentation.

BUILD_SHARED_LIBS, BUILD_STATIC_LIBS
  You can configure whether to build shared libraries, static
  libraries, or both. You must select at least one of these options.
  For rapid iteration, select only one as this cuts the build time in
  half.

  On Windows, if you build with shared libraries, you must have the
  output directory for libqpdf (e.g. :file:`libqpdf/Release` or
  :file:`libqpdf` within the build directory) in your path so that the
  compiled executables can find the DLL. Updating your path is not
  necessary if you build with static libraries only.

QTEST_COLOR
  Turn this on or off to control whether qtest uses color in its
  output.

Options for Working on qpdf
~~~~~~~~~~~~~~~~~~~~~~~~~~~

CHECK_SIZES
  The source file :file:`qpdf/sizes.cc` is used to display the sizes
  of all objects in the public API. Consistency of its output between
  releases is used as part of the check against accidental breakage of
  the binary interface (ABI). Turning this on causes a test to be run
  that ensures an exact match between classes in ``sizes.cc`` and
  classes in the library's public API. This option requires Python 3.

ENABLE_QTC
  This is off by default, except in maintainer mode. When off,
  ``QTC::TC`` calls are compiled out by having ``QTC::TC`` be an empty
  inline function. The underlying ``QTC::TC`` remains in the library,
  so it is possible to build and package the qpdf library with
  ``ENABLE_QTC`` turned off while still allowing developer code to use
  ``QTC::TC`` if desired. If you are modifying qpdf code, it's a good
  idea to have this on for more robust automated testing. Otherwise,
  there's no reason to have it on.

GENERATE_AUTO_JOB
  Some qpdf source files are automatically generated from
  :file:`job.yml` and the CLI documentation. If you are adding new
  command-line arguments to the qpdf CLI or updating
  :file:`manual/cli.rst` in the qpdf sources, you should turn this on.
  This option requires Python 3.

WERROR
  Make any compiler warnings into errors. We want qpdf to compile free
  of warnings whenever possible, but there's always a chance that a
  compiler upgrade or tool change may cause warnings to appear that
  weren't there before. If you are testing qpdf with a new compiler,
  you should turn this on.

Environment-Specific Options
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

SHOW_FAILED_TEST_OUTPUT
  Ordinarily, qtest (which drives qpdf's test suite) writes detailed
  information about its output to the file ``qtest.log`` in the build
  output directory. If you are running a build in a continuous
  integration or automated environment where you can't get to those
  files, you should enable this option and also run ``ctest
  --verbose`` or ``ctest --output-on-failure``. This will cause
  detailed test failure output to be written into the build log.

CI_MODE
  Turning this on sets options used in qpdf's continuous integration
  environment to ensure we catch as many problems as possible.
  Specifically, this option enables ``SHOW_FAILED_TEST_OUTPUT`` and
  ``WERROR`` and forces the native crypto provider to be built.

MAINTAINER_MODE
  Turning this option on sets options that should be on if you are
  maintaining qpdf. In turns on the following:

  - ``BUILD_DOC``

  - ``CHECK_SIZES``

  - ``ENABLE_QTC``

  - ``GENERATE_AUTO_JOB``

  - ``WERROR``

  - ``REQUIRE_NATIVE_CRYPTO``

  It is possible to turn ``BUILD_DOC`` off in maintainer mode so that
  the extra requirements for building documentation don't have to be
  available.

.. _crypto.build:

Build-time Crypto Selection
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Since version 9.1.0, qpdf can use external crypto providers in
addition to its native provider. For a general discussion, see
:ref:`crypto`. This section discusses how to configure which crypto
providers are compiled into qpdf.

In nearly all cases, external crypto providers should be preferred
over the native one. However, if you are not concerned about working
with encrypted files and want to reduce the number of dependencies,
the native crypto provider is fully supported.

By default, qpdf's build enables every external crypto providers whose
dependencies are available and only enables the native crypto provider
if no external providers are available. You can change this behavior
with the options described here.

USE_IMPLICIT_CRYPTO
  This is on by default. If turned off, only explicitly selected
  crypto providers will be built. You must use at least one of the
  ``REQUIRE`` options below.

ALLOW_CRYPTO_NATIVE
  This option is only available when USE_IMPLICIT_CRYPTO is selected,
  in which case it is on by default. Turning it off prevents qpdf from
  falling back to the native crypto provider when no external provider
  is available.

REQUIRE_CRYPTO_NATIVE
  Build the native crypto provider even if other options are
  available.

REQUIRE_CRYPTO_GNUTLS
  Require the gnutls crypto provider. Turning this on makes in an
  error if the gnutls library is not available.

REQUIRE_CRYPTO_OPENSSL
  Require the openssl crypto provider. Turning this on makes in an
  error if the openssl library is not available.

DEFAULT_CRYPTO
  Explicitly select which crypto provider is used by default. See
  :ref:`crypto.runtime` for information about run-time selection of
  the crypto provider. If not specified, qpdf will pick gnutls if
  available, otherwise openssl if available, and finally native as a
  last priority.

Example: if you wanted to build with only the gnutls crypto provider,
you should run cmake with ``-DUSE_IMPLICIT_CRYPTO=0
-DREQUIRE_CRYPTO_GNUTLS=1``.

Advanced Build Options
~~~~~~~~~~~~~~~~~~~~~~

These options are used only for special purposes and are not relevant
to most users.

AVOID_WINDOWS_HANDLE
  Disable use of the ``HANDLE`` type in Windows. This can be useful if
  you are building for certain embedded Windows environments. Some
  functionality won't work, but you can still process PDF files from
  memory in this configuration.

BUILD_DOC_DIST, INSTALL_MANUAL
  By default, installing qpdf does not include a pre-built copy of the
  manual. Instead, it installs a README file that tells people where
  to find the manual online. If you want to install the manual, you
  must enable the ``INSTALL_MANUAL`` option, and you must have a
  ``doc-dist`` directory in the manual directory of the build. The
  ``doc-dist`` directory is created if ``BUILD_DOC_DIST`` is selected
  and ``BUILD_DOC_PDF`` and ``BUILD_DOC_HTML`` are both on.

  The ``BUILD_DOC_DIST`` and ``INSTALL_MANUAL`` options are separate
  and independent because of the additional tools required to build
  documentation. In particular, for qpdf's official release
  preparation, a ``doc-dist`` directory is built in Linux and then
  extracted into the Windows builds so that it can be included in the
  Windows installers. This prevents us from having to build the
  documentation in a Windows environment. For additional discussion,
  see :ref:`doc-packaging-rationale`.

INSTALL_CMAKE_PACKAGE
  Controls whether or not to install qpdf's cmake configuration file
  (on by default).

INSTALL_EXAMPLES
  Controls whether or not to install qpdf's example source files with
  documentation (on by default).

INSTALL_PKGCONFIG
  Controls whether or not to install qpdf's pkg-config configuration
  file (on by default).

OSS_FUZZ
  Turning this option on changes the build of the fuzzers in a manner
  specifically required by Google's oss-fuzz project. There is no
  reason to turn this on for any other reason. It is enabled by the
  build script that builds qpdf from that context.

SKIP_OS_SECURE_RANDOM, USE_INSECURE_RANDOM
  The native crypto implementation uses the operating systems's secure
  random number source when available. It is not used when an external
  crypto provider is in use. If you are building in a very specialized
  environment where you are not using an external crypto provider but
  can't use the OS-provided secure random number generator, you can
  turn both of these options on. This will cause qpdf to fall back to
  an insecure random number generator, which may generate guessable
  random numbers. The resulting qpdf is still secure, but encrypted
  files may be more subject to brute force attacks. Unless you know
  you need these options for a specialized purpose, you don't need
  them. These options were added to qpdf in response to a special
  request from a user who needed to run a specialized PDF-related task
  in an embedded environment that didn't have a secure random number
  source.

Building without wchar_t
~~~~~~~~~~~~~~~~~~~~~~~~

It is possible to build qpdf on a system that doesn't have
``wchar_t``. The resulting build of qpdf is not API-compatible with a
regular qpdf build, so this option cannot be selected from cmake. This
option was added to qpdf to support installation on a very stripped
down embedded environment that included only a partial implementation
of the standard C++ library.

You can disable use of ``wchar_t`` in qpdf's code by defining the
``QPDF_NO_WCHAR_T`` preprocessor symbol in your build (e.g. by
including ``-DQPDF_NO_WCHAR_T`` in ``CFLAGS`` and ``CXXFLAGS``).

While ``wchar_t`` is part of the C++ standard library and should be
present on virtually every system, there are some stripped down
systems, such as those targeting certain embedded environments, that
lack ``wchar_t``. Internally, qpdf uses UTF-8 encoding for everything,
so there is nothing important in qpdf's API that uses ``wchar_t``.
However, there are some helper methods for converting between
``wchar_t*`` and ``char*``.

If you are building in an environment that does not support
``wchar_t``, you can define the preprocessor symbol
``QPDF_NO_WCHAR_T`` in your build. This will work whether you are
building qpdf and need to avoid compiling the code that uses wchar_t
or whether you are building client code that uses qpdf.

Note that, when you build code with libqpdf, it is *not necessary* to
have the definition of ``QPDF_NO_WCHAR_T`` in your build match what
was defined when the library was built as long as you are not calling
any of the methods that use ``wchar_t``.

.. _crypto:

Crypto Providers
----------------

Starting with qpdf 9.1.0, the qpdf library can be built with multiple
implementations of providers of cryptographic functions, which we refer
to as "crypto providers." At the time of writing, a crypto
implementation must provide MD5 and SHA2 (256, 384, and 512-bit) hashes
and RC4 and AES256 with and without CBC encryption. In the future, if
digital signature is added to qpdf, there may be additional requirements
beyond this. Some of these are weak cryptographic algorithms. For a
discussion of why they're needed, see :ref:`weak-crypto`.

The available crypto provider implementations are ``gnutls``,
``openssl``, and ``native``. OpenSSL support was added in qpdf 10.0.0
with support for OpenSSL added in 10.4.0. GnuTLS support was
introduced in qpdf 9.1.0. Additional implementations can be added as
needed. It is also possible for a developer to provide their own
implementation without modifying the qpdf library.

For information about selecting which crypto providers are compiled
into qpdf, see :ref:`crypto.build`.

.. _crypto.runtime:

Runtime Crypto Provider Selection
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You can use the :qpdf:ref:`--show-crypto` option to :command:`qpdf` to
get a list of available crypto providers. The default provider is
always listed first, and the rest are listed in lexical order. Each
crypto provider is listed on a line by itself with no other text,
enabling the output of this command to be used easily in scripts.

You can override which crypto provider is used by setting the
``QPDF_CRYPTO_PROVIDER`` environment variable. There are few reasons
to ever do this, but you might want to do it if you were explicitly
trying to compare behavior of two different crypto providers while
testing performance or reproducing a bug. It could also be useful for
people who are implementing their own crypto providers.

.. _crypto.develop:

Crypto Provider Information for Developers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If you are writing code that uses libqpdf and you want to force a
certain crypto provider to be used, you can call the method
``QPDFCryptoProvider::setDefaultProvider``. The argument is the name
of a built-in or developer-supplied provider. To add your own crypto
provider, you have to create a class derived from ``QPDFCryptoImpl``
and register it with ``QPDFCryptoProvider``. For additional
information, see comments in :file:`include/qpdf/QPDFCryptoImpl.hh`.

.. _crypto.design:

Crypto Provider Design Notes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This section describes a few bits of rationale for why the crypto
provider interface was set up the way it was. You don't need to know any
of this information, but it's provided for the record and in case it's
interesting.

As a general rule, I want to avoid as much as possible including large
blocks of code that are conditionally compiled such that, in most
builds, some code is never built. This is dangerous because it makes it
very easy for invalid code to creep in unnoticed. As such, I want it to
be possible to build qpdf with all available crypto providers, and this
is the way I build qpdf for local development. At the same time, if a
particular packager feels that it is a security liability for qpdf to
use crypto functionality from other than a library that gets
considerable scrutiny for this specific purpose (such as gnutls,
openssl, or nettle), then I want to give that packager the ability to
completely disable qpdf's native implementation. Or if someone wants to
avoid adding a dependency on one of the external crypto providers, I
don't want the availability of the provider to impose additional
external dependencies within that environment. Both of these are
situations that I know to be true for some users of qpdf.

I want registration and selection of crypto providers to be thread-safe,
and I want it to work deterministically for a developer to provide their
own crypto provider and be able to set it up as the default. This was
the primary motivation behind requiring C++-11 as doing so enabled me to
exploit the guaranteed thread safety of local block static
initialization. The ``QPDFCryptoProvider`` class uses a singleton
pattern with thread-safe initialization to create the singleton instance
of ``QPDFCryptoProvider`` and exposes only static methods in its public
interface. In this way, if a developer wants to call any
``QPDFCryptoProvider`` methods, the library guarantees the
``QPDFCryptoProvider`` is fully initialized and all built-in crypto
providers are registered. Making ``QPDFCryptoProvider`` actually know
about all the built-in providers may seem a bit sad at first, but this
choice makes it extremely clear exactly what the initialization behavior
is. There's no question about provider implementations automatically
registering themselves in a nondeterministic order. It also means that
implementations do not need to know anything about the provider
interface, which makes them easier to test in isolation. Another
advantage of this approach is that a developer who wants to develop
their own crypto provider can do so in complete isolation from the qpdf
library and, with just two calls, can make qpdf use their provider in
their application. If they decided to contribute their code, plugging it
into the qpdf library would require a very small change to qpdf's source
code.

The decision to make the crypto provider selectable at runtime was one I
struggled with a little, but I decided to do it for various reasons.
Allowing an end user to switch crypto providers easily could be very
useful for reproducing a potential bug. If a user reports a bug that
some cryptographic thing is broken, I can easily ask that person to try
with the ``QPDF_CRYPTO_PROVIDER`` variable set to different values. The
same could apply in the event of a performance problem. This also makes
it easier for qpdf's own test suite to exercise code with different
providers without having to make every program that links with qpdf
aware of the possibility of multiple providers. In qpdf's continuous
integration environment, the entire test suite is run for each supported
crypto provider. This is made simple by being able to select the
provider using an environment variable.

Finally, making crypto providers selectable in this way establish a
pattern that I may follow again in the future for stream filter
providers. One could imagine a future enhancement where someone could
provide their own implementations for basic filters like
``/FlateDecode`` or for other filters that qpdf doesn't support.
Implementing the registration functions and internal storage of
registered providers was also easier using C++-11's functional
interfaces, which was another reason to require C++-11 at this time.

.. _autoconf-to-cmake:

Converting From autoconf to cmake
---------------------------------

Versions of qpdf before qpdf 11 were built with ``autoconf`` and a
home-grown GNU Make-based build system. If you built qpdf with special
``./configure`` options, this section can help you switch them over to
``cmake``.

In most cases, there is a one-to-one mapping between configure options
and cmake options. There are a few exceptions:

- The cmake build behaves differently with respect to whether or not
  to include support for the native crypto provider. Specifically, it
  is not implicitly enabled unless explicitly requested if there are
  other options available. You can force it to be included by enabling
  ``REQUIRE_CRYPTO_NATIVE``. For details, see :ref:`crypto.build`.

- The ``--enable-external-libs`` option is no longer available. The
  cmake build detects the presence of ``external-libs`` automatically.
  See :file:`README-windows.md` in the source distribution for a more
  in-depth discussion.

- The sense of the option representing use of the OS-provided secure
  random number generator has been reversed: the
  ``--enable-os-secure-random``, which was on by default, has been
  replaced by the ``SKIP_OS_SECURE_RANDOM`` option, which is off by
  default. The option's new name and behavior match the preprocessor
  symbol that it turns on.

- Non-default test configuration is selected with environment
  variables rather than cmake. The old ``./configure`` options just
  set environment variables. Note that the sense of the variable for
  image comparison tests has been reversed. It used to be that you had
  to set ``QPDF_SKIP_TEST_COMPARE_IMAGES`` to ``1`` to *disable* image
  comparison tests. This was done by default. Now you have to set
  ``QPDF_TEST_COMPARE_IMAGES`` to ``1`` to *enable* image comparison
  tests. Either way, they are off by default.

- Non-user-visible change: the preprocessor symbol that triggers the
  export of functions into the public ABI (application binary
  interface) has been changed from ``DLL_EXPORT`` to
  ``libqpdf_EXPORTS``. This detail is encapsulated in the build and is
  only relevant to people who are building qpdf on their own or who
  may have previously needed to work around a collision between qpdf's
  use of ``DLL_EXPORT`` and someone else's use of the same symbol.

- A handful of options that were specific to autoconf or the old build
  system have been dropped.

- ``cmake --install`` installs example source code in
  ``doc/qpdf/examples`` in the ``examples`` installation component.
  Packagers are encouraged to package this with development files if
  there is no separate doc package. This can be turned off by
  disabling the ``INSTALL_EXAMPLES`` build option.

There are some new options available in the cmake build that were not
available in the autoconf build. This table shows the old options and
their equivalents in cmake.

.. list-table:: configure flags to cmake options
   :widths: 40 60
   :header-rows: 0

   - - enable-avoid-windows-handle
     - AVOID_WINDOWS_HANDLE

   - - enable-check-autofiles
     - none -- not relevant to cmake

   - - enable-crypto-gnutls
     - REQUIRE_CRYPTO_GNUTLS

   - - enable-crypto-native
     - REQUIRE_CRYPTO_NATIVE (but see above)

   - - enable-crypto-openssl
     - REQUIRE_CRYPTO_OPENSSL

   - - enable-doc-maintenance
     - BUILD_DOC

   - - enable-external-libs
     - none -- detected automatically

   - - enable-html-doc
     - BUILD_DOC_HTML

   - - enable-implicit-crypto
     - USE_IMPLICIT_CRYPTO

   - - enable-insecure-random
     - USE_INSECURE_RANDOM

   - - enable-ld-version-script
     - none -- detected automatically

   - - enable-maintainer-mode
     - MAINTAINER_MODE (slight differences)

   - - enable-os-secure-random (on by default)
     - SKIP_OS_SECURE_RANDOM (off by default)

   - - enable-oss-fuzz
     - OSS_FUZZ

   - - enable-pdf-doc
     - BUILD_DOC_PDF

   - - enable-rpath
     - none -- cmake handles rpath correctly

   - - enable-show-failed-test-output
     - SHOW_FAILED_TEST_OUTPUT

   - - enable-test-compare-images
     - set the ``QPDF_TEST_COMPARE_IMAGES`` environment variable

   - - enable-werror
     - WERROR

   - - with-buildrules
     - none -- not relevant to cmake

   - - with-default-crypto
     - DEFAULT_CRYPTO

   - - large-file-test-path
     - set the ``QPDF_LARGE_FILE_TEST_PATH`` environment variable
