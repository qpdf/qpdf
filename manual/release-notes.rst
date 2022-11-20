.. _release-notes:

Release Notes
=============

For a detailed list of changes, please see the file
:file:`ChangeLog` in the source distribution.

.. x.y.z: not yet released

11.2.0: November 20, 2022
  - Build changes

    - A C++-17 compiler is now required.

  - Library enhancements

    - Move stream creation functions in the ``QPDF`` object where they
      belong. The ones in ``QPDFObjectHandle`` are not deprecated and
      will stick around.

    - Add some convenience methods to ``QPDFTokenizer::Token`` for
      testing token types. This is part of qpdf's lexical layer and
      will not be needed by most developers.

  - Bug fixes

    - Fix issue with missing symbols in the mingw build.

    - Fix major performance bug with the OpenSSL crypto provider. This
      bug was causing a 6x to 12x slowdown for encrypted files when
      OpenSSL 3 was in use. This includes the default Windows builds
      distributed with the qpdf release.

    - Fix obscure bug involving appended files that reuse an object
      number that was used as a cross reference stream in an earlier
      stage of the file.

11.1.1: October 1, 2022
  - Bug fixes

    - Fix edge case with character encoding for strings whose initial
      characters happen to coincide with Unicode markers.

    - Fix issue with AppImage discarding the first command-line
      argument when invoked as the name of one of the embedded
      executables. Also, fix-qdf, for unknown reasons, had the wrong
      runpath and would use a qpdf library that was installed on the
      system.

  - Test improvements

    - Exercise the case of ``char`` being ``unsigned`` by default in
      automated tests.

    - Add AppImage-specific tests to CI to ensure that the AppImage
      works in the various ways it is intended to be invoked.

  - Other changes

    - Include more code tidying and performance improvements from M.
      Holger.

11.1.0: September 14, 2022
  - Build fixes

    - Remove ``LL_FMT`` tests, which were broken for cross
      compilation. The code just uses ``%lld`` now.

    - Some symbols were not properly exported for the Windows DLL
      build.

    - Force project-specific header files to precede all others in the
      build so that a previous qpdf installation won't break building
      qpdf from source.

  - Packaging note omitted from 11.0.0 release notes:

    - On GitHub, the release tags are now ``vX.Y.Z`` instead of
      ``release-qpdf-X.Y.Z`` to be more consistent with current
      practice.

11.0.0: September 10, 2022
  - Replacement of ``PointerHolder`` with ``std::shared_ptr``

    - The qpdf-specific ``PointerHolder`` smart pointer implementation
      has now been completely replaced with ``std::shared_ptr``
      through the qpdf API. Please see :ref:`smart-pointers` for
      details about this change and a comprehensive migration plan.
      Note that a backward-compatible ``PointerHolder`` class is
      provided and is enabled by default. A warning is issued, but
      this can be turned off by following the migration steps outlined
      in the manual.

  - qpdf JSON version 2

    - qpdf's JSON output mode is now at version 2. This fixes several
      flaws with version 1. Version 2 JSON output is unambiguous and
      complete, and bidirectional conversion between JSON and PDF is
      supported. Command-line options and library API are available
      for creating JSON from PDF, creating PDF from JSON and updating
      existing PDF at the object level from JSON.

    - New command-line arguments: :qpdf:ref:`--json-output`,
      :qpdf:ref:`--json-input`, :qpdf:ref:`--update-from-json`

    - New C++ API calls: ``QPDF::writeJSON``,
      ``QPDF::createFromJSON``, ``QPDF::updateFromJSON``

    - New C API calls: ``qpdf_create_from_json_file``,
      ``qpdf_create_from_json_data``, ``qpdf_update_from_json_file``,
      ``qpdf_update_from_json_data``, and ``qpdf_write_json``.

    - Complete documentation can be found at :ref:`json`. A
      comprehensive list of changes from version 1 to version 2 can be
      found at :ref:`json-v2-changes`.

  - Build replaced with cmake

    - The old autoconf-based build has been replaced with CMake. CMake
      version 3.16 or newer is required. For details, please read
      :ref:`installing` and, if you package qpdf for a distribution,
      :ref:`packaging`.

    - For the most part, other than being familiar with generally how to
      build things with cmake, what you need to know to convert your
      build over is described in :ref:`autoconf-to-cmake`. Here are a
      few changes in behavior to be aware of:

      - Example sources are installed by default in the documentation
	directory.

      - The configure options to enable image comparison and large file
	tests have been replaced by environment variables. The old
	options set environment variables behind the scenes. Before, to
	skip image tests, you had to set
	``QPDF_SKIP_TEST_COMPARE_IMAGES=1``, which was done by default.
	Now these are off by default, and you have to set
	``QPDF_TEST_COMPARE_IMAGES=1`` to enable them.

      - In the default configuration, the native crypto provider is only
	selected when explicitly requested or when there are no other
	options. See :ref:`crypto.build` for a detailed discussion.

      - Windows external libraries are detected by default if the
	:file:`external-libraries` directory is found. Static libraries
	for zlib, libjpeg, and openssl are provided as described in
	:file:`README-windows.md`. They are only compatible with
	non-debug builds.

      - A new directory called ``pkg-tests`` has been added which
	contains short shell scripts that can be used to smoke test an
	installed qpdf package. These are used by the debian
	``autopkgtest`` framework but can be used by others. See
	:file:`pkg-test/README.md` for details.

  - Performance improvements

    - Many performance enhancements have been added. In developer
      performance benchmarks, gains on the order of 20% have been
      observed. Most of that work, including major optimization of
      qpdf's lexical and parsing layers, was done by M. Holger.

  - CLI: breaking changes

    - The :qpdf:ref:`--show-encryption` flag now provides encryption
      information even if a correct password is not supplied. If you
      were relying on its not working in this case, see
      :qpdf:ref:`--requires-password` for a reliable test.

    - The default json output version when :qpdf:ref:`--json` is
      specified has been changed from ``1`` to ``latest``, which is
      now ``2``.

    - The :qpdf:ref:`--allow-weak-crypto` flag is now mandatory when
      explicitly creating files with weak cryptographic algorithms.
      See :ref:`weak-crypto` for a discussion.

  - API: breaking changes

    - Deprecate ``QPDFObject.hh`` for removal in qpdf 12. The only use
      case for including ``qpdf/QPDFObject.hh`` was to get
      ``QPDFObject::object_type_e``. Since 10.5.0, this has been an
      alias to ``qpdf_object_type_e``, defined in
      ``qpdf/Constants.h``. To fix your code, replace any includes of
      ``qpdf/QPDFObject.hh`` with ``qpdf/Constants.h``, and replace
      all occurrences of ``QPDFObject::ot_`` with ``::ot_``. If you
      need your code to be backward compatible to qpdf versions prior
      to 10.5.0, you can check that the preprocessor symbol
      ``QPDF_MAJOR_VERSION`` is defined and ``>= 11``. As a stop-gap,
      you can ``#define QPDF_OBJECT_NOWARN`` to suppress the warning.

    - ``Pipeline::write`` now takes ``unsigned char const*`` instead
      of ``unsigned char*``. Callers don't need to change anything,
      but you no longer have to pass writable pointers to pipelines.
      If you've implemented your own pipeline classes, you will need
      to update them.

    - Remove deprecated
      ``QPDFAcroFormDocumentHelper::copyFieldsFromForeignPage``. This
      method never worked and only did something in qpdf version
      10.2.x.

    - Remove deprecated ``QPDFNameTreeObjectHelper`` and
      ``QPDFNumberTreeObjectHelper`` constructors that don't take a
      ``QPDF&`` argument.

    - The function passed to and called by ``QPDFJob::doIfVerbose``
      now takes a ``Pipeline&`` argument instead of a
      ``std::ostream&`` argument.

    - Intentionally break API to call attention to operations that
      write files with insecure encryption:

      - Remove pre qpdf-8.4.0 encryption API methods from ``QPDFWriter``
        and their corresponding C API functions

      - Add ``Insecure`` to the names of some ``QPDFWriter`` methods
        and ``_insecure`` to the names of some C API functions without
        otherwise changing their behavior

      - See :ref:`breaking-crypto-api` for specific details, and see
        :ref:`weak-crypto` for a general discussion.

    - ``QPDFObjectHandle::warnIfPossible`` no longer takes an optional
      argument to throw an exception if there is no description. If
      there is no description, it writes to the default
      ``QPDFLogger``'s error stream. (``QPDFLogger`` is new in qpdf
      11---see below.)

    - ``QPDF`` objects can no longer be copied or assigned to. It has
      never been safe to do this because of assumptions made by
      library code. Now it is prevented by the API. If you run into
      trouble, use ``QPDF::create()`` to create ``QPDF`` shared
      pointers (or create them in some other way if you need backward
      compatibility with older qpdf versions).

  - CLI Enhancements

    - ``qpdf --list-attachments --verbose`` includes some additional
      information about attachments. Additional information about
      attachments is also included in the ``attachments`` JSON key
      with ``--json``.

    - For encrypted files, ``qpdf --json`` reveals the user password
      when the specified password did not match the user password and
      the owner password was used to recover the user password. The
      user password is not recoverable from the owner password when
      256-bit keys are in use.

    - ``--verbose`` and ``--progress`` may be now used when writing
      the output PDF to standard output. In that case, the verbose and
      progress messages are written to standard error.

  - Library Enhancements

    - A new object ``QPDFLogger`` has been added. Details are in
      :file:`include/qpdf/QPDFLogger.hh`.

      - ``QPDF`` and ``QPDFJob`` both use the default logger by
        default but can have their loggers overridden. The
        ``setOutputStreams`` method is deprecated in both classes.

      - A few things from ``QPDFObjectHandle`` that used to be
        exceptions now write errors with the default logger.

      - By configuring the default logger, it is possible to capture
        output and errors that slipped through the cracks with
        ``setOutputStreams``.

      - A C API is available in :file:`include/qpdf/qpdflogger-c.h`.

      - See examples :file:`examples/qpdfjob-save-attachment.cc` and
        :file:`examples/qpdfjob-c-save-attachment.cc`.

    - In ``QPDFObjectHandle``, new methods ``insertItemAndGetNew``,
      ``appendItemAndGetNew``, and ``replaceKeyAndGetNew`` return the
      newly added item. New methods ``eraseItemAndGetOld``,
      ``replaceKeyAndGetOld``, and ``removeKeyAndGetOld`` return the
      item that was just removed or, in the case of
      ``replaceKeyAndGetOld``, a ``null`` object if the object was not
      previously there.

    - The ``QPDFObjectHandle::isDestroyed`` method can be used to
      detect when an indirect object ``QPDFObjectHandle`` belongs to a
      ``QPDF`` that has been destroyed. Any attempt to unparse this
      type of ``QPDFObjectHandle`` will throw a logic error.

    - The ``QPDFObjectHandle::getOwningQPDF`` method now returns a
      null pointer rather than an invalid pointer when the owning
      ``QPDF`` object has been destroyed. Indirect objects whose
      owning ``QPDF`` has been destroyed become invalid. Direct
      objects just lose their owning ``QPDF`` but continue to be
      valid.

    - The method ``QPDFObjectHandle::getQPDF`` is an alternative to
      ``QPDFObjectHandle::getOwningQPDF``. It returns a ``QPDF&``
      rather than a ``QPDF*`` and can be used when the object is known
      to have an owning ``QPDF``. It throws an exception if the object
      does not have an owning ``QPDF``. Only indirect objects are
      guaranteed to have an owning ``QPDF``. Direct objects may have
      one if they were initially read from a PDF input source that is
      still valid, but it's also possible to have direct objects that
      don't have an owning ``QPDF``.

    - Add method ``QPDFObjectHandle::isSameObjectAs`` for testing
      whether two ``QPDFObjectHandle`` objects point to the same
      underlying object, meaning changes to one will be reflected in
      the other. Note that this method does not compare the contents
      of the objects, so two distinct but structurally identical
      objects will not be considered the same object.

    - New factory method ``QPDF::create()`` returns a
      ``std::shared_ptr<QPDF>``.

    - New ``Pipeline`` methods have been added to reduce the amount of
      casting that is needed:

      - ``write``: overloaded version that takes ``char const*`` in
        addition to the one that takes ``unsigned char const*``

      - ``writeCstr``: writes a null-terminated C string

      - ``writeString``: writes a std::string

      - ``operator <<``: for null-terminated C strings, std::strings,
        and integer types

    - New ``Pipeline`` type ``Pl_OStream`` writes to a
      ``std::ostream``.

    - New ``Pipeline`` type ``Pl_String`` appends to a
      ``std::string``.

    - New ``Pipeline`` type ``Pl_Function`` can be used to call an
      arbitrary function on write. It supports ``std::function`` for
      C++ code and can also accept C-style functions that indicate
      success using a return value and take an extra parameter for
      passing user data.

    - Methods have been added to ``QUtil`` for converting PDF
      timestamps and ``QPDFTime`` objects to ISO-8601 timestamps.

    - Enhance JSON class to better support incrementally reading and
      writing large amounts of data without having to keep everything
      in memory.

    - Add new functions to the C API for ``qpdfjob`` that use a
      ``qpdfjob_handle``. Like with the regular C API for qpdf, you
      have to call ``qpdfjob_init`` first, pass the handle to the
      functions, and call ``qpdfjob_cleanup`` at the end. This
      interface offers more flexibility than the old interface, which
      remains available.

    - Add ``QPDFJob::registerProgressReporter`` and
      ``qpdfjob_register_progress_reporter`` to allow a custom
      progress reporter to be used with ``QPDFJob``. The ``QPDFJob``
      object must be configured to report progress (via command-line
      argument or otherwise) for this to be used.

    - Add new overloads to
      ``QPDFObjectHandle::StreamDataProvider::provideStreamData`` that
      take ``QPDFObjGen const&`` instead of separate object ID and
      generation parameters. The old versions will continue to be
      supported and are not deprecated.

    - In ``QPDFPageObjectHelper``, add a ``copy_if_fallback``
      parameter to most of the page bounding box methods, and clarify
      in the comments about the difference between ``copy_if_shared``
      and ``copy_if_fallback``.

    - Add a move constructor to the ``Buffer`` class.

  - Other changes

    - On GitHub, the release tags are now `vX.Y.Z` instead of
      `release-qpdf-X.Y.Z` to be more consistent with current practice.

    - In JSON v1 mode, the ``"objects"`` key now reflects the repaired
      pages tree if ``"pages"`` (or any other key that has the side
      effect of repairing the page tree) is specified. To see the
      original objects with any unrepaired page tree errors, specify
      ``"objects"`` and/or ``"objectinfo"`` by themselves. This is
      consistent with how JSON v2 behaves.

    - A new chapter on contributing to qpdf has been added to the
      documentation. See :ref:`contributing`.

    - The qpdf source code is now formatted automatically with
      ``clang-format``. See :ref:`code-formatting` for information.

    - Test coverage with ``QTC`` is enabled during development but
      compiled out of distributed qpdf binaries by default. This
      results in a significant performance improvement, especially on
      Windows. ``QTC::TC`` is still available in the library and is
      still usable by end user code even though calls to it made
      internally by the library are turned off. Internally, there is
      some additional caching to reduce the overhead of repeatedly
      reading environment variables at runtime.

    - The test files used by the ``performance_check`` script at the
      top of the repository are now available in the
      `qpdf/performance-test-files github repository
      <https://github.com/qpdf/performance-test-files>`__. In addition
      to running time, memory usage is also included in performance
      test results when available. The ``performance_check`` tool has
      only been tested on Linux.

    - Lots of code cleanup and refactoring work was contributed in
      multiple pull requests by M. Holger. This includes the work
      required to enable detection of ``QPDFObjectHandle`` objects
      that belong to destroyed ``QPDF`` objects.

