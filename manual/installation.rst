.. _installing:

Building and Installing QPDF
============================

This chapter describes how to build and install qpdf. Please see also
the :file:`README.md` and
:file:`INSTALL` files in the source distribution.

.. _prerequisites:

System Requirements
-------------------

The qpdf package has few external dependencies. In order to build qpdf,
the following packages are required:

- A C++ compiler that supports C++-14.

- zlib: http://www.zlib.net/

- jpeg: http://www.ijg.org/files/ or https://libjpeg-turbo.org/

- *Recommended but not required:* gnutls: https://www.gnutls.org/ to be
  able to use the gnutls crypto provider, and/or openssl:
  https://openssl.org/ to be able to use the openssl crypto provider.

- gnu make 3.81 or newer: http://www.gnu.org/software/make

- perl version 5.8 or newer: http://www.perl.org/; required for running
  the test suite. Starting with qpdf version 9.1.1, perl is no longer
  required at runtime.

- GNU diffutils (any version): http://www.gnu.org/software/diffutils/
  is required to run the test suite. Note that this is the version of
  diff present on virtually all GNU/Linux systems. This is required
  because the test suite uses :command:`diff -u`.

Part of qpdf's test suite does comparisons of the contents PDF files by
converting them images and comparing the images. The image comparison
tests are disabled by default. Those tests are not required for
determining correctness of a qpdf build if you have not modified the
code since the test suite also contains expected output files that are
compared literally. The image comparison tests provide an extra check to
make sure that any content transformations don't break the rendering of
pages. Transformations that affect the content streams themselves are
off by default and are only provided to help developers look into the
contents of PDF files. If you are making deep changes to the library
that cause changes in the contents of the files that qpdf generate,
then you should enable the image comparison tests. Enable them by
running :command:`configure` with the
:samp:`--enable-test-compare-images` flag. If you enable
this, the following additional requirements are required by the test
suite. Note that in no case are these items required to use qpdf.

- libtiff: http://www.remotesensing.org/libtiff/

- GhostScript version 8.60 or newer: http://www.ghostscript.com

If you do not enable this, then you do not need to have tiff and
ghostscript.

Pre-built documentation is distributed with qpdf, so you should
generally not need to rebuild the documentation. In order to build the
documentation from source, you need to install `Sphinx
<https://sphinx-doc.org>`__. To build the PDF version of the
documentation, you need ``pdflatex``, ``latexmk``, and a fairly complete
LaTeX installation. Detailed requirements can be found in the Sphinx
documentation.

.. _building:

Build Instructions
------------------

Building qpdf on UNIX is generally just a matter of running

::

   ./configure
   make

You can also run :command:`make check` to run the test
suite and :command:`make install` to install. Please run
:command:`./configure --help` for options on what can be
configured. You can also set the value of ``DESTDIR`` during
installation to install to a temporary location, as is common with many
open source packages. Please see also the
:file:`README.md` and
:file:`INSTALL` files in the source distribution.

Building on Windows is a little bit more complicated. For details,
please see :file:`README-windows.md` in the source
distribution. You can also download a binary distribution for Windows.
There is a port of qpdf to Visual C++ version 6 in the
:file:`contrib` area generously contributed by Jian
Ma. This is also discussed in more detail in
:file:`README-windows.md`.

While ``wchar_t`` is part of the C++ standard, qpdf uses it in only one
place in the public API, and it's just in a helper function. It is
possible to build qpdf on a system that doesn't have ``wchar_t``, and
it's also possible to compile a program that uses qpdf on a system
without ``wchar_t`` as long as you don't call that one method. This is a
very unusual situation. For a detailed discussion, please see the
top-level README.md file in qpdf's source distribution.

There are some other things you can do with the build. Although qpdf
uses :command:`autoconf`, it does not use
:command:`automake` but instead uses a
hand-crafted non-recursive Makefile that requires gnu make. If you're
really interested, please read the comments in the top-level
:file:`Makefile`.

.. _crypto:

Crypto Providers
----------------

Starting with qpdf 9.1.0, the qpdf library can be built with multiple
implementations of providers of cryptographic functions, which we refer
to as "crypto providers." At the time of writing, a crypto
implementation must provide MD5 and SHA2 (256, 384, and 512-bit) hashes
and RC4 and AES256 with and without CBC encryption. In the future, if
digital signature is added to qpdf, there may be additional requirements
beyond this.

Starting with qpdf version 9.1.0, the available implementations are
``native`` and ``gnutls``. In qpdf 10.0.0, ``openssl`` was added.
Additional implementations may be added if needed. It is also possible
for a developer to provide their own implementation without modifying
the qpdf library.

.. _crypto.build:

Build Support For Crypto Providers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When building with qpdf's build system, crypto providers can be enabled
at build time using various :command:`./configure`
options. The default behavior is for
:command:`./configure` to discover which crypto providers
can be supported based on available external libraries, to build all
available crypto providers, and to use an external provider as the
default over the native one. This behavior can be changed with the
following flags to :command:`./configure`:

- :samp:`--enable-crypto-{x}`
  (where :samp:`{x}` is a supported crypto
  provider): enable the :samp:`{x}` crypto
  provider, requiring any external dependencies it needs

- :samp:`--disable-crypto-{x}`:
  disable the :samp:`{x}` provider, and do not
  link against its dependencies even if they are available

- :samp:`--with-default-crypto={x}`:
  make :samp:`{x}` the default provider even if
  a higher priority one is available

- :samp:`--disable-implicit-crypto`: only build crypto
  providers that are explicitly requested with an
  :samp:`--enable-crypto-{x}`
  option

For example, if you want to guarantee that the gnutls crypto provider is
used and that the native provider is not built, you could run
:command:`./configure --enable-crypto-gnutls
--disable-implicit-crypto`.

If you build qpdf using your own build system, in order for qpdf to work
at all, you need to enable at least one crypto provider. The file
:file:`libqpdf/qpdf/qpdf-config.h.in` provides
macros ``DEFAULT_CRYPTO``, whose value must be a string naming the
default crypto provider, and various symbols starting with
``USE_CRYPTO_``, at least one of which has to be enabled. Additionally,
you must compile the source files that implement a crypto provider. To
get a list of those files, look at
:file:`libqpdf/build.mk`. If you want to omit a
particular crypto provider, as long as its ``USE_CRYPTO_`` symbol is
undefined, you can completely ignore the source files that belong to a
particular crypto provider. Additionally, crypto providers may have
their own external dependencies that can be omitted if the crypto
provider is not used. For example, if you are building qpdf yourself and
are using an environment that does not support gnutls or openssl, you
can ensure that ``USE_CRYPTO_NATIVE`` is defined, ``USE_CRYPTO_GNUTLS``
is not defined, and ``DEFAULT_CRYPTO`` is defined to ``"native"``. Then
you must include the source files used in the native implementation,
some of which were added or renamed from earlier versions, to your
build, and you can ignore
:file:`QPDFCrypto_gnutls.cc`. Always consult
:file:`libqpdf/build.mk` to get the list of source
files you need to build.

.. _crypto.runtime:

Runtime Crypto Provider Selection
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You can use the :samp:`--show-crypto` option to
:command:`qpdf` to get a list of available crypto
providers. The default provider is always listed first, and the rest are
listed in lexical order. Each crypto provider is listed on a line by
itself with no other text, enabling the output of this command to be
used easily in scripts.

You can override which crypto provider is used by setting the
``QPDF_CRYPTO_PROVIDER`` environment variable. There are few reasons to
ever do this, but you might want to do it if you were explicitly trying
to compare behavior of two different crypto providers while testing
performance or reproducing a bug. It could also be useful for people who
are implementing their own crypto providers.

.. _crypto.develop:

Crypto Provider Information for Developers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If you are writing code that uses libqpdf and you want to force a
certain crypto provider to be used, you can call the method
``QPDFCryptoProvider::setDefaultProvider``. The argument is the name of
a built-in or developer-supplied provider. To add your own crypto
provider, you have to create a class derived from ``QPDFCryptoImpl`` and
register it with ``QPDFCryptoProvider``. For additional information, see
comments in :file:`include/qpdf/QPDFCryptoImpl.hh`.

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

.. _packaging:

Notes for Packagers
-------------------

If you are packaging qpdf for an operating system distribution, here are
some things you may want to keep in mind:

- Starting in qpdf version 9.1.1, qpdf no longer has a runtime
  dependency on perl. This is because fix-qdf was rewritten in C++.
  However, qpdf still has a build-time dependency on perl.

- Make sure you are getting the intended behavior with regard to crypto
  providers. Read :ref:`crypto.build` for details.

- Passing :samp:`--enable-show-failed-test-output` to
  :command:`./configure` will cause any failed test
  output to be written to the console. This can be very useful for
  seeing test failures generated by autobuilders where you can't access
  qtest.log after the fact.

- If qpdf's build environment detects the presence of autoconf and
  related tools, it will check to ensure that automatically generated
  files are up-to-date with recorded checksums and fail if it detects a
  discrepancy. This feature is intended to prevent you from
  accidentally forgetting to regenerate automatic files after modifying
  their sources. If your packaging environment automatically refreshes
  automatic files, it can cause this check to fail. Suppress qpdf's
  checks by passing :samp:`--disable-check-autofiles`
  to :command:`/.configure`. This is safe since qpdf's
  :command:`autogen.sh` just runs autotools in the
  normal way.

- QPDF's :command:`make install` does not install
  completion files by default, but as a packager, it's good if you
  install them wherever your distribution expects such files to go. You
  can find completion files to install in the
  :file:`completions` directory.

- Packagers are encouraged to install the source files from the
  :file:`examples` directory along with qpdf
  development packages.
