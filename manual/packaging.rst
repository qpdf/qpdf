.. _packaging:

Notes for Packagers
===================

If you are packaging qpdf for an operating system distribution, this
chapter is for you. Otherwise, feel free to skip.

Build Options
-------------

For a detailed discussion of build options, please refer to
:ref:`build-options`. This section calls attention to options that are
particularly useful to packagers.

- Perl must be present at build time. Prior to qpdf version 9.1.1,
  there was a runtime dependency on perl, but this is no longer the
  case.

- Make sure you are getting the intended behavior with regard to crypto
  providers. Read :ref:`crypto.build` for details.

- Use of ``SHOW_FAILED_TEST_OUTPUT`` is recommended for building in
  continuous integration or other automated environments as it makes
  it possible to see test failures in build logs. This should be
  combined with either ``ctest --verbose`` or ``ctest
  --output-on-failure``.

- qpdf's install targets do not install completion files by default
  since there is no standard location for them. As a packager, it's
  good if you install them wherever your distribution expects such
  files to go. You can find completion files to install in the
  :file:`completions` directory. See the :file:`completions/README.md`
  file for more information.

- Starting with qpdf 11, qpdf's default installation installs source
  files from the examples directory with documentation. Prior to qpdf
  11, this was a recommendation for packagers but was not done
  automatically.

.. _package-tests:

Package Tests
-------------

The :file:`pkg-test` directory contains very small test shell scripts
that are designed to help smoke-test an installation of qpdf. They
were designed to be used with debian's `autopkgtest
<https://wiki.debian.org/ContinuousIntegration/autopkgtest>`__
framework but can be used by others. Please see
:file:`pkg-test/README.md` in the source distribution for details.

.. _packaging-doc:

Packaging Documentation
-----------------------

Starting in qpdf version 10.5, pre-built documentation is no longer
distributed with the qpdf source distribution. Here are a few options
you may want to consider for your packages:

- **Do nothing**

  When you run ``make install``, the file :file:`README-doc.txt` is
  installed in the documentation directory. That file tells the reader
  where to find the documentation online and where to go to download
  offline copies of the documentation. This is the option selected by
  the debian packages.

- **Embed pre-built documentation**

  You can obtain pre-built documentation and extract its contents into
  your distribution. This is what the Windows binary distributions
  available from the qpdf release site do. You can find the pre-built
  documentation in the release area in the file
  :file:`qpdf-{version}-doc.zip`. For an example of this approach,
  look at qpdf's GitHub actions build scripts. The
  :file:`build-scripts/build-doc` script builds with
  ``-DBUILD_DOC_DIST=1`` to create the documentation distribution. The
  :file:`build-scripts/build-windows` script extracts it into the
  build tree and builds with ``-DINSTALL_MANUAL=1`` to include it in
  the installer.

- **Build the documentation yourself**

  You can build the documentation as part of your build process. Be
  sure to pass ``-DBUILD_DOC_DIST=1`` and ``-DINSTALL_MANUAL=1`` to
  cmake. This is what the AppImage build does. The latest version of
  Sphinx at the time of the initial conversion a sphinx-based
  documentation was 4.3.2. Older versions are not guaranteed to work.

.. _doc-packaging-rationale:

Documentation Packaging Rationale
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This section describes the reason for things being the way they are.
It's for information only; you don't have to know any of this to
package qpdf.

What is the reason for this change? Prior to qpdf 10.5, the qpdf
manual was a docbook XML file. The generated documents were the
product of running the file through build-time style sheets and
contained no copyrighted material of their own. Starting with version
10.5, the manual is written in reStructured Text and built with `Sphinx
<https://www.sphinx-doc.org>`__. This change was made to make it much
easier to automatically generate portions of the documentation and to
make the documentation easier to work with. The HTML output of Sphinx
is also much more readable, usable, and suitable for online
consumption than the output of the docbook style sheets. The downsides
are that the generated HTML documentation now contains Javascript code
and embedded fonts, and the PDF version of the documentation is no
longer as suitable for printing (at least as of the 10.5 distribution)
since external link targets are no longer shown and cross references
no longer contain page number information. The presence of copyrighted
material in the generated documentation, even though things are
licensed with MIT and BSD licenses, complicates the job of the
packager in various ways. For one thing, it means the
:file:`NOTICE.md` file in the source repository would have to keep up
with the copyright information for files that are not controlled in
the repository. Additionally, some distributions (notably
Debian/Ubuntu) discourage inclusion of sphinx-generated documentation
in packages, preferring you instead to build the documentation as part
of the package build process and to depend at runtime on a shared
package that contains the code. At the time of the conversion of the
qpdf manual from docbook to sphinx, newer versions of both sphinx and
the html theme were required than were available in some of most of
the Debian/Ubuntu versions for which qpdf was packaged.

Since always-on Internet connectivity is much more common than it used
to be, many users of qpdf would prefer to consume the documentation
online anyway, and the lack of pre-built documentation in the
distribution won't be as big of a deal. However there are still some
people who can't or choose not to view documentation online. For them,
pre-built documentation is still available.