10.6.3: March 8, 2022
  - Announcement of upcoming change:

    - qpdf 11 will be built with cmake. The qpdf 11 documentation will
      include detailed migration instructions.

  - Bug fixes:

    - Recognize strings explicitly encoded as UTF-8 as allowed by the
      PDF 2.0 spec.

    - Fix edge cases with appearance stream generation for form fields
      whose ``/DA`` field lacks proper font size specification or that
      specifies auto sizing. At this time, qpdf does not support auto
      sizing.

    - Minor, non-functional changes to build and documentation to
      accommodate a wider range of compilation environments in
      preparation for migration to cmake.

10.6.2: February 16, 2022
  - Bug fixes:

    - Recognize strings encoded as UTF-16LE as Unicode. The PDF spec
      only allows UTF-16BE, but most readers accept UTF16-LE as well.

    - Fix a regression in command-line argument parsing to restore a
      previously undocumented behavior that some people were relying
      on.

    - Fix one more problem with mapping Unicode to PDF doc encoding

10.6.1: February 11, 2022
  - Fix compilation errors on some platforms

10.6.0: February 9, 2022
  - Preparation for replacement of ``PointerHolder``

    The next major release of qpdf will replace ``PointerHolder`` with
    ``std::shared_ptr`` across all of qpdf's public API. No action is
    required at this time, but if you'd like to prepare, read the
    comments in :file:`include/qpdf/PointerHolder.hh` and see
    :ref:`smart-pointers` for details on what you can do now to create
    code that will continue to work with older versions of qpdf and be
    easier to switch over to qpdf 11 when it comes out.

  - Preparation for a new JSON output version

    - The :qpdf:ref:`--json` option takes an optional parameter
      indicating the version of the JSON output. At present, there is
      only one JSON version (``1``), but there are plans for an
      updated version in a coming release. Until the release of qpdf
      11, the default value of ``--json`` is ``1`` for compatibility.
      Once qpdf 11 is out, the default version will be ``latest``. If
      you are depending on the exact format of ``--json`` for code,
      you should start using ``--json=1`` in preparation.

  - New QPDFJob API exposes CLI functionality

    Prior to qpdf 10.6, a lot of the functionality implemented by the
    qpdf CLI executable was built into the executable itself and not
    available from the library. qpdf 10.6 introduces a new object,
    ``QPDFJob``, that exposes all of the command-line functionality.
    This includes a native ``QPDFJob`` API with fluent interfaces that
    mirror the command-line syntax, a JSON syntax for specifying the
    equivalent of a command-line invocation, and the ability to run a
    qpdf "job" by passing a null-terminated array of qpdf command-line
    options. The command-line argument array and JSON methods of
    invoking ``QPDFJob`` are also exposed to the C API. For details,
    see :ref:`qpdf-job`.

  - Other Library Enhancements

    - New ``QPDFObjectHandle`` literal syntax using C++'s user-defined
      literal syntax. You can use

      .. code-block:: c++

         auto oh = "<</Some (valid) /PDF (object)>>"_qpdf;

      to create a QPDFObjectHandle. It is a shorthand for
      ``QPDFObjectHandle::parse``.

    - Preprocessor symbols ``QPDF_MAJOR_VERSION``,
      ``QPDF_MINOR_VERSION``, and ``QPDF_PATCH_VERSION`` are now
      available and can be used to make it easier to write code that
      supports multiple versions of qpdf. You don't have to include
      any new header files to get these, which makes it possible to
      write code like this:

      .. code-block:: c++

         #if !defined(QPDF_MAJOR_VERSION) || QPDF_MAJOR_VERSION < 11
             // do something using qpdf 10 or older API
         #else
             // do something using qpdf 11 or newer API
         #endif

      Since this was introduced only in qpdf version 10.6.0, testing
      for an undefined value of ``QPDF_MAJOR_VERSION`` is equivalent
      to detecting a version prior to 10.6.0.

      The symbol ``QPDF_VERSION`` is also defined as a string
      containing the same version number that is returned by
      ``QPDF::QPDFVersion``. Note that ``QPDF_VERSION`` may differ
      from ``QPDF::QPDFVersion()`` if your header files and library
      are out of sync with each other.

    - The method ``QPDF::QPDFVersion`` and corresponding C API call
      ``qpdf_get_qpdf_version`` are now both guaranteed to return a
      reference (or pointer) to a static string, so you don't have to
      copy these if you are using them in your software. They have
      always returned static values. Now the fact that they return
      static values is part of the API contract and can be safely
      relied upon.

    - New accessor methods for ``QPDFObjectHandle``. In addition to
      the traditional ones, such as ``getIntValue``, ``getName``,
      etc., there are a family of new accessors whose names are of the
      form ``getValueAsX``. The difference in behavior is as follows:

      - The older accessor methods, which will continue to be
        supported, return the value of the object if it is the
        expected type. Otherwise, they return a fallback value and
        issue a warning.

      - The newer accessor methods return a boolean indicating whether
        or not the object is of the expected type. If it is, a
        reference to a variable of the correct type is initialized.

      In many cases, the new interfaces will enable more compact code
      and will also never generate type warnings. Thanks to M. Holger
      for contributing these accessors. Search for ``getValueAs`` in
      :file:`include/qpdf/QPDFObjectHandle.hh` for a complete list.

      These are also exposed in the C API in functions whose names
      start with ``qpdf_oh_get_value_as``.

    - New convenience methods in ``QPDFObjectHandle``:
      ``isDictionaryOfType``, ``isStreamOfType``, and
      ``isNameAndEquals`` allow more compact querying of dictionaries.
      Also added to the C API: ``qpdf_oh_is_dictionary_of_type`` and
      ``qpdf_oh_is_name_and_equals``. Thanks to M. Holger for the
      contribution.

    - New convenience method in ``QPDFObjectHandle``: ``getKeyIfDict``
      returns null when called on null and otherwise calls ``getKey``.
      This makes it easier to access optional, lower-level
      dictionaries. It is exposed in the C API
      ``qpdf_oh_get_key_if_dict``. Thanks to M. Holger for the
      contribution.

    - New functions added to ``QUtil``: ``make_shared_cstr`` and
      ``make_unique_cstr`` copy ``std::string`` to
      ``std::shared_ptr<char>`` and ``std::unique_ptr<char[]>``. These
      are alternatives to the existing ``QUtil::copy_string`` function
      which offer other ways to get a C string with safer memory
      management.

    - New function ``QUtil::file_can_be_opened`` tests to see whether
      a file can actually be opened by attempting to open it and close
      it again.

    - There is a new version of ``QUtil::call_main_from_wmain`` that
      takes a ``const`` argv array and calls a main that takes a
      ``const`` argv array.

    - ``QPDF::emptyPDF`` has been exposed to the C API as
      ``qpdf_empty_pdf``. This makes it possible to create a PDF from
      scratch with the C API.

    - New C API functions ``qpdf_oh_get_binary_utf8_value`` and
      ``qpdf_oh_new_binary_unicode_string`` take length parameters,
      which makes it possible to handle UTF-8-encoded C strings with
      embedded NUL characters. Thanks to M. Holger for the
      contribution.

    - There is a new ``PDFVersion`` class for representing a PDF
      version number with the ability to compare and order PDF
      versions. Methods ``QPDF::getVersionAsPDFVersion`` and a new
      version of ``QPDFWriter::setMinimumPDFVersion`` use it. This
      makes it easier to create an output file whose PDF version is
      the maximum of the versions across all the input files that
      contributed to it.

    - The ``JSON`` object in the qpdf library has been enhanced to
      include a parser and the ability to get values out of the
      ``JSON`` object. Previously it was a write-only interface. Even
      so, qpdf's ``JSON`` object is not intended to be a
      general-purpose JSON implementation as discussed in
      :file:`include/qpdf/JSON.hh`.

    - The ``JSON`` object's "schema" checking functionality now allows
      for optional keys. Note that this "schema" functionality doesn't
      conform to any type of standard. It's just there to help with
      error reporting with qpdf's own JSON support.

  - Documentation Enhancements

    - Documentation for the command-line tool has been completely
      rewritten. This includes a top-to-bottom rewrite of :ref:`using`
      in the manual. Command-line arguments are now indexed, and
      internal links can appear to them within the documentation.

    - The output of ``qpdf --help`` is generated from the manual and
      is divided into help topics that parallel the sections of the
      manual. When you run ``qpdf --help``, instead of getting a Great
      Wall of Text, you are given basic usage information and a list
      of help topics. It is possible to request help for any
      individual topic or any specific command-line option, or you can
      get a dump of all available help text. The manual continues to
      contain a greater level of detail and more examples.

  - Bug Fixes

    - Some characters were not correctly translated from PDF doc
      encoding to Unicode.

    - When splitting or combining pages, ensure that all output files
      have a PDF version greater than or equal to the maximum version
      of all the input files.

10.5.0: December 21, 2021
  - Packaging changes

    - Pre-built documentation is no longer distributed with the source
      distribution. The AppImage and Windows binary distributions
      still contain embedded documentation, and a separate ``doc``
      distribution file is available from the qpdf release site.
      Documentation is now available at `https://qpdf.readthedocs.io
      <https://qpdf.readthedocs.io>`__ for every major/minor version
      starting with version 10.5. Please see :ref:`packaging-doc` for
      details on how packagers should handle documentation.

    - The documentation sources have been switched from docbook to
      reStructuredText processed with `Sphinx
      <https://www.sphinx-doc.org>`__. This will break previous
      documentation links. A redirect is in place on the main website.
      A top-to-bottom review of the documentation is planned for an
      upcoming release.

  - Library Enhancements

    - Since qpdf version 8, using object accessor methods on an
      instance of ``QPDFObjectHandle`` may create warnings if the
      object is not of the expected type. These warnings now have an
      error code of ``qpdf_e_object`` instead of
      ``qpdf_e_damaged_pdf``. Also, comments have been added to
      :file:`QPDFObjectHandle.hh` to explain in more detail what the
      behavior is. See :ref:`object-accessors` for a more in-depth
      discussion.

    - Add ``Pl_Buffer::getMallocBuffer()`` to initialize a buffer
      allocated with ``malloc()`` for better cross-language
      interoperability.

  - C API Enhancements

    - Many thanks to M. Holger whose contributions have heavily
      influenced these C API enhancements. His several suggestions,
      pull requests, questions, and critical reading of documentation
      and comments have resulted in significant usability improvements
      to the C API.

    - Overhaul error handling for the object handle functions C API.
      Some rare error conditions that would previously have caused a
      crash are now trapped and reported, and the functions that
      generate them return fallback values. See comments in the
      ``ERROR HANDLING`` section of :file:`include/qpdf/qpdf-c.h` for
      details. In particular, exceptions thrown by the underlying C++
      code when calling object accessors are caught and converted into
      errors. The errors can be checked by calling ``qpdf_has_error``.
      Use ``qpdf_silence_errors`` to prevent the error from being
      written to stderr.

    - Add ``qpdf_get_last_string_length`` to the C API to get the
      length of the last string that was returned. This is needed to
      handle strings that contain embedded null characters.

    - Add ``qpdf_oh_is_initialized`` and
      ``qpdf_oh_new_uninitialized`` to the C API to make it possible
      to work with uninitialized objects.

    - Add ``qpdf_oh_new_object`` to the C API. This allows you to
      clone an object handle.

    - Add ``qpdf_get_object_by_id``, ``qpdf_make_indirect_object``,
      and ``qpdf_replace_object``, exposing the corresponding methods
      in ``QPDF`` and ``QPDFObjectHandle``.

    - Add several functions for working with pages. See ``PAGE
      FUNCTIONS`` in ``include/qpdf/qpdf-c.h`` for details.

    - Add several functions for working with streams. See ``STREAM
      FUNCTIONS`` in ``include/qpdf/qpdf-c.h`` for details.

    - Add ``qpdf_oh_get_type_code`` and ``qpdf_oh_get_type_name``.

    - Add ``qpdf_oh_get_binary_string_value`` and
      ``qpdf_oh_new_binary_string`` for making it easier to deal with
      strings that contain embedded null characters.

10.4.0: November 16, 2021
  - Handling of Weak Cryptography Algorithms

    - From the qpdf CLI, the
      :qpdf:ref:`--allow-weak-crypto` is now required to
      suppress a warning when explicitly creating PDF files using RC4
      encryption. While qpdf will always retain the ability to read
      and write such files, doing so will require explicit
      acknowledgment moving forward. For qpdf 10.4, this change only
      affects the command-line tool. Starting in qpdf 11, there will
      be small API changes to require explicit acknowledgment in
      those cases as well. For additional information, see :ref:`weak-crypto`.

  - Bug Fixes

    - Fix potential bounds error when handling shell completion that
      could occur when given bogus input.

    - Properly handle overlay/underlay on completely empty pages
      (with no resource dictionary).

    - Fix crash that could occur under certain conditions when using
      :qpdf:ref:`--pages` with files that had form
      fields.

  - Library Enhancements

    - Make ``QPDF::findPage`` functions public.

    - Add methods to ``Pl_Flate`` to be able to receive warnings on
      certain recoverable conditions.

    - Add an extra check to the library to detect when foreign
      objects are inserted directly (instead of using
      ``QPDF::copyForeignObject``) at the time of insertion rather
      than when the file is written. Catching the error sooner makes
      it much easier to locate the incorrect code.

  - CLI Enhancements

    - Improve diagnostics around parsing
      :qpdf:ref:`--pages` command-line options

  - Packaging Changes

    - The Windows binary distribution is now built with crypto
      provided by OpenSSL 3.0.

10.3.2: May 8, 2021
  - Bug Fixes

    - When generating a file while preserving object streams,
      unreferenced objects are correctly removed unless
      :qpdf:ref:`--preserve-unreferenced` is specified.

  - Library Enhancements

    - When adding a page that already exists, make a shallow copy
      instead of throwing an exception. This makes the library
      behavior consistent with the CLI behavior. See
      :file:`ChangeLog` for additional notes.

10.3.1: March 11, 2021
  - Bug Fixes

    - Form field copying failed on files where /DR was a direct
      object in the document-level form dictionary.

10.3.0: March 4, 2021
  - Bug Fixes

    - The code for handling form fields when copying pages from
      10.2.0 was not quite right and didn't work in a number of
      situations, such as when the same page was copied multiple
      times or when there were conflicting resource or field names
      across multiple copies. The 10.3.0 code has been much more
      thoroughly tested with more complex cases and with a multitude
      of readers and should be much closer to correct. The 10.2.0
      code worked well enough for page splitting or for copying pages
      with form fields into documents that didn't already have them
      but was still not quite correct in handling of field-level
      resources.

    - When ``QPDF::replaceObject`` or ``QPDF::swapObjects`` is
      called, existing ``QPDFObjectHandle`` instances no longer point
      to the old objects. The next time they are accessed, they
      automatically notice the change to the underlying object and
      update themselves. This resolves a very longstanding source of
      confusion, albeit in a very rarely used method call.

    - Fix form field handling code to look for default appearances,
      quadding, and default resources in the right places. The code
      was not looking for things in the document-level interactive
      form dictionary that it was supposed to be finding there. This
      required adding a few new methods to
      ``QPDFFormFieldObjectHelper``.

  - Library Enhancements

    - Reworked the code that handles copying annotations and form
      fields during page operations. There were additional methods
      added to the public API from 10.2.0 and a one deprecation of a
      method added in 10.2.0. The majority of the API changes are in
      methods most people would never call and that will hopefully be
      superseded by higher-level interfaces for handling page copies.
      Please see the :file:`ChangeLog` file for
      details.

    - The method ``QPDF::numWarnings`` was added so that you can tell
      whether any warnings happened during a specific block of code.

10.2.0: February 23, 2021
  - CLI Behavior Changes

    - Operations that work on combining pages are much better about
      protecting form fields. In particular,
      :qpdf:ref:`--split-pages` and
      :qpdf:ref:`--pages` now preserve interaction form
      functionality by copying the relevant form field information
      from the original files. Additionally, if you use
      :qpdf:ref:`--pages` to select only some pages from
      the original input file, unused form fields are removed, which
      prevents lots of unused annotations from being retained.

    - By default, :command:`qpdf` no longer allows
      creation of encrypted PDF files whose user password is
      non-empty and owner password is empty when a 256-bit key is in
      use. The :qpdf:ref:`--allow-insecure` option,
      specified inside the :qpdf:ref:`--encrypt` options,
      allows creation of such files. Behavior changes in the CLI are
      avoided when possible, but an exception was made here because
      this is security-related. qpdf must always allow creation of
      weird files for testing purposes, but it should not default to
      letting users unknowingly create insecure files.

  - Library Behavior Changes

    - Note: the changes in this section cause differences in output
      in some cases. These differences change the syntax of the PDF
      but do not change the semantics (meaning). I make a strong
      effort to avoid gratuitous changes in qpdf's output so that
      qpdf changes don't break people's tests. In this case, the
      changes significantly improve the readability of the generated
      PDF and don't affect any output that's generated by simple
      transformation. If you are annoyed by having to update test
      files, please rest assured that changes like this have been and
      will continue to be rare events.

    - ``QPDFObjectHandle::newUnicodeString`` now uses whichever of
      ASCII, PDFDocEncoding, of UTF-16 is sufficient to encode all
      the characters in the string. This reduces needless encoding in
      UTF-16 of strings that can be encoded in ASCII. This change may
      cause qpdf to generate different output than before when form
      field values are set using ``QPDFFormFieldObjectHelper`` but
      does not change the meaning of the output.

    - The code that places form XObjects and also the code that
      flattens rotations trim trailing zeroes from real numbers that
      they calculate. This causes slight (but semantically
      equivalent) differences in generated appearance streams and
      form XObject invocations in overlay/underlay code or in user
      code that calls the methods that place form XObjects on a page.

  - CLI Enhancements

    - Add new command line options for listing, saving, adding,
      removing, and and copying file attachments. See :ref:`attachments` for details.

    - Page splitting and merging operations, as well as
      :qpdf:ref:`--flatten-rotation`, are better behaved
      with respect to annotations and interactive form fields. In
      most cases, interactive form field functionality and proper
      formatting and functionality of annotations is preserved by
      these operations. There are still some cases that aren't
      perfect, such as when functionality of annotations depends on
      document-level data that qpdf doesn't yet understand or when
      there are problems with referential integrity among form fields
      and annotations (e.g., when a single form field object or its
      associated annotations are shared across multiple pages, a case
      that is out of spec but that works in most viewers anyway).

    - The option
      :samp:`--password-file={filename}`
      can now be used to read the decryption password from a file.
      You can use ``-`` as the file name to read the password from
      standard input. This is an easier/more obvious way to read
      passwords from files or standard input than using
      :samp:`@file` for this purpose.

    - Add some information about attachments to the JSON output, and
      added ``attachments`` as an additional JSON key. The
      information included here is limited to the preferred name and
      content stream and a reference to the file spec object. This is
      enough detail for clients to avoid the hassle of navigating a
      name tree and provides what is needed for basic enumeration and
      extraction of attachments. More detailed information can be
      obtained by following the reference to the file spec object.

    - Add numeric option to :qpdf:ref:`--collate`. If
      :samp:`--collate={n}`
      is given, take pages in groups of
      :samp:`{n}` from the given files.

    - It is now valid to provide :samp:`--rotate=0`
      to clear rotation from a page.

  - Library Enhancements

    - This release includes numerous additions to the API. Not all
      changes are listed here. Please see the
      :file:`ChangeLog` file in the source
      distribution for a comprehensive list. Highlights appear below.

    - Add ``QPDFObjectHandle::ditems()`` and
      ``QPDFObjectHandle::aitems()`` that enable C++-style iteration,
      including range-for iteration, over dictionary and array
      QPDFObjectHandles. See comments in
      :file:`include/qpdf/QPDFObjectHandle.hh`
      and
      :file:`examples/pdf-name-number-tree.cc`
      for details.

    - Add ``QPDFObjectHandle::copyStream`` for making a copy of a
      stream within the same ``QPDF`` instance.

    - Add new helper classes for supporting file attachments, also
      known as embedded files. New classes are
      ``QPDFEmbeddedFileDocumentHelper``,
      ``QPDFFileSpecObjectHelper``, and ``QPDFEFStreamObjectHelper``.
      See their respective headers for details and
      :file:`examples/pdf-attach-file.cc` for an
      example.

    - Add a version of ``QPDFObjectHandle::parse`` that takes a
      ``QPDF`` pointer as context so that it can parse strings
      containing indirect object references. This is illustrated in
      :file:`examples/pdf-attach-file.cc`.

    - Re-implement ``QPDFNameTreeObjectHelper`` and
      ``QPDFNumberTreeObjectHelper`` to be more efficient, add an
      iterator-based API, give them the capability to repair broken
      trees, and create methods for modifying the trees. With this
      change, qpdf has a robust read/write implementation of name and
      number trees.

    - Add new versions of ``QPDFObjectHandle::replaceStreamData``
      that take ``std::function`` objects for cases when you need
      something between a static string and a full-fledged
      StreamDataProvider. Using this with ``QUtil::file_provider`` is
      a very easy way to create a stream from the contents of a file.

    - The ``QPDFMatrix`` class, formerly a private, internal class,
      has been added to the public API. See
      :file:`include/qpdf/QPDFMatrix.hh` for
      details. This class is for working with transformation
      matrices. Some methods in ``QPDFPageObjectHelper`` make use of
      this to make information about transformation matrices
      available. For an example, see
      :file:`examples/pdf-overlay-page.cc`.

    - Several new methods were added to
      ``QPDFAcroFormDocumentHelper`` for adding, removing, getting
      information about, and enumerating form fields.

    - Add method
      ``QPDFAcroFormDocumentHelper::transformAnnotations``, which
      applies a transformation to each annotation on a page.

    - Add ``QPDFPageObjectHelper::copyAnnotations``, which copies
      annotations and, if applicable, associated form fields, from
      one page to another, possibly transforming the rectangles.

  - Build Changes

    - A C++-14 compiler is now required to build qpdf. There is no
      intention to require anything newer than that for a while.
      C++-14 includes modest enhancements to C++-11 and appears to be
      supported about as widely as C++-11.

  - Bug Fixes

    - The :qpdf:ref:`--flatten-rotation` option applies
      transformations to any annotations that may be on the page.

    - If a form XObject lacks a resources dictionary, consider any
      names in that form XObject to be referenced from the containing
      page. This is compliant with older PDF versions. Also detect if
      any form XObjects have any unresolved names and, if so, don't
      remove unreferenced resources from them or from the page that
      contains them. Unfortunately this has the side effect of
      preventing removal of unreferenced resources in some cases
      where names appear that don't refer to resources, such as with
      tagged PDF. This is a bit of a corner case that is not likely
      to cause a significant problem in practice, but the only side
      effect would be lack of removal of shared resources. A future
      version of qpdf may be more sophisticated in its detection of
      names that refer to resources.

    - Properly handle strings if they appear in inline image
      dictionaries while externalizing inline images.

10.1.0: January 5, 2021
  - CLI Enhancements

    - Add :qpdf:ref:`--flatten-rotation` command-line
      option, which causes all pages that are rotated using
      parameters in the page's dictionary to instead be identically
      rotated in the page's contents. The change is not user-visible
      for compliant PDF readers but can be used to work around broken
      PDF applications that don't properly handle page rotation.

  - Library Enhancements

    - Support for user-provided (pluggable, modular) stream filters.
      It is now possible to derive a class from ``QPDFStreamFilter``
      and register it with ``QPDF`` so that regular library methods,
      including those used by ``QPDFWriter``, can decode streams with
      filters not directly supported by the library. The example
      :file:`examples/pdf-custom-filter.cc`
      illustrates how to use this capability.

    - Add methods to ``QPDFPageObjectHelper`` to iterate through
      XObjects on a page or form XObjects, possibly recursing into
      nested form XObjects: ``forEachXObject``, ``ForEachImage``,
      ``forEachFormXObject``.

    - Enhance several methods in ``QPDFPageObjectHelper`` to work
      with form XObjects as well as pages, as noted in comments. See
      :file:`ChangeLog` for a full list.

    - Rename some functions in ``QPDFPageObjectHelper``, while
      keeping old names for compatibility:

      - ``getPageImages`` to ``getImages``

      - ``filterPageContents`` to ``filterContents``

      - ``pipePageContents`` to ``pipeContents``

      - ``parsePageContents`` to ``parseContents``

    - Add method ``QPDFPageObjectHelper::getFormXObjects`` to return
      a map of form XObjects directly on a page or form XObject

    - Add new helper methods to ``QPDFObjectHandle``:
      ``isFormXObject``, ``isImage``

    - Add the optional ``allow_streams`` parameter
      ``QPDFObjectHandle::makeDirect``. When
      ``QPDFObjectHandle::makeDirect`` is called in this way, it
      preserves references to streams rather than throwing an
      exception.

    - Add ``QPDFObjectHandle::setFilterOnWrite`` method. Calling this
      on a stream prevents ``QPDFWriter`` from attempting to
      uncompress, recompress, or otherwise filter a stream even if it
      could. Developers can use this to protect streams that are
      optimized should be protected from ``QPDFWriter``'s default
      behavior for any other reason.

    - Add ``ostream`` ``<<`` operator for ``QPDFObjGen``. This is
      useful to have for debugging.

    - Add method ``QPDFPageObjectHelper::flattenRotation``, which
      replaces a page's ``/Rotate`` keyword by rotating the page
      within the content stream and altering the page's bounding
      boxes so the rendering is the same. This can be used to work
      around buggy PDF readers that can't properly handle page
      rotation.

  - C API Enhancements

    - Add several new functions to the C API for working with
      objects. These are wrappers around many of the methods in
      ``QPDFObjectHandle``. Their inclusion adds considerable new
      capability to the C API.

    - Add ``qpdf_register_progress_reporter`` to the C API,
      corresponding to ``QPDFWriter::registerProgressReporter``.

  - Performance Enhancements

    - Improve steps ``QPDFWriter`` takes to prepare a ``QPDF`` object
      for writing, resulting in about an 8% improvement in write
      performance while allowing indirect objects to appear in
      ``/DecodeParms``.

    - When extracting pages, the :command:`qpdf` CLI
      only removes unreferenced resources from the pages that are
      being kept, resulting in a significant performance improvement
      when extracting small numbers of pages from large, complex
      documents.

  - Bug Fixes

    - ``QPDFPageObjectHelper::externalizeInlineImages`` was not
      externalizing images referenced from form XObjects that
      appeared on the page.

    - ``QPDFObjectHandle::filterPageContents`` was broken for pages
      with multiple content streams.

    - Tweak zsh completion code to behave a little better with
      respect to path completion.

10.0.4: November 21, 2020
  - Bug Fixes

    - Fix a handful of integer overflows. This includes cases found
      by fuzzing as well as having qpdf not do range checking on
      unused values in the xref stream.

10.0.3: October 31, 2020
  - Bug Fixes

    - The fix to the bug involving copying streams with indirect
      filters was incorrect and introduced a new, more serious bug.
      The original bug has been fixed correctly, as has the bug
      introduced in 10.0.2.

10.0.2: October 27, 2020
  - Bug Fixes

    - When concatenating content streams, as with
      :qpdf:ref:`--coalesce-contents`, there were cases
      in which qpdf would merge two lexical tokens together, creating
      invalid results. A newline is now inserted between merged
      content streams if one is not already present.

    - Fix an internal error that could occur when copying foreign
      streams whose stream data had been replaced using a stream data
      provider if those streams had indirect filters or decode
      parameters. This is a rare corner case.

    - Ensure that the caller's locale settings do not change the
      results of numeric conversions performed internally by the qpdf
      library. Note that the problem here could only be caused when
      the qpdf library was used programmatically. Using the qpdf CLI
      already ignored the user's locale for numeric conversion.

    - Fix several instances in which warnings were not suppressed in
      spite of :qpdf:ref:`--no-warn` and/or errors or
      warnings were written to standard output rather than standard
      error.

    - Fixed a memory leak that could occur under specific
      circumstances when
      :samp:`--object-streams=generate` was used.

    - Fix various integer overflows and similar conditions found by
      the OSS-Fuzz project.

  - Enhancements

    - New option :qpdf:ref:`--warning-exit-0` causes qpdf
      to exit with a status of ``0`` rather than ``3`` if there are
      warnings but no errors. Combine with
      :qpdf:ref:`--no-warn` to completely ignore
      warnings.

    - Performance improvements have been made to
      ``QPDF::processMemoryFile``.

    - The OpenSSL crypto provider produces more detailed error
      messages.

  - Build Changes

    - The option :samp:`--disable-rpath` is now
      supported by qpdf's :command:`./configure`
      script. Some distributions' packaging standards recommended the
      use of this option.

    - Selection of a printf format string for ``long long`` has
      been moved from ``ifdefs`` to an autoconf
      test. If you are using your own build system, you will need to
      provide a value for ``LL_FMT`` in
      :file:`libqpdf/qpdf/qpdf-config.h`, which
      would typically be ``"%lld"`` or, for some Windows compilers,
      ``"%I64d"``.

    - Several improvements were made to build-time configuration of
      the OpenSSL crypto provider.

    - A nearly stand-alone Linux binary zip file is now included with
      the qpdf release. This is built on an older (but supported)
      Ubuntu LTS release, but would work on most reasonably recent
      Linux distributions. It contains only the executables and
      required shared libraries that would not be present on a
      minimal system. It can be used for including qpdf in a minimal
      environment, such as a docker container. The zip file is also
      known to work as a layer in AWS Lambda.

    - QPDF's automated build has been migrated from Azure Pipelines
      to GitHub Actions.

  - Windows-specific Changes

    - The Windows executables distributed with qpdf releases now use
      the OpenSSL crypto provider by default. The native crypto
      provider is also compiled in and can be selected at runtime
      with the ``QPDF_CRYPTO_PROVIDER`` environment variable.

    - Improvements have been made to how a cryptographic provider is
      obtained in the native Windows crypto implementation. However
      mostly this is shadowed by OpenSSL being used by default.

10.0.1: April 9, 2020
  - Bug Fixes

    - 10.0.0 introduced a bug in which calling
      ``QPDFObjectHandle::getStreamData`` on a stream that can't be
      filtered was returning the raw data instead of throwing an
      exception. This is now fixed.

    - Fix a bug that was preventing qpdf from linking with some
      versions of clang on some platforms.

  - Enhancements

    - Improve the :file:`pdf-invert-images`
      example to avoid having to load all the images into RAM at the
      same time.

10.0.0: April 6, 2020
  - Performance Enhancements

    - The qpdf library and executable should run much faster in this
      version than in the last several releases. Several internal
      library optimizations have been made, and there has been
      improved behavior on page splitting as well. This version of
      qpdf should outperform any of the 8.x or 9.x versions.

  - Incompatible API (source-level) Changes (minor)

    - The ``QUtil::srandom`` method was removed. It didn't do
      anything unless insecure random numbers were compiled in, and
      they have been off by default for a long time. If you were
      calling it, just remove the call since it wasn't doing anything
      anyway.

  - Build/Packaging Changes

    - Add a ``openssl`` crypto provider, which is implemented with
      OpenSSL and also works with BoringSSL. Thanks to Dean Scarff
      for this contribution. If you maintain qpdf for a distribution,
      pay special attention to make sure that you are including
      support for the crypto providers you want. Package maintainers
      will have to weigh the advantages of allowing users to pick a
      crypto provider at runtime against the disadvantages of adding
      more dependencies to qpdf.

    - Allow qpdf to built on stripped down systems whose C/C++
      libraries lack the ``wchar_t`` type. Search for ``wchar_t`` in
      qpdf's README.md for details. This should be very rare, but it
      is known to be helpful in some embedded environments.

  - CLI Enhancements

    - Add ``objectinfo`` key to the JSON output. This will be a place
      to put computed metadata or other information about PDF objects
      that are not immediately evident in other ways or that seem
      useful for some other reason. In this version, information is
      provided about each object indicating whether it is a stream
      and, if so, what its length and filters are. Without this, it
      was not possible to tell conclusively from the JSON output
      alone whether or not an object was a stream. Run
      :command:`qpdf --json-help` for details.

    - Add new option
      :qpdf:ref:`--remove-unreferenced-resources` which
      takes ``auto``, ``yes``, or ``no`` as arguments. The new
      ``auto`` mode, which is the default, performs a fast heuristic
      over a PDF file when splitting pages to determine whether the
      expensive process of finding and removing unreferenced
      resources is likely to be of benefit. For most files, this new
      default will result in a significant performance improvement
      for splitting pages.

    - The :qpdf:ref:`--preserve-unreferenced-resources`
      is now just a synonym for
      :samp:`--remove-unreferenced-resources=no`.

    - If the ``QPDF_EXECUTABLE`` environment variable is set when
      invoking :command:`qpdf --bash-completion` or
      :command:`qpdf --zsh-completion`, the completion
      command that it outputs will refer to qpdf using the value of
      that variable rather than what :command:`qpdf`
      determines its executable path to be. This can be useful when
      wrapping :command:`qpdf` with a script, working
      with a version in the source tree, using an AppImage, or other
      situations where there is some indirection.

  - Library Enhancements

    - Random number generation is now delegated to the crypto
      provider. The old behavior is still used by the native crypto
      provider. It is still possible to provide your own random
      number generator.

    - Add a new version of
      ``QPDFObjectHandle::StreamDataProvider::provideStreamData``
      that accepts the ``suppress_warnings`` and ``will_retry``
      options and allows a success code to be returned. This makes it
      possible to implement a ``StreamDataProvider`` that calls
      ``pipeStreamData`` on another stream and to pass the response
      back to the caller, which enables better error handling on
      those proxied streams.

    - Update ``QPDFObjectHandle::pipeStreamData`` to return an
      overall success code that goes beyond whether or not filtered
      data was written successfully. This allows better error
      handling of cases that were not filtering errors. You have to
      call this explicitly. Methods in previously existing APIs have
      the same semantics as before.

    - The ``QPDFPageObjectHelper::placeFormXObject`` method now
      allows separate control over whether it should be willing to
      shrink or expand objects to fit them better into the
      destination rectangle. The previous behavior was that shrinking
      was allowed but expansion was not. The previous behavior is
      still the default.

    - When calling the C API, any non-zero value passed to a boolean
      parameter is treated as ``TRUE``. Previously only the value
      ``1`` was accepted. This makes the C API behave more like most
      C interfaces and is known to improve compatibility with some
      Windows environments that dynamically load the DLL and call
      functions from it.

    - Add ``QPDFObjectHandle::unsafeShallowCopy`` for copying only
      top-level dictionary keys or array items. This is unsafe
      because it creates a situation in which changing a lower-level
      item in one object may also change it in another object, but
      for cases in which you *know* you are only inserting or
      replacing top-level items, it is much faster than
      ``QPDFObjectHandle::shallowCopy``.

    - Add ``QPDFObjectHandle::filterAsContents``, which filter's a
      stream's data as a content stream. This is useful for parsing
      the contents for form XObjects in the same way as parsing page
      content streams.

  - Bug Fixes

    - When detecting and removing unreferenced resources during page
      splitting, traverse into form XObjects and handle their
      resources dictionaries as well.

    - The same error recovery is applied to streams in other than the
      primary input file when merging or splitting pages.

9.1.1: January 26, 2020
  - Build/Packaging Changes

    - The fix-qdf program was converted from perl to C++. As such,
      qpdf no longer has a runtime dependency on perl.

  - Library Enhancements

    - Added new helper routine ``QUtil::call_main_from_wmain`` which
      converts ``wchar_t`` arguments to UTF-8 encoded strings. This
      is useful for qpdf because library methods expect file names to
      be UTF-8 encoded, even on Windows

    - Added new ``QUtil::read_lines_from_file`` methods that take
      ``FILE*`` arguments and that allow preservation of end-of-line
      characters. This also fixes a bug where
      ``QUtil::read_lines_from_file`` wouldn't work properly with
      Unicode filenames.

  - CLI Enhancements

    - Added options :qpdf:ref:`--is-encrypted` and
      :qpdf:ref:`--requires-password` for testing whether
      a file is encrypted or requires a password other than the
      supplied (or empty) password. These communicate via exit
      status, making them useful for shell scripts. They also work on
      encrypted files with unknown passwords.

    - Added ``encrypt`` key to JSON options. With the exception of
      the reconstructed user password for older encryption formats,
      this provides the same information as
      :qpdf:ref:`--show-encryption` but in a consistent,
      parseable format. See output of :command:`qpdf
      --json-help` for details.

  - Bug Fixes

    - In QDF mode, be sure not to write more than one XRef stream to
      a file, even when
      :qpdf:ref:`--preserve-unreferenced` is used.
      :command:`fix-qdf` assumes that there is only
      one XRef stream, and that it appears at the end of the file.

    - When externalizing inline images, properly handle images whose
      color space is a reference to an object in the page's resource
      dictionary.

    - Windows-specific fix for acquiring crypt context with a new
      keyset.

9.1.0: November 17, 2019
  - Build Changes

    - A C++-11 compiler is now required to build qpdf.

    - A new crypto provider that uses gnutls for crypto functions is
      now available and can be enabled at build time. See :ref:`crypto` for more information about crypto
      providers and :ref:`crypto.build` for specific information about
      the build.

  - Library Enhancements

    - Incorporate contribution from Masamichi Hosoda to properly
      handle signature dictionaries by not including them in object
      streams, formatting the ``Contents`` key has a hexadecimal
      string, and excluding the ``/Contents`` key from encryption and
      decryption.

    - Incorporate contribution from Masamichi Hosoda to provide new
      API calls for getting file-level information about input and
      output files, enabling certain operations on the files at the
      file level rather than the object level. New methods include
      ``QPDF::getXRefTable()``,
      ``QPDFObjectHandle::getParsedOffset()``,
      ``QPDFWriter::getRenumberedObjGen(QPDFObjGen)``, and
      ``QPDFWriter::getWrittenXRefTable()``.

    - Support build-time and runtime selectable crypto providers.
      This includes the addition of new classes
      ``QPDFCryptoProvider`` and ``QPDFCryptoImpl`` and the
      recognition of the ``QPDF_CRYPTO_PROVIDER`` environment
      variable. Crypto providers are described in depth in :ref:`crypto`.

  - CLI Enhancements

    - Addition of the :qpdf:ref:`--show-crypto` option in
      support of selectable crypto providers, as described in :ref:`crypto`.

    - Allow ``:even`` or ``:odd`` to be appended to numeric ranges
      for specification of the even or odd pages from among the pages
      specified in the range.

    - Fix shell wildcard expansion behavior (``*`` and ``?``) of the
      :command:`qpdf.exe` as built my MSVC.

9.0.2: October 12, 2019
  - Bug Fix

    - Fix the name of the temporary file used by
      :qpdf:ref:`--replace-input` so that it doesn't
      require path splitting and works with paths include
      directories.

9.0.1: September 20, 2019
  - Bug Fixes/Enhancements

    - Fix some build and test issues on big-endian systems and
      compilers with characters that are unsigned by default. The
      problems were in build and test only. There were no actual bugs
      in the qpdf library itself relating to endianness or unsigned
      characters.

    - When a dictionary has a duplicated key, report this with a
      warning. The behavior of the library in this case is unchanged,
      but the error condition is no longer silently ignored.

    - When a form field's display rectangle is erroneously specified
      with inverted coordinates, detect and correct this situation.
      This avoids some form fields from being flipped when flattening
      annotations on files with this condition.

9.0.0: August 31, 2019
  - Incompatible API (source-level) Changes (minor)

    - The method ``QUtil::strcasecmp`` has been renamed to
      ``QUtil::str_compare_nocase``. This incompatible change is
      necessary to enable qpdf to build on platforms that define
      ``strcasecmp`` as a macro.

    - The ``QPDF::copyForeignObject`` method had an overloaded
      version that took a boolean parameter that was not used. If you
      were using this version, just omit the extra parameter.

    - There was a version ``QPDFTokenizer::expectInlineImage`` that
      took no arguments. This version has been removed since it
      caused the tokenizer to return incorrect inline images. A new
      version was added some time ago that produces correct output.
      This is a very low level method that doesn't make sense to call
      outside of qpdf's lexical engine. There are higher level
      methods for tokenizing content streams.

    - Change ``QPDFOutlineDocumentHelper::getTopLevelOutlines`` and
      ``QPDFOutlineObjectHelper::getKids`` to return a
      ``std::vector`` instead of a ``std::list`` of
      ``QPDFOutlineObjectHelper`` objects.

    - Remove method ``QPDFTokenizer::allowPoundAnywhereInName``. This
      function would allow creation of name tokens whose value would
      change when unparsed, which is never the correct behavior.

  - CLI Enhancements

    - The :qpdf:ref:`--replace-input` option may be given
      in place of an output file name. This causes qpdf to overwrite
      the input file with the output. See the description of
      :qpdf:ref:`--replace-input` for more details.

    - The :qpdf:ref:`--recompress-flate` instructs
      :command:`qpdf` to recompress streams that are
      already compressed with ``/FlateDecode``. Useful with
      :qpdf:ref:`--compression-level`.

    - The
      :samp:`--compression-level={level}`
      sets the zlib compression level used for any streams compressed
      by ``/FlateDecode``. Most effective when combined with
      :qpdf:ref:`--recompress-flate`.

  - Library Enhancements

    - A new namespace ``QIntC``, provided by
      :file:`qpdf/QIntC.hh`, provides safe
      conversion methods between different integer types. These
      conversion methods do range checking to ensure that the cast
      can be performed with no loss of information. Every use of
      ``static_cast`` in the library was inspected to see if it could
      use one of these safe converters instead. See :ref:`casting` for additional details.

    - Method ``QPDF::anyWarnings`` tells whether there have been any
      warnings without clearing the list of warnings.

    - Method ``QPDF::closeInputSource`` closes or otherwise releases
      the input source. This enables the input file to be deleted or
      renamed.

    - New methods have been added to ``QUtil`` for converting back
      and forth between strings and unsigned integers:
      ``uint_to_string``, ``uint_to_string_base``,
      ``string_to_uint``, and ``string_to_ull``.

    - New methods have been added to ``QPDFObjectHandle`` that return
      the value of ``Integer`` objects as ``int`` or ``unsigned int``
      with range checking and sensible fallback values, and a new
      method was added to return an unsigned value. This makes it
      easier to write code that is safe from unintentional data loss.
      Functions: ``getUIntValue``, ``getIntValueAsInt``,
      ``getUIntValueAsUInt``.

    - When parsing content streams with
      ``QPDFObjectHandle::ParserCallbacks``, in place of the method
      ``handleObject(QPDFObjectHandle)``, the developer may override
      ``handleObject(QPDFObjectHandle, size_t offset, size_t
      length)``. If this method is defined, it will
      be invoked with the object along with its offset and length
      within the overall contents being parsed. Intervening spaces
      and comments are not included in offset and length.
      Additionally, a new method ``contentSize(size_t)`` may be
      implemented. If present, it will be called prior to the first
      call to ``handleObject`` with the total size in bytes of the
      combined contents.

    - New methods ``QPDF::userPasswordMatched`` and
      ``QPDF::ownerPasswordMatched`` have been added to enable a
      caller to determine whether the supplied password was the user
      password, the owner password, or both. This information is also
      displayed by :command:`qpdf --show-encryption`
      and :command:`qpdf --check`.

    - Static method ``Pl_Flate::setCompressionLevel`` can be called
      to set the zlib compression level globally used by all
      instances of Pl_Flate in deflate mode.

    - The method ``QPDFWriter::setRecompressFlate`` can be called to
      tell ``QPDFWriter`` to uncompress and recompress streams
      already compressed with ``/FlateDecode``.

    - The underlying implementation of QPDF arrays has been enhanced
      to be much more memory efficient when dealing with arrays with
      lots of nulls. This enables qpdf to use drastically less memory
      for certain types of files.

    - When traversing the pages tree, if nodes are encountered with
      invalid types, the types are fixed, and a warning is issued.

    - A new helper method ``QUtil::read_file_into_memory`` was added.

    - All conditions previously reported by
      ``QPDF::checkLinearization()`` as errors are now presented as
      warnings.

    - Name tokens containing the ``#`` character not preceded by two
      hexadecimal digits, which is invalid in PDF 1.2 and above, are
      properly handled by the library: a warning is generated, and
      the name token is properly preserved, even if invalid, in the
      output. See :file:`ChangeLog` for a more
      complete description of this change.

  - Bug Fixes

    - A small handful of memory issues, assertion failures, and
      unhandled exceptions that could occur on badly mangled input
      files have been fixed. Most of these problems were found by
      Google's OSS-Fuzz project.

    - When :command:`qpdf --check` or
      :command:`qpdf --check-linearization` encounters
      a file with linearization warnings but not errors, it now
      properly exits with exit code 3 instead of 2.

    - The :qpdf:ref:`--completion-bash` and
      :qpdf:ref:`--completion-zsh` options now work
      properly when qpdf is invoked as an AppImage.

    - Calling ``QPDFWriter::set*EncryptionParameters`` on a
      ``QPDFWriter`` object whose output filename has not yet been
      set no longer produces a segmentation fault.

    - When reading encrypted files, follow the spec more closely
      regarding encryption key length. This allows qpdf to open
      encrypted files in most cases when they have invalid or missing
      /Length keys in the encryption dictionary.

  - Build Changes

    - On platforms that support it, qpdf now builds with
      :samp:`-fvisibility=hidden`. If you build qpdf
      with your own build system, this is now safe to use. This
      prevents methods that are not part of the public API from being
      exported by the shared library, and makes qpdf's ELF shared
      libraries (used on Linux, MacOS, and most other UNIX flavors)
      behave more like the Windows DLL. Since the DLL already behaves
      in much this way, it is unlikely that there are any methods
      that were accidentally not exported. However, with ELF shared
      libraries, typeinfo for some classes has to be explicitly
      exported. If there are problems in dynamically linked code
      catching exceptions or subclassing, this could be the reason.
      If you see this, please report a bug at
      https://github.com/qpdf/qpdf/issues/.

    - QPDF is now compiled with integer conversion and sign
      conversion warnings enabled. Numerous changes were made to the
      library to make this safe.

    - QPDF's :command:`make install` target explicitly
      specifies the mode to use when installing files instead of
      relying the user's umask. It was previously doing this for some
      files but not others.

    - If :command:`pkg-config` is available, use it to
      locate :file:`libjpeg` and
      :file:`zlib` dependencies, falling back on
      old behavior if unsuccessful.

  - Other Notes

    - QPDF has been fully integrated into `Google's OSS-Fuzz
      project <https://github.com/google/oss-fuzz>`__. This project
      exercises code with randomly mutated inputs and is great for
      discovering hidden security crashes and security issues.
      Several bugs found by oss-fuzz have already been fixed in qpdf.

8.4.2: May 18, 2019
   This release has just one change: correction of a buffer overrun in
   the Windows code used to open files. Windows users should take this
   update. There are no code changes that affect non-Windows releases.

8.4.1: April 27, 2019
  - Enhancements

    - When :command:`qpdf --version` is run, it will
      detect if the qpdf CLI was built with a different version of
      qpdf than the library, which may indicate a problem with the
      installation.

    - New option :qpdf:ref:`--remove-page-labels` will
      remove page labels before generating output. This used to
      happen if you ran :command:`qpdf --empty --pages ..
      --`, but the behavior changed in qpdf 8.3.0. This
      option enables people who were relying on the old behavior to
      get it again.

    - New option
      :samp:`--keep-files-open-threshold={count}`
      can be used to override number of files that qpdf will use to
      trigger the behavior of not keeping all files open when merging
      files. This may be necessary if your system allows fewer than
      the default value of 200 files to be open at the same time.

  - Bug Fixes

    - Handle Unicode characters in filenames on Windows. The changes
      to support Unicode on the CLI in Windows broke Unicode
      filenames for Windows.

    - Slightly tighten logic that determines whether an object is a
      page. This should resolve problems in some rare files where
      some non-page objects were passing qpdf's test for whether
      something was a page, thus causing them to be erroneously lost
      during page splitting operations.

    - Revert change that included preservation of outlines
      (bookmarks) in :qpdf:ref:`--split-pages`. The way
      it was implemented in 8.3.0 and 8.4.0 caused a very significant
      degradation of performance for splitting certain files. A
      future release of qpdf may re-introduce the behavior in a more
      performant and also more correct fashion.

    - In JSON mode, add missing leading 0 to decimal values between
      -1 and 1 even if not present in the input. The JSON
      specification requires the leading 0. The PDF specification
      does not.

8.4.0: February 1, 2019
  - Command-line Enhancements

    - *Non-compatible CLI change:* The qpdf command-line tool
      interprets passwords given at the command-line differently from
      previous releases when the passwords contain non-ASCII
      characters. In some cases, the behavior differs from previous
      releases. For a discussion of the current behavior, please see
      :ref:`unicode-passwords`. The
      incompatibilities are as follows:

      - On Windows, qpdf now receives all command-line options as
        Unicode strings if it can figure out the appropriate
        compile/link options. This is enabled at least for MSVC and
        mingw builds. That means that if non-ASCII strings are
        passed to the qpdf CLI in Windows, qpdf will now correctly
        receive them. In the past, they would have either been
        encoded as Windows code page 1252 (also known as "Windows
        ANSI" or as something unintelligible. In almost all cases,
        qpdf is able to properly interpret Unicode arguments now,
        whereas in the past, it would almost never interpret them
        properly. The result is that non-ASCII passwords given to
        the qpdf CLI on Windows now have a much greater chance of
        creating PDF files that can be opened by a variety of
        readers. In the past, usually files encrypted from the
        Windows CLI using non-ASCII passwords would not be readable
        by most viewers. Note that the current version of qpdf is
        able to decrypt files that it previously created using the
        previously supplied password.

      - The PDF specification requires passwords to be encoded as
        UTF-8 for 256-bit encryption and with PDF Doc encoding for
        40-bit or 128-bit encryption. Older versions of qpdf left it
        up to the user to provide passwords with the correct
        encoding. The qpdf CLI now detects when a password is given
        with UTF-8 encoding and automatically transcodes it to what
        the PDF spec requires. While this is almost always the
        correct behavior, it is possible to override the behavior if
        there is some reason to do so. This is discussed in more
        depth in :ref:`unicode-passwords`.

    - New options
      :qpdf:ref:`--externalize-inline-images`,
      :qpdf:ref:`--ii-min-bytes`, and
      :qpdf:ref:`--keep-inline-images` control qpdf's
      handling of inline images and possible conversion of them to
      regular images. By default,
      :qpdf:ref:`--optimize-images` now also applies to
      inline images.

    - Add options :qpdf:ref:`--overlay` and
      :qpdf:ref:`--underlay` for overlaying or
      underlaying pages of other files onto output pages. See
      :ref:`overlay-underlay` for
      details.

    - When opening an encrypted file with a password, if the
      specified password doesn't work and the password contains any
      non-ASCII characters, qpdf will try a number of alternative
      passwords to try to compensate for possible character encoding
      errors. This behavior can be suppressed with the
      :qpdf:ref:`--suppress-password-recovery` option.
      See :ref:`unicode-passwords` for a full
      discussion.

    - Add the :qpdf:ref:`--password-mode` option to
      fine-tune how qpdf interprets password arguments, especially
      when they contain non-ASCII characters. See :ref:`unicode-passwords` for more information.

    - In the :qpdf:ref:`--pages` option, it is now
      possible to copy the same page more than once from the same
      file without using the previous workaround of specifying two
      different paths to the same file.

    - In the :qpdf:ref:`--pages` option, allow use of "."
      as a shortcut for the primary input file. That way, you can do
      :command:`qpdf in.pdf --pages . 1-2 -- out.pdf`
      instead of having to repeat :file:`in.pdf`
      in the command.

    - When encrypting with 128-bit and 256-bit encryption, new
      encryption options :qpdf:ref:`--assemble`,
      :qpdf:ref:`--annotate`,
      :qpdf:ref:`--form`, and
      :qpdf:ref:`--modify-other` allow more fine-grained
      granularity in configuring options. Before, the
      :qpdf:ref:`--modify` option only configured certain
      predefined groups of permissions.

  - Bug Fixes and Enhancements

    - *Potential data-loss bug:* Versions of qpdf between 8.1.0 and
      8.3.0 had a bug that could cause page splitting and merging
      operations to drop some font or image resources if the PDF
      file's internal structure shared these resource lists across
      pages and if some but not all of the pages in the output did
      not reference all the fonts and images. Using the
      :qpdf:ref:`--preserve-unreferenced-resources`
      option would work around the incorrect behavior. This bug was
      the result of a typo in the code and a deficiency in the test
      suite. The case that triggered the error was known, just not
      handled properly. This case is now exercised in qpdf's test
      suite and properly handled.

    - When optimizing images, detect and refuse to optimize images
      that can't be converted to JPEG because of bit depth or color
      space.

    - Linearization and page manipulation APIs now detect and recover
      from files that have duplicate Page objects in the pages tree.

    - Using older option
      :samp:`--stream-data=compress` with object
      streams, object streams and xref streams were not compressed.

    - When the tokenizer returns inline image tokens, delimiters
      following ``ID`` and ``EI`` operators are no longer excluded.
      This makes it possible to reliably extract the actual image
      data.

  - Library Enhancements

    - Add method ``QPDFPageObjectHelper::externalizeInlineImages`` to
      convert inline images to regular images.

    - Add method ``QUtil::possible_repaired_encodings()`` to generate
      a list of strings that represent other ways the given string
      could have been encoded. This is the method the QPDF CLI uses
      to generate the strings it tries when recovering incorrectly
      encoded Unicode passwords.

    - Add new versions of
      ``QPDFWriter::setR{3,4,5,6}EncryptionParameters`` that allow
      more granular setting of permissions bits. See
      :file:`QPDFWriter.hh` for details.

    - Add new versions of the transcoders from UTF-8 to single-byte
      coding systems in ``QUtil`` that report success or failure
      rather than just substituting a specified unknown character.

    - Add method ``QUtil::analyze_encoding()`` to determine whether a
      string has high-bit characters and is appears to be UTF-16 or
      valid UTF-8 encoding.

    - Add new method ``QPDFPageObjectHelper::shallowCopyPage()`` to
      copy a new page that is a "shallow copy" of a page. The
      resulting object is an indirect object ready to be passed to
      ``QPDFPageDocumentHelper::addPage()`` for either the original
      ``QPDF`` object or a different one. This is what the
      :command:`qpdf` command-line tool uses to copy
      the same page multiple times from the same file during
      splitting and merging operations.

    - Add method ``QPDF::getUniqueId()``, which returns a unique
      identifier for the given QPDF object. The identifier will be
      unique across the life of the application. The returned value
      can be safely used as a map key.

    - Add method ``QPDF::setImmediateCopyFrom``. This further
      enhances qpdf's ability to allow a ``QPDF`` object from which
      objects are being copied to go out of scope before the
      destination object is written. If you call this method on a
      ``QPDF`` instances, objects copied *from* this instance will be
      copied immediately instead of lazily. This option uses more
      memory but allows the source object to go out of scope before
      the destination object is written in all cases. See comments in
      :file:`QPDF.hh` for details.

    - Add method ``QPDFPageObjectHelper::getAttribute`` for
      retrieving an attribute from the page dictionary taking
      inheritance into consideration, and optionally making a copy if
      your intention is to modify the attribute.

    - Fix long-standing limitation of
      ``QPDFPageObjectHelper::getPageImages`` so that it now properly
      reports images from inherited resources dictionaries,
      eliminating the need to call
      ``QPDFPageDocumentHelper::pushInheritedAttributesToPage`` in
      this case.

    - Add method ``QPDFObjectHandle::getUniqueResourceName`` for
      finding an unused name in a resource dictionary.

    - Add method ``QPDFPageObjectHelper::getFormXObjectForPage`` for
      generating a form XObject equivalent to a page. The resulting
      object can be used in the same file or copied to another file
      with ``copyForeignObject``. This can be useful for implementing
      underlay, overlay, n-up, thumbnails, or any other functionality
      requiring replication of pages in other contexts.

    - Add method ``QPDFPageObjectHelper::placeFormXObject`` for
      generating content stream text that places a given form XObject
      on a page, centered and fit within a specified rectangle. This
      method takes care of computing the proper transformation matrix
      and may optionally compensate for rotation or scaling of the
      destination page.

    - Exit codes returned by ``QPDFJob::run()`` and the C API wrappers
      are now defined in :file:`qpdf/Constants.h` in the
      ``qpdf_exit_code_e`` type so that they are accessible from the C
      API. They were previously only defined as constants in
      :file:`qpdf/QPDFJob.hh`.

  - Build Improvements

    - Add new configure option
      :samp:`--enable-avoid-windows-handle`, which
      causes the preprocessor symbol ``AVOID_WINDOWS_HANDLE`` to be
      defined. When defined, qpdf will avoid referencing the Windows
      ``HANDLE`` type, which is disallowed with certain versions of
      the Windows SDK.

    - For Windows builds, attempt to determine what options, if any,
      have to be passed to the compiler and linker to enable use of
      ``wmain``. This causes the preprocessor symbol
      ``WINDOWS_WMAIN`` to be defined. If you do your own builds with
      other compilers, you can define this symbol to cause ``wmain``
      to be used. This is needed to allow the Windows
      :command:`qpdf` command to receive Unicode
      command-line options.

8.3.0: January 7, 2019
  - Command-line Enhancements

    - Shell completion: you can now use eval :command:`$(qpdf
      --completion-bash)` and eval :command:`$(qpdf
      --completion-zsh)` to enable shell completion for
      bash and zsh.

    - Page numbers (also known as page labels) are now preserved when
      merging and splitting files with the
      :qpdf:ref:`--pages` and
      :qpdf:ref:`--split-pages` options.

    - Bookmarks are partially preserved when splitting pages with the
      :qpdf:ref:`--split-pages` option. Specifically, the
      outlines dictionary and some supporting metadata are copied
      into the split files. The result is that all bookmarks from the
      original file appear, those that point to pages that are
      preserved work, and those that point to pages that are not
      preserved don't do anything. This is an interim step toward
      proper support for bookmarks in splitting and merging
      operations.

    - Page collation: add new option
      :qpdf:ref:`--collate`. When specified, the
      semantics of :qpdf:ref:`--pages` change from
      concatenation to collation. See :ref:`page-selection` for examples and discussion.

    - Generation of information in JSON format, primarily to
      facilitate use of qpdf from languages other than C++. Add new
      options :qpdf:ref:`--json`,
      :qpdf:ref:`--json-key`, and
      :qpdf:ref:`--json-object` to generate a JSON
      representation of the PDF file. Run :command:`qpdf
      --json-help` to get a description of the JSON
      format. For more information, see :ref:`json`.

    - The :qpdf:ref:`--generate-appearances` flag will
      cause qpdf to generate appearances for form fields if the PDF
      file indicates that form field appearances are out of date.
      This can happen when PDF forms are filled in by a program that
      doesn't know how to regenerate the appearances of the filled-in
      fields.

    - The :qpdf:ref:`--flatten-annotations` flag can be
      used to *flatten* annotations, including form fields.
      Ordinarily, annotations are drawn separately from the page.
      Flattening annotations is the process of combining their
      appearances into the page's contents. You might want to do this
      if you are going to rotate or combine pages using a tool that
      doesn't understand about annotations. You may also want to use
      :qpdf:ref:`--generate-appearances` when using this
      flag since annotations for outdated form fields are not
      flattened as that would cause loss of information.

    - The :qpdf:ref:`--optimize-images` flag tells qpdf
      to recompresses every image using DCT (JPEG) compression as
      long as the image is not already compressed with lossy
      compression and recompressing the image reduces its size. The
      additional options :qpdf:ref:`--oi-min-width`,
      :qpdf:ref:`--oi-min-height`, and
      :qpdf:ref:`--oi-min-area` prevent recompression of
      images whose width, height, or pixel area (width × height) are
      below a specified threshold.

    - The :qpdf:ref:`--show-object` option can now be
      given as :samp:`--show-object=trailer` to show
      the trailer dictionary.

  - Bug Fixes and Enhancements

    - QPDF now automatically detects and recovers from dangling
      references. If a PDF file contained an indirect reference to a
      non-existent object, which is valid, when adding a new object
      to the file, it was possible for the new object to take the
      object ID of the dangling reference, thereby causing the
      dangling reference to point to the new object. This case is now
      prevented.

    - Fixes to form field setting code: strings are always written in
      UTF-16 format, and checkboxes and radio buttons are handled
      properly with respect to synchronization of values and
      appearance states.

    - The ``QPDF::checkLinearization()`` no longer causes the program
      to crash when it detects problems with linearization data.
      Instead, it issues a normal warning or error.

    - Ordinarily qpdf treats an argument of the form
      :samp:`@file` to mean that command-line options
      should be read from :file:`file`. Now, if
      :file:`file` does not exist but
      :file:`@file` does, qpdf will treat
      :file:`@file` as a regular option. This
      makes it possible to work more easily with PDF files whose
      names happen to start with the ``@`` character.

  - Library Enhancements

    - Remove the restriction in most cases that the source QPDF
      object used in a ``QPDF::copyForeignObject`` call has to stick
      around until the destination QPDF is written. The exceptional
      case is when the source stream gets is data using a
      QPDFObjectHandle::StreamDataProvider. For a more in-depth
      discussion, see comments around ``copyForeignObject`` in
      :file:`QPDF.hh`.

    - Add new method ``QPDFWriter::getFinalVersion()``, which returns
      the PDF version that will ultimately be written to the final
      file. See comments in :file:`QPDFWriter.hh`
      for some restrictions on its use.

    - Add several methods for transcoding strings to some of the
      character sets used in PDF files: ``QUtil::utf8_to_ascii``,
      ``QUtil::utf8_to_win_ansi``, ``QUtil::utf8_to_mac_roman``, and
      ``QUtil::utf8_to_utf16``. For the single-byte encodings that
      support only a limited character sets, these methods replace
      unsupported characters with a specified substitute.

    - Add new methods to ``QPDFAnnotationObjectHelper`` and
      ``QPDFFormFieldObjectHelper`` for querying flags and
      interpretation of different field types. Define constants in
      :file:`qpdf/Constants.h` to help with
      interpretation of flag values.

    - Add new methods
      ``QPDFAcroFormDocumentHelper::generateAppearancesIfNeeded`` and
      ``QPDFFormFieldObjectHelper::generateAppearance`` for
      generating appearance streams. See discussion in
      :file:`QPDFFormFieldObjectHelper.hh` for
      limitations.

    - Add two new helper functions for dealing with resource
      dictionaries: ``QPDFObjectHandle::getResourceNames()`` returns
      a list of all second-level keys, which correspond to the names
      of resources, and ``QPDFObjectHandle::mergeResources()`` merges
      two resources dictionaries as long as they have non-conflicting
      keys. These methods are useful for certain types of objects
      that resolve resources from multiple places, such as form
      fields.

    - Add methods ``QPDFPageDocumentHelper::flattenAnnotations()``
      and
      ``QPDFAnnotationObjectHelper::getPageContentForAppearance()``
      for handling low-level details of annotation flattening.

    - Add new helper classes: ``QPDFOutlineDocumentHelper``,
      ``QPDFOutlineObjectHelper``, ``QPDFPageLabelDocumentHelper``,
      ``QPDFNameTreeObjectHelper``, and
      ``QPDFNumberTreeObjectHelper``.

    - Add method ``QPDFObjectHandle::getJSON()`` that returns a JSON
      representation of the object. Call ``serialize()`` on the
      result to convert it to a string.

    - Add a simple JSON serializer. This is not a complete or
      general-purpose JSON library. It allows assembly and
      serialization of JSON structures with some restrictions, which
      are described in the header file. This is the serializer used
      by qpdf's new JSON representation.

    - Add new ``QPDFObjectHandle::Matrix`` class along with a few
      convenience methods for dealing with six-element numerical
      arrays as matrices.

    - Add new method ``QPDFObjectHandle::wrapInArray``, which returns
      the object itself if it is an array, or an array containing the
      object otherwise. This is a common construct in PDF. This
      method prevents you from having to explicitly test whether
      something is a single element or an array.

  - Build Improvements

    - It is no longer necessary to run
      :command:`autogen.sh` to build from a pristine
      checkout. Automatically generated files are now committed so
      that it is possible to build on platforms without autoconf
      directly from a clean checkout of the repository. The
      :command:`configure` script detects if the files
      are out of date when it also determines that the tools are
      present to regenerate them.

    - Pull requests and the master branch are now built automatically
      in `Azure
      Pipelines <https://dev.azure.com/qpdf/qpdf/_build>`__, which is
      free for open source projects. The build includes Linux, mac,
      Windows 32-bit and 64-bit with mingw and MSVC, and an AppImage
      build. Official qpdf releases are now built with Azure
      Pipelines.

  - Notes for Packagers

    - A new section has been added to the documentation with notes
      for packagers. Please see :ref:`packaging`.

    - The qpdf detects out-of-date automatically generated files. If
      your packaging system automatically refreshes libtool or
      autoconf files, it could cause this check to fail. To avoid
      this problem, pass
      :samp:`--disable-check-autofiles` to
      :command:`configure`.

    - If you would like to have qpdf completion enabled
      automatically, you can install completion files in the
      distribution's default location. You can find sample completion
      files to install in the :file:`completions`
      directory.

8.2.1: August 18, 2018
  - Command-line Enhancements

    - Add
      :samp:`--keep-files-open={[yn]}`
      to override default determination of whether to keep files open
      when merging. Please see the discussion of
      :qpdf:ref:`--keep-files-open` for additional details.

8.2.0: August 16, 2018
  - Command-line Enhancements

    - Add :qpdf:ref:`--no-warn` option to suppress
      issuing warning messages. If there are any conditions that
      would have caused warnings to be issued, the exit status is
      still 3.

  - Bug Fixes and Optimizations

    - Performance fix: optimize page merging operation to avoid
      unnecessary open/close calls on files being merged. This solves
      a dramatic slow-down that was observed when merging certain
      types of files.

    - Optimize how memory was used for the TIFF predictor,
      drastically improving performance and memory usage for files
      containing high-resolution images compressed with Flate using
      the TIFF predictor.

    - Bug fix: end of line characters were not properly handled
      inside strings in some cases.

    - Bug fix: using :qpdf:ref:`--progress` on very small
      files could cause an infinite loop.

  - API enhancements

    - Add new class ``QPDFSystemError``, derived from
      ``std::runtime_error``, which is now thrown by
      ``QUtil::throw_system_error``. This enables the triggering
      ``errno`` value to be retrieved.

    - Add ``ClosedFileInputSource::stayOpen`` method, enabling a
      ``ClosedFileInputSource`` to stay open during manually
      indicated periods of high activity, thus reducing the overhead
      of frequent open/close operations.

  - Build Changes

    - For the mingw builds, change the name of the DLL import library
      from :file:`libqpdf.a` to
      :file:`libqpdf.dll.a` to more accurately
      reflect that it is an import library rather than a static
      library. This potentially clears the way for supporting a
      static library in the future, though presently, the qpdf
      Windows build only builds the DLL and executables.

8.1.0: June 23, 2018
  - Usability Improvements

    - When splitting files, qpdf detects fonts and images that the
      document metadata claims are referenced from a page but are not
      actually referenced and omits them from the output file. This
      change can cause a significant reduction in the size of split
      PDF files for files created by some software packages. In some
      cases, it can also make page splitting slower. Prior versions
      of qpdf would believe the document metadata and sometimes
      include all the images from all the other pages even though the
      pages were no longer present. In the unlikely event that the
      old behavior should be desired, or if you have a case where
      page splitting is very slow, the old behavior (and speed) can
      be enabled by specifying
      :qpdf:ref:`--preserve-unreferenced-resources`.

    - When merging multiple PDF files, qpdf no longer leaves all the
      files open. This makes it possible to merge numbers of files
      that may exceed the operating system's limit for the maximum
      number of open files.

    - The :qpdf:ref:`--rotate` option's syntax has been
      extended to make the page range optional. If you specify
      :samp:`--rotate={angle}`
      without specifying a page range, the rotation will be applied
      to all pages. This can be especially useful for adjusting a PDF
      created from a multi-page document that was scanned upside
      down.

    - When merging multiple files, the
      :qpdf:ref:`--verbose` option now prints information
      about each file as it operates on that file.

    - When the :qpdf:ref:`--progress` option is
      specified, qpdf will print a running indicator of its best
      guess at how far through the writing process it is. Note that,
      as with all progress meters, it's an approximation. This option
      is implemented in a way that makes it useful for software that
      uses the qpdf library; see API Enhancements below.

  - Bug Fixes

    - Properly decrypt files that use revision 3 of the standard
      security handler but use 40 bit keys (even though revision 3
      supports 128-bit keys).

    - Limit depth of nested data structures to prevent crashes from
      certain types of malformed (malicious) PDFs.

    - In "newline before endstream" mode, insert the required extra
      newline before the ``endstream`` at the end of object streams.
      This one case was previously omitted.

  - API Enhancements

    - The first round of higher level "helper" interfaces has been
      introduced. These are designed to provide a more convenient way
      of interacting with certain document features than using
      ``QPDFObjectHandle`` directly. For details on helpers, see
      :ref:`helper-classes`. Specific additional
      interfaces are described below.

    - Add two new document helper classes: ``QPDFPageDocumentHelper``
      for working with pages, and ``QPDFAcroFormDocumentHelper`` for
      working with interactive forms. No old methods have been
      removed, but ``QPDFPageDocumentHelper`` is now the preferred
      way to perform operations on pages rather than calling the old
      methods in ``QPDFObjectHandle`` and ``QPDF`` directly. Comments
      in the header files direct you to the new interfaces. Please
      see the header files and :file:`ChangeLog`
      for additional details.

    - Add three new object helper class: ``QPDFPageObjectHelper`` for
      pages, ``QPDFFormFieldObjectHelper`` for interactive form
      fields, and ``QPDFAnnotationObjectHelper`` for annotations. All
      three classes are fairly sparse at the moment, but they have
      some useful, basic functionality.

    - A new example program
      :file:`examples/pdf-set-form-values.cc` has
      been added that illustrates use of the new document and object
      helpers.

    - The method ``QPDFWriter::registerProgressReporter`` has been
      added. This method allows you to register a function that is
      called by ``QPDFWriter`` to update your idea of the percentage
      it thinks it is through writing its output. Client programs can
      use this to implement reasonably accurate progress meters. The
      :command:`qpdf` command line tool uses this to
      implement its :qpdf:ref:`--progress` option.

    - New methods ``QPDFObjectHandle::newUnicodeString`` and
      ``QPDFObject::unparseBinary`` have been added to allow for more
      convenient creation of strings that are explicitly encoded
      using big-endian UTF-16. This is useful for creating strings
      that appear outside of content streams, such as labels, form
      fields, outlines, document metadata, etc.

    - A new class ``QPDFObjectHandle::Rectangle`` has been added to
      ease working with PDF rectangles, which are just arrays of four
      numeric values.

8.0.2: March 6, 2018
  - When a loop is detected while following cross reference streams or
    tables, treat this as damage instead of silently ignoring the
    previous table. This prevents loss of otherwise recoverable data
    in some damaged files.

  - Properly handle pages with no contents.

8.0.1: March 4, 2018
  - Disregard data check errors when uncompressing ``/FlateDecode``
    streams. This is consistent with most other PDF readers and allows
    qpdf to recover data from another class of malformed PDF files.

  - On the command line when specifying page ranges, support preceding
    a page number by "r" to indicate that it should be counted from
    the end. For example, the range ``r3-r1`` would indicate the last
    three pages of a document.

8.0.0: February 25, 2018
  - Packaging and Distribution Changes

    - QPDF is now distributed as an
      `AppImage <https://appimage.org/>`__ in addition to all the
      other ways it is distributed. The AppImage can be found in the
      download area with the other packages. Thanks to Kurt Pfeifle
      and Simon Peter for their contributions.

  - Bug Fixes

    - ``QPDFObjectHandle::getUTF8Val`` now properly treats
      non-Unicode strings as encoded with PDF Doc Encoding.

    - Improvements to handling of objects in PDF files that are not
      of the expected type. In most cases, qpdf will be able to warn
      for such cases rather than fail with an exception. Previous
      versions of qpdf would sometimes fail with errors such as
      "operation for dictionary object attempted on object of wrong
      type". This situation should be mostly or entirely eliminated
      now.

  - Enhancements to the :command:`qpdf` Command-line
    Tool. All new options listed here are documented in more detail in
    :ref:`using`.

    - The option
      :samp:`--linearize-pass1={file}`
      has been added for debugging qpdf's linearization code.

    - The option :qpdf:ref:`--coalesce-contents` can be
      used to combine content streams of a page whose contents are an
      array of streams into a single stream.

  - API Enhancements. All new API calls are documented in their
    respective classes' header files. There are no non-compatible
    changes to the API.

    - Add function ``qpdf_check_pdf`` to the C API. This function
      does basic checking that is a subset of what :command:`qpdf
      --check` performs.

    - Major enhancements to the lexical layer of qpdf. For a complete
      list of enhancements, please refer to the
      :file:`ChangeLog` file. Most of the changes
      result in improvements to qpdf's ability handle erroneous
      files. It is also possible for programs to handle whitespace,
      comments, and inline images as tokens.

    - New API for working with PDF content streams at a lexical
      level. The new class ``QPDFObjectHandle::TokenFilter`` allows
      the developer to provide token handlers. Token filters can be
      used with several different methods in ``QPDFObjectHandle`` as
      well as with a lower-level interface. See comments in
      :file:`QPDFObjectHandle.hh` as well as the
      new examples
      :file:`examples/pdf-filter-tokens.cc` and
      :file:`examples/pdf-count-strings.cc` for
      details.

7.1.1: February 4, 2018
  - Bug fix: files whose /ID fields were other than 16 bytes long can
    now be properly linearized

  - A few compile and link issues have been corrected for some
    platforms.

7.1.0: January 14, 2018
  - PDF files contain streams that may be compressed with various
    compression algorithms which, in some cases, may be enhanced by
    various predictor functions. Previously only the PNG up predictor
    was supported. In this version, all the PNG predictors as well as
    the TIFF predictor are supported. This increases the range of
    files that qpdf is able to handle.

  - QPDF now allows a raw encryption key to be specified in place of a
    password when opening encrypted files, and will optionally display
    the encryption key used by a file. This is a non-standard
    operation, but it can be useful in certain situations. Please see
    the discussion of :qpdf:ref:`--password-is-hex-key` or the comments around
    ``QPDF::setPasswordIsHexKey`` in
    :file:`QPDF.hh` for additional details.

  - Bug fix: numbers ending with a trailing decimal point are now
    properly recognized as numbers.

  - Bug fix: when building qpdf from source on some platforms
    (especially MacOS), the build could get confused by older versions
    of qpdf installed on the system. This has been corrected.

7.0.0: September 15, 2017
  - Packaging and Distribution Changes

    - QPDF's primary license is now `version 2.0 of the Apache
      License <http://www.apache.org/licenses/LICENSE-2.0>`__ rather
      than version 2.0 of the Artistic License. You may still, at
      your option, consider qpdf to be licensed with version 2.0 of
      the Artistic license.

    - QPDF no longer has a dependency on the PCRE (Perl-Compatible
      Regular Expression) library. QPDF now has an added dependency
      on the JPEG library.

  - Bug Fixes

    - This release contains many bug fixes for various infinite
      loops, memory leaks, and other memory errors that could be
      encountered with specially crafted or otherwise erroneous PDF
      files.

  - New Features

    - QPDF now supports reading and writing streams encoded with JPEG
      or RunLength encoding. Library API enhancements and
      command-line options have been added to control this behavior.
      See command-line options
      :qpdf:ref:`--compress-streams` and
      :qpdf:ref:`--decode-level` and methods
      ``QPDFWriter::setCompressStreams`` and
      ``QPDFWriter::setDecodeLevel``.

    - QPDF is much better at recovering from broken files. In most
      cases, qpdf will skip invalid objects and will preserve broken
      stream data by not attempting to filter broken streams. QPDF is
      now able to recover or at least not crash on dozens of broken
      test files I have received over the past few years.

    - Page rotation is now supported and accessible from both the
      library and the command line.

    - ``QPDFWriter`` supports writing files in a way that preserves
      PCLm compliance in support of driverless printing. This is very
      specialized and is only useful to applications that already
      know how to create PCLm files.

  - Enhancements to the :command:`qpdf` Command-line
    Tool. All new options listed here are documented in more detail in
    :ref:`using`.

    - Command-line arguments can now be read from files or standard
      input using ``@file`` or ``@-`` syntax. Please see :ref:`invocation`.

    - :qpdf:ref:`--rotate`: request page rotation

    - :qpdf:ref:`--newline-before-endstream`: ensure that
      a newline appears before every ``endstream`` keyword in the
      file; used to prevent qpdf from breaking PDF/A compliance on
      already compliant files.

    - :qpdf:ref:`--preserve-unreferenced`: preserve
      unreferenced objects in the input PDF

    - :qpdf:ref:`--split-pages`: break output into chunks
      with fixed numbers of pages

    - :qpdf:ref:`--verbose`: print the name of each
      output file that is created

    - :qpdf:ref:`--compress-streams` and
      :qpdf:ref:`--decode-level` replace
      :qpdf:ref:`--stream-data` for improving granularity
      of controlling compression and decompression of stream data.
      The :qpdf:ref:`--stream-data` option will remain
      available.

    - When running :command:`qpdf --check` with other
      options, checks are always run first. This enables qpdf to
      perform its full recovery logic before outputting other
      information. This can be especially useful when manually
      recovering broken files, looking at qpdf's regenerated cross
      reference table, or other similar operations.

    - Process :command:`--pages` earlier so that other
      options like :qpdf:ref:`--show-pages` or
      :qpdf:ref:`--split-pages` can operate on the file
      after page splitting/merging has occurred.

  - API Changes. All new API calls are documented in their respective
    classes' header files.

    - ``QPDFObjectHandle::rotatePage``: apply rotation to a page
      object

    - ``QPDFWriter::setNewlineBeforeEndstream``: force newline to
      appear before ``endstream``

    - ``QPDFWriter::setPreserveUnreferencedObjects``: preserve
      unreferenced objects that appear in the input PDF. The default
      behavior is to discard them.

    - New ``Pipeline`` types ``Pl_RunLength`` and ``Pl_DCT`` are
      available for developers who wish to produce or consume
      RunLength or DCT stream data directly. The
      :file:`examples/pdf-create.cc` example
      illustrates their use.

    - ``QPDFWriter::setCompressStreams`` and
      ``QPDFWriter::setDecodeLevel`` methods control handling of
      different types of stream compression.

    - Add new C API functions ``qpdf_set_compress_streams``,
      ``qpdf_set_decode_level``,
      ``qpdf_set_preserve_unreferenced_objects``, and
      ``qpdf_set_newline_before_endstream`` corresponding to the new
      ``QPDFWriter`` methods.

6.0.0: November 10, 2015
  - Implement :qpdf:ref:`--deterministic-id` command-line
    option and ``QPDFWriter::setDeterministicID`` as well as C API
    function ``qpdf_set_deterministic_ID`` for generating a
    deterministic ID for non-encrypted files. When this option is
    selected, the ID of the file depends on the contents of the output
    file, and not on transient items such as the timestamp or output
    file name.

  - Make qpdf more tolerant of files whose xref table entries are not
    the correct length.

5.1.3: May 24, 2015
  - Bug fix: fix-qdf was not properly handling files that contained
    object streams with more than 255 objects in them.

  - Bug fix: qpdf was not properly initializing Microsoft's secure
    crypto provider on fresh Windows installations that had not had
    any keys created yet.

  - Fix a few errors found by Gynvael Coldwind and Mateusz Jurczyk of
    the Google Security Team. Please see the ChangeLog for details.

  - Properly handle pages that have no contents at all. There were
    many cases in which qpdf handled this fine, but a few methods
    blindly obtained page contents with handling the possibility that
    there were no contents.

  - Make qpdf more robust for a few more kinds of problems that may
    occur in invalid PDF files.

5.1.2: June 7, 2014
  - Bug fix: linearizing files could create a corrupted output file
    under extremely unlikely file size circumstances. See ChangeLog
    for details. The odds of getting hit by this are very low, though
    one person did.

  - Bug fix: qpdf would fail to write files that had streams with
    decode parameters referencing other streams.

  - New example program: :command:`pdf-split-pages`:
    efficiently split PDF files into individual pages. The example
    program does this more efficiently than using :command:`qpdf
    --pages` to do it.

  - Packaging fix: Visual C++ binaries did not support Windows XP.
    This has been rectified by updating the compilers used to generate
    the release binaries.

5.1.1: January 14, 2014
  - Performance fix: copying foreign objects could be very slow with
    certain types of files. This was most likely to be visible during
    page splitting and was due to traversing the same objects multiple
    times in some cases.

5.1.0: December 17, 2013
  - Added runtime option (``QUtil::setRandomDataProvider``) to supply
    your own random data provider. You can use this if you want to
    avoid using the OS-provided secure random number generation
    facility or stdlib's less secure version. See comments in
    include/qpdf/QUtil.hh for details.

  - Fixed image comparison tests to not create 12-bit-per-pixel images
    since some versions of tiffcmp have bugs in comparing them in some
    cases. This increases the disk space required by the image
    comparison tests, which are off by default anyway.

  - Introduce a number of small fixes for compilation on the latest
    clang in MacOS and the latest Visual C++ in Windows.

  - Be able to handle broken files that end the xref table header with
    a space instead of a newline.

5.0.1: October 18, 2013
  - Thanks to a detailed review by Florian Weimer and the Red Hat
    Product Security Team, this release includes a number of
    non-user-visible security hardening changes. Please see the
    ChangeLog file in the source distribution for the complete list.

  - When available, operating system-specific secure random number
    generation is used for generating initialization vectors and other
    random values used during encryption or file creation. For the
    Windows build, this results in an added dependency on Microsoft's
    cryptography API. To disable the OS-specific cryptography and use
    the old version, pass the
    :samp:`--enable-insecure-random` option to
    :command:`./configure`.

  - The :command:`qpdf` command-line tool now issues a
    warning when :samp:`-accessibility=n` is specified
    for newer encryption versions stating that the option is ignored.
    qpdf, per the spec, has always ignored this flag, but it
    previously did so silently. This warning is issued only by the
    command-line tool, not by the library. The library's handling of
    this flag is unchanged.

5.0.0: July 10, 2013
  - Bug fix: previous versions of qpdf would lose objects with
    generation != 0 when generating object streams. Fixing this
    required changes to the public API.

  - Removed methods from public API that were only supposed to be
    called by QPDFWriter and couldn't realistically be called anywhere
    else. See ChangeLog for details.

  - New ``QPDFObjGen`` class added to represent an object
    ID/generation pair. ``QPDFObjectHandle::getObjGen()`` is now
    preferred over ``QPDFObjectHandle::getObjectID()`` and
    ``QPDFObjectHandle::getGeneration()`` as it makes it less likely
    for people to accidentally write code that ignores the generation
    number. See :file:`QPDF.hh` and
    :file:`QPDFObjectHandle.hh` for additional
    notes.

  - Add :qpdf:ref:`--show-npages` command-line option to
    the :command:`qpdf` command to show the number of
    pages in a file.

  - Allow omission of the page range within
    :qpdf:ref:`--pages` for the
    :command:`qpdf` command. When omitted, the page
    range is implicitly taken to be all the pages in the file.

  - Various enhancements were made to support different types of
    broken files or broken readers. Details can be found in
    :file:`ChangeLog`.

4.1.0: April 14, 2013
  - Note to people including qpdf in distributions: the
    :file:`.la` files generated by libtool are now
    installed by qpdf's :command:`make install` target.
    Before, they were not installed. This means that if your
    distribution does not want to include
    :file:`.la` files, you must remove them as
    part of your packaging process.

  - Major enhancement: API enhancements have been made to support
    parsing of content streams. This enhancement includes the
    following changes:

    - ``QPDFObjectHandle::parseContentStream`` method parses objects
      in a content stream and calls handlers in a callback class. The
      example
      :file:`examples/pdf-parse-content.cc`
      illustrates how this may be used.

    - ``QPDFObjectHandle`` can now represent operators and inline
      images, object types that may only appear in content streams.

    - Method ``QPDFObjectHandle::getTypeCode()`` returns an
      enumerated type value representing the underlying object type.
      Method ``QPDFObjectHandle::getTypeName()`` returns a text
      string describing the name of the type of a
      ``QPDFObjectHandle`` object. These methods can be used for more
      efficient parsing and debugging/diagnostic messages.

  - :command:`qpdf --check` now parses all pages'
    content streams in addition to doing other checks. While there are
    still many types of errors that cannot be detected, syntactic
    errors in content streams will now be reported.

  - Minor compilation enhancements have been made to facilitate easier
    for support for a broader range of compilers and compiler
    versions.

    - Warning flags have been moved into a separate variable in
      :file:`autoconf.mk`

    - The configure flag :samp:`--enable-werror` work
      for Microsoft compilers

    - All MSVC CRT security warnings have been resolved.

    - All C-style casts in C++ Code have been replaced by C++ casts,
      and many casts that had been included to suppress higher
      warning levels for some compilers have been removed, primarily
      for clarity. Places where integer type coercion occurs have
      been scrutinized. A new casting policy has been documented in
      the manual. This is of concern mainly to people porting qpdf to
      new platforms or compilers. It is not visible to programmers
      writing code that uses the library

    - Some internal limits have been removed in code that converts
      numbers to strings. This is largely invisible to users, but it
      does trigger a bug in some older versions of mingw-w64's C++
      library. See :file:`README-windows.md` in
      the source distribution if you think this may affect you. The
      copy of the DLL distributed with qpdf's binary distribution is
      not affected by this problem.

  - The RPM spec file previously included with qpdf has been removed.
    This is because virtually all Linux distributions include qpdf now
    that it is a dependency of CUPS filters.

  - A few bug fixes are included:

    - Overridden compressed objects are properly handled. Before,
      there were certain constructs that could cause qpdf to see old
      versions of some objects. The most usual manifestation of this
      was loss of filled in form values for certain files.

    - Installation no longer uses GNU/Linux-specific versions of some
      commands, so :command:`make install` works on
      Solaris with native tools.

    - The 64-bit mingw Windows binary package no longer includes a
      32-bit DLL.

4.0.1: January 17, 2013
  - Fix detection of binary attachments in test suite to avoid false
    test failures on some platforms.

  - Add clarifying comment in :file:`QPDF.hh` to
    methods that return the user password explaining that it is no
    longer possible with newer encryption formats to recover the user
    password knowing the owner password. In earlier encryption
    formats, the user password was encrypted in the file using the
    owner password. In newer encryption formats, a separate encryption
    key is used on the file, and that key is independently encrypted
    using both the user password and the owner password.

4.0.0: December 31, 2012
  - Major enhancement: support has been added for newer encryption
    schemes supported by version X of Adobe Acrobat. This includes use
    of 127-character passwords, 256-bit encryption keys, and the
    encryption scheme specified in ISO 32000-2, the PDF 2.0
    specification. This scheme can be chosen from the command line by
    specifying use of 256-bit keys. qpdf also supports the deprecated
    encryption method used by Acrobat IX. This encryption style has
    known security weaknesses and should not be used in practice.
    However, such files exist "in the wild," so support for this
    scheme is still useful. New methods
    ``QPDFWriter::setR6EncryptionParameters`` (for the PDF 2.0 scheme)
    and ``QPDFWriter::setR5EncryptionParameters`` (for the deprecated
    scheme) have been added to enable these new encryption schemes.
    Corresponding functions have been added to the C API as well.

  - Full support for Adobe extension levels in PDF version
    information. Starting with PDF version 1.7, corresponding to ISO
    32000, Adobe adds new functionality by increasing the extension
    level rather than increasing the version. This support includes
    addition of the ``QPDF::getExtensionLevel`` method for retrieving
    the document's extension level, addition of versions of
    ``QPDFWriter::setMinimumPDFVersion`` and
    ``QPDFWriter::forcePDFVersion`` that accept an extension level,
    and extended syntax for specifying forced and minimum versions on
    the command line as described in :qpdf:ref:`--force-version` and
    :qpdf:ref:`--min-version`. Corresponding functions have been added
    to the C API as well.

  - Minor fixes to prevent qpdf from referencing objects in the file
    that are not referenced in the file's overall structure. Most
    files don't have any such objects, but some files have contain
    unreferenced objects with errors, so these fixes prevent qpdf from
    needlessly rejecting or complaining about such objects.

  - Add new generalized methods for reading and writing files from/to
    programmer-defined sources. The method
    ``QPDF::processInputSource`` allows the programmer to use any
    input source for the input file, and
    ``QPDFWriter::setOutputPipeline`` allows the programmer to write
    the output file through any pipeline. These methods would make it
    possible to perform any number of specialized operations, such as
    accessing external storage systems, creating bindings for qpdf in
    other programming languages that have their own I/O systems, etc.

  - Add new method ``QPDF::getEncryptionKey`` for retrieving the
    underlying encryption key used in the file.

  - This release includes a small handful of non-compatible API
    changes. While effort is made to avoid such changes, all the
    non-compatible API changes in this version were to parts of the
    API that would likely never be used outside the library itself. In
    all cases, the altered methods or structures were parts of the
    ``QPDF`` that were public to enable them to be called from either
    ``QPDFWriter`` or were part of validation code that was
    over-zealous in reporting problems in parts of the file that would
    not ordinarily be referenced. In no case did any of the removed
    methods do anything worse that falsely report error conditions in
    files that were broken in ways that didn't matter. The following
    public parts of the ``QPDF`` class were changed in a
    non-compatible way:

    - Updated nested ``QPDF::EncryptionData`` class to add fields
      needed by the newer encryption formats, member variables
      changed to private so that future changes will not require
      breaking backward compatibility.

    - Added additional parameters to ``compute_data_key``, which is
      used by ``QPDFWriter`` to compute the encryption key used to
      encrypt a specific object.

    - Removed the method ``flattenScalarReferences``. This method was
      previously used prior to writing a new PDF file, but it has the
      undesired side effect of causing qpdf to read objects in the
      file that were not referenced. Some otherwise files have
      unreferenced objects with errors in them, so this could cause
      qpdf to reject files that would be accepted by virtually all
      other PDF readers. In fact, qpdf relied on only a very small
      part of what flattenScalarReferences did, so only this part has
      been preserved, and it is now done directly inside
      ``QPDFWriter``.

    - Removed the method ``decodeStreams``. This method was used by
      the :qpdf:ref:`--check` option of the
      :command:`qpdf` command-line tool to force all
      streams in the file to be decoded, but it also suffered from
      the problem of opening otherwise unreferenced streams and thus
      could report false positive. The
      :qpdf:ref:`--check` option now causes qpdf to go
      through all the motions of writing a new file based on the
      original one, so it will always reference and check exactly
      those parts of a file that any ordinary viewer would check.

    - Removed the method ``trimTrailerForWrite``. This method was
      used by ``QPDFWriter`` to modify the original QPDF object by
      removing fields from the trailer dictionary that wouldn't apply
      to the newly written file. This functionality, though generally
      harmless, was a poor implementation and has been replaced by
      having QPDFWriter filter these out when copying the trailer
      rather than modifying the original QPDF object. (Note that qpdf
      never modifies the original file itself.)

  - Allow the PDF header to appear anywhere in the first 1024 bytes of
    the file. This is consistent with what other readers do.

  - Fix the :command:`pkg-config` files to list zlib
    and pcre in ``Requires.private`` to better support static linking
    using :command:`pkg-config`.

3.0.2: September 6, 2012
  - Bug fix: ``QPDFWriter::setOutputMemory`` did not work when not
    used with ``QPDFWriter::setStaticID``, which made it pretty much
    useless. This has been fixed.

  - New API call ``QPDFWriter::setExtraHeaderText`` inserts additional
    text near the header of the PDF file. The intended use case is to
    insert comments that may be consumed by a downstream application,
    though other use cases may exist.

3.0.1: August 11, 2012
  - Version 3.0.0 included addition of files for
    :command:`pkg-config`, but this was not mentioned
    in the release notes. The release notes for 3.0.0 were updated to
    mention this.

  - Bug fix: if an object stream ended with a scalar object not
    followed by space, qpdf would incorrectly report that it
    encountered a premature EOF. This bug has been in qpdf since
    version 2.0.

3.0.0: August 2, 2012
  - Acknowledgment: I would like to express gratitude for the
    contributions of Tobias Hoffmann toward the release of qpdf
    version 3.0. He is responsible for most of the implementation and
    design of the new API for manipulating pages, and contributed code
    and ideas for many of the improvements made in version 3.0.
    Without his work, this release would certainly not have happened
    as soon as it did, if at all.

  - *Non-compatible API changes:*

    - The method ``QPDFObjectHandle::replaceStreamData`` that uses a
      ``StreamDataProvider`` to provide the stream data no longer
      takes a ``length`` parameter. The parameter was removed since
      this provides the user an opportunity to simplify the calling
      code. This method was introduced in version 2.2. At the time,
      the ``length`` parameter was required in order to ensure that
      calls to the stream data provider returned the same length for a
      specific stream every time they were invoked. In particular, the
      linearization code depends on this. Instead, qpdf 3.0 and newer
      check for that constraint explicitly. The first time the stream
      data provider is called for a specific stream, the actual length
      is saved, and subsequent calls are required to return the same
      number of bytes. This means the calling code no longer has to
      compute the length in advance, which can be a significant
      simplification. If your code fails to compile because of the
      extra argument and you don't want to make other changes to your
      code, just omit the argument.

    - Many methods take ``long long`` instead of other integer types.
      Most if not all existing code should compile fine with this
      change since such parameters had always previously been smaller
      types. This change was required to support files larger than two
      gigabytes in size.

  - Support has been added for large files. The test suite verifies
    support for files larger than 4 gigabytes, and manual testing has
    verified support for files larger than 10 gigabytes. Large file
    support is available for both 32-bit and 64-bit platforms as long
    as the compiler and underlying platforms support it.

  - Support for page selection (splitting and merging PDF files) has
    been added to the :command:`qpdf` command-line
    tool. See :ref:`page-selection`.

  - The :qpdf:ref:`--copy-encryption` option have been added to the
    :command:`qpdf` command-line tool for copying encryption
    parameters from another file.

  - New methods have been added to the ``QPDF`` object for adding and
    removing pages. See :ref:`adding-and-remove-pages`.

  - New methods have been added to the ``QPDF`` object for copying
    objects from other PDF files. See :ref:`foreign-objects`

  - A new method ``QPDFObjectHandle::parse`` has been added for
    constructing ``QPDFObjectHandle`` objects from a string
    description.

  - Methods have been added to ``QPDFWriter`` to allow writing to an
    already open stdio ``FILE*`` addition to writing to standard
    output or a named file. Methods have been added to ``QPDF`` to be
    able to process a file from an already open stdio ``FILE*``. This
    makes it possible to read and write PDF from secure temporary
    files that have been unlinked prior to being fully read or
    written.

  - The ``QPDF::emptyPDF`` can be used to allow creation of PDF files
    from scratch. The example
    :file:`examples/pdf-create.cc` illustrates how
    it can be used.

  - Several methods to take ``PointerHolder<Buffer>`` can now also
    accept ``std::string`` arguments.

  - Many new convenience methods have been added to the library, most
    in ``QPDFObjectHandle``. See :file:`ChangeLog`
    for a full list.

  - When building on a platform that supports ELF shared libraries
    (such as Linux), symbol versions are enabled by default. They can
    be disabled by passing
    :samp:`--disable-ld-version-script` to
    :command:`./configure`.

  - The file :file:`libqpdf.pc` is now installed
    to support :command:`pkg-config`.

  - Image comparison tests are off by default now since they are not
    needed to verify a correct build or port of qpdf. They are needed
    only when changing the actual PDF output generated by qpdf. You
    should enable them if you are making deep changes to qpdf itself.
    See :file:`README.md` for details.

  - Large file tests are off by default but can be turned on with
    :command:`./configure` or by setting an environment
    variable before running the test suite. See
    :file:`README.md` for details.

  - When qpdf's test suite fails, failures are not printed to the
    terminal anymore by default. Instead, find them in
    :file:`build/qtest.log`. For packagers who are
    building with an autobuilder, you can add the
    :samp:`--enable-show-failed-test-output` option to
    :command:`./configure` to restore the old behavior.

2.3.1: December 28, 2011
  - Fix thread-safety problem resulting from non-thread-safe use of
    the PCRE library.

  - Made a few minor documentation fixes.

  - Add workaround for a bug that appears in some versions of
    ghostscript to the test suite

  - Fix minor build issue for Visual C++ 2010.

2.3.0: August 11, 2011
  - Bug fix: when preserving existing encryption on encrypted files
    with cleartext metadata, older qpdf versions would generate
    password-protected files with no valid password. This operation
    now works. This bug only affected files created by copying
    existing encryption parameters; explicit encryption with
    specification of cleartext metadata worked before and continues to
    work.

  - Enhance ``QPDFWriter`` with a new constructor that allows you to
    delay the specification of the output file. When using this
    constructor, you may now call ``QPDFWriter::setOutputFilename`` to
    specify the output file, or you may use
    ``QPDFWriter::setOutputMemory`` to cause ``QPDFWriter`` to write
    the resulting PDF file to a memory buffer. You may then use
    ``QPDFWriter::getBuffer`` to retrieve the memory buffer.

  - Add new API call ``QPDF::replaceObject`` for replacing objects by
    object ID

  - Add new API call ``QPDF::swapObjects`` for swapping two objects by
    object ID

  - Add ``QPDFObjectHandle::getDictAsMap`` and
    ``QPDFObjectHandle::getArrayAsVector`` to allow retrieval of
    dictionary objects as maps and array objects as vectors.

  - Add functions ``qpdf_get_info_key`` and ``qpdf_set_info_key`` to
    the C API for manipulating string fields of the document's
    ``/Info`` dictionary.

  - Add functions ``qpdf_init_write_memory``,
    ``qpdf_get_buffer_length``, and ``qpdf_get_buffer`` to the C API
    for writing PDF files to a memory buffer instead of a file.

2.2.4: June 25, 2011
  - Fix installation and compilation issues; no functionality changes.

2.2.3: April 30, 2011
  - Handle some damaged streams with incorrect characters following
    the stream keyword.

  - Improve handling of inline images when normalizing content
    streams.

  - Enhance error recovery to properly handle files that use object 0
    as a regular object, which is specifically disallowed by the spec.

2.2.2: October 4, 2010
  - Add new function ``qpdf_read_memory`` to the C API to call
    ``QPDF::processMemoryFile``. This was an omission in qpdf 2.2.1.

2.2.1: October 1, 2010
  - Add new method ``QPDF::setOutputStreams`` to replace ``std::cout``
    and ``std::cerr`` with other streams for generation of diagnostic
    messages and error messages. This can be useful for GUIs or other
    applications that want to capture any output generated by the
    library to present to the user in some other way. Note that QPDF
    does not write to ``std::cout`` (or the specified output stream)
    except where explicitly mentioned in
    :file:`QPDF.hh`, and that the only use of the
    error stream is for warnings. Note also that output of warnings is
    suppressed when ``setSuppressWarnings(true)`` is called.

  - Add new method ``QPDF::processMemoryFile`` for operating on PDF
    files that are loaded into memory rather than in a file on disk.

  - Give a warning but otherwise ignore empty PDF objects by treating
    them as null. Empty object are not permitted by the PDF
    specification but have been known to appear in some actual PDF
    files.

  - Handle inline image filter abbreviations when the appear as stream
    filter abbreviations. The PDF specification does not allow use of
    stream filter abbreviations in this way, but Adobe Reader and some
    other PDF readers accept them since they sometimes appear
    incorrectly in actual PDF files.

  - Implement miscellaneous enhancements to ``PointerHolder`` and
    ``Buffer`` to support other changes.

2.2.0: August 14, 2010
  - Add new methods to ``QPDFObjectHandle`` (``newStream`` and
    ``replaceStreamData`` for creating new streams and replacing
    stream data. This makes it possible to perform a wide range of
    operations that were not previously possible.

  - Add new helper method in ``QPDFObjectHandle``
    (``addPageContents``) for appending or prepending new content
    streams to a page. This method makes it possible to manipulate
    content streams without having to be concerned whether a page's
    contents are a single stream or an array of streams.

  - Add new method in ``QPDFObjectHandle``: ``replaceOrRemoveKey``,
    which replaces a dictionary key with a given value unless the
    value is null, in which case it removes the key instead.

  - Add new method in ``QPDFObjectHandle``: ``getRawStreamData``,
    which returns the raw (unfiltered) stream data into a buffer. This
    complements the ``getStreamData`` method, which returns the
    filtered (uncompressed) stream data and can only be used when the
    stream's data is filterable.

  - Provide two new examples:
    :command:`pdf-double-page-size` and
    :command:`pdf-invert-images` that illustrate the
    newly added interfaces.

  - Fix a memory leak that would cause loss of a few bytes for every
    object involved in a cycle of object references. Thanks to Jian Ma
    for calling my attention to the leak.

2.1.5: April 25, 2010
  - Remove restriction of file identifier strings to 16 bytes. This
    unnecessary restriction was preventing qpdf from being able to
    encrypt or decrypt files with identifier strings that were not
    exactly 16 bytes long. The specification imposes no such
    restriction.

2.1.4: April 18, 2010
  - Apply the same padding calculation fix from version 2.1.2 to the
    main cross reference stream as well.

  - Since :command:`qpdf --check` only performs limited
    checks, clarify the output to make it clear that there still may
    be errors that qpdf can't check. This should make it less
    surprising to people when another PDF reader is unable to read a
    file that qpdf thinks is okay.

2.1.3: March 27, 2010
  - Fix bug that could cause a failure when rewriting PDF files that
    contain object streams with unreferenced objects that in turn
    reference indirect scalars.

  - Don't complain about (invalid) AES streams that aren't a multiple
    of 16 bytes. Instead, pad them before decrypting.

2.1.2: January 24, 2010
  - Fix bug in padding around first half cross reference stream in
    linearized files. The bug could cause an assertion failure when
    linearizing certain unlucky files.

2.1.1: December 14, 2009
  - No changes in functionality; insert missing include in an internal
    library header file to support gcc 4.4, and update test suite to
    ignore broken Adobe Reader installations.

2.1: October 30, 2009
  - This is the first version of qpdf to include Windows support. On
    Windows, it is possible to build a DLL. Additionally, a partial
    C-language API has been introduced, which makes it possible to
    call qpdf functions from non-C++ environments. I am very grateful
    to Žarko Gajić (http://zarko-gajic.iz.hr/) for tirelessly testing
    numerous pre-release versions of this DLL and providing many
    excellent suggestions on improving the interface.

    For programming to the C interface, please see the header file
    :file:`qpdf/qpdf-c.h` and the example
    :file:`examples/pdf-linearize.c`.

  - Žarko Gajić has written a Delphi wrapper for qpdf, which can be
    downloaded from qpdf's download side. Žarko's Delphi wrapper is
    released with the same licensing terms as qpdf itself and comes
    with this disclaimer: "Delphi wrapper unit
    :file:`qpdf.pas` created by Žarko Gajić
    (http://zarko-gajic.iz.hr/). Use at your own risk and for whatever
    purpose you want. No support is provided. Sample code is
    provided."

  - Support has been added for AES encryption and crypt filters.
    Although qpdf does not presently support files that use PKI-based
    encryption, with the addition of AES and crypt filters, qpdf is
    now be able to open most encrypted files created with newer
    versions of Acrobat or other PDF creation software. Note that I
    have not been able to get very many files encrypted in this way,
    so it's possible there could still be some cases that qpdf can't
    handle. Please report them if you find them.

  - Many error messages have been improved to include more information
    in hopes of making qpdf a more useful tool for PDF experts to use
    in manually recovering damaged PDF files.

  - Attempt to avoid compressing metadata streams if possible. This is
    consistent with other PDF creation applications.

  - Provide new command-line options for AES encrypt, cleartext
    metadata, and setting the minimum and forced PDF versions of
    output files.

  - Add additional methods to the ``QPDF`` object for querying the
    document's permissions. Although qpdf does not enforce these
    permissions, it does make them available so that applications that
    use qpdf can enforce permissions.

  - The :qpdf:ref:`--check` option to
    :command:`qpdf` has been extended to include some
    additional information.

  - *Non-compatible API changes:*

    - QPDF's exception handling mechanism now uses
      ``std::logic_error`` for internal errors and
      ``std::runtime_error`` for runtime errors in favor of the now
      removed ``QEXC`` classes used in previous versions. The ``QEXC``
      exception classes predated the addition of the
      :file:`<stdexcept>` header file to the C++ standard library.
      Most of the exceptions thrown by the qpdf library itself are
      still of type ``QPDFExc`` which is now derived from
      ``std::runtime_error``. Programs that catch an instance of
      ``std::exception`` and displayed it by calling the ``what()``
      method will not need to be changed.

    - The ``QPDFExc`` class now internally represents various fields
      of the error condition and provides interfaces for querying
      them. Among the fields is a numeric error code that can help
      applications act differently on (a small number of) different
      error conditions. See :file:`QPDFExc.hh` for details.

    - Warnings can be retrieved from qpdf as instances of ``QPDFExc``
      instead of strings.

    - The nested ``QPDF::EncryptionData`` class's constructor takes an
      additional argument. This class is primarily intended to be used
      by ``QPDFWriter``. There's not really anything useful an
      end-user application could do with it. It probably shouldn't
      really be part of the public interface to begin with. Likewise,
      some of the methods for computing internal encryption dictionary
      parameters have changed to support ``/R=4`` encryption.

    - The method ``QPDF::getUserPassword`` has been removed since it
      didn't do what people would think it did. There are now two new
      methods: ``QPDF::getPaddedUserPassword`` and
      ``QPDF::getTrimmedUserPassword``. The first one does what the
      old ``QPDF::getUserPassword`` method used to do, which is to
      return the password with possible binary padding as specified by
      the PDF specification. The second one returns a human-readable
      password string.

    - The enumerated types that used to be nested in ``QPDFWriter``
      have moved to top-level enumerated types and are now defined in
      the file :file:`qpdf/Constants.h`. This enables them to be
      shared by both the C and C++ interfaces.

2.0.6: May 3, 2009
  - Do not attempt to uncompress streams that have decode parameters
    we don't recognize. Earlier versions of qpdf would have rejected
    files with such streams.

2.0.5: March 10, 2009
  - Improve error handling in the LZW decoder, and fix a small error
    introduced in the previous version with regard to handling full
    tables. The LZW decoder has been more strongly verified in this
    release.

2.0.4: February 21, 2009
  - Include proper support for LZW streams encoded without the "early
    code change" flag. Special thanks to Atom Smasher who reported the
    problem and provided an input file compressed in this way, which I
    did not previously have.

  - Implement some improvements to file recovery logic.

2.0.3: February 15, 2009
  - Compile cleanly with gcc 4.4.

  - Handle strings encoded as UTF-16BE properly.

2.0.2: June 30, 2008
  - Update test suite to work properly with a
    non-:command:`bash`
    :file:`/bin/sh` and with Perl 5.10. No changes
    were made to the actual qpdf source code itself for this release.

2.0.1: May 6, 2008
  - No changes in functionality or interface. This release includes
    fixes to the source code so that qpdf compiles properly and passes
    its test suite on a broader range of platforms. See
    :file:`ChangeLog` in the source distribution
    for details.

2.0: April 29, 2008
  - First public release.
