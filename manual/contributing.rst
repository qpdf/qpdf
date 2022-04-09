.. _contributing:

Contributing to qpdf
====================

.. _source-repository:

Source Repository
-----------------

The qpdf source code lives at https://github.com/qpdf/qpdf.

Create issues (bug reports, feature requests) at
https://github.com/qpdf/qpdf/issues. If you have a general question or
topic for discussion, you can create a discussion at
https://github.com/qpdf/qpdf/discussions.

.. _code-formatting:

Code Formatting
---------------

The qpdf source code is formatting using clang-format ≥ version 15
with a :file:`.clang-format` file at the top of the source tree. The
:file:`format-code` script reformats all the source code in the
repository. You must have ``clang-format`` in your path, and it must be
at least version 15.

For emacs users, the :file:`.dir-locals.el` file configures emacs
``cc-mode`` for an indentation style that is similar to but not
exactly like what ``clang-format`` produces. When there are
differences, ``clang-format`` is authoritative. It is not possible to
make ``cc-mode`` and ``clang-format`` exactly match since the syntax
parser in emacs is not as sophisticated.

Blocks of code that should not be formatted can be surrounded by the
comments ``// clang-format off`` and ``// clang-format on``. Sometimes
clang-format tries to combine lines in ways that are undesirable. In
this case, we follow a convention of adding a comment ``//
line-break`` on its own line.

For exact details, consult :file:`.clang-format`. Here is a broad,
partial summary of the formatting rules:

- Use spaces, not tabs.

- Keep lines to 80 columns when possible.

- Braces are on their own lines after classes and functions (and
  similar top-level constructs) and are compact otherwise.

- Closing parentheses are attached to the previous material, not not
  their own lines.

The :file:`README-maintainer` file has a few additional notes that are
probably not important to anyone who is not making deep changes to
qpdf.

.. _automated-testing:

Automated Tests
---------------

The testing style of qpdf has evolved over time. More recent tests
call ``assert()``. Older tests print stuff to standard output and
compare the output against reference files. Many tests are a mixture
of these techniques.

The `qtest <https://qtest.sourceforge.io>`__ style of testing is to
test everything through the application. So effectively most testing
is "integration testing" or "end-to-end testing".

For details about ``qtest``, consult the `QTest Manual
<https://qtest.sourceforge.io/doc/qtest-manual.html>`__. As you read
it, keep in mind that, in spite of the recent date on the file, the
vast majority of that documentation is from before 2007 and predates
many test frameworks and approaches that are in use today.

Notes on testing:

- In most cases, things in the code are tested through integration
  tests, though the test suite is very thorough. Many tests are driven
  through the ``qpdf`` CLI. Others are driven through other files in
  the ``qpdf`` directory, especially ``test_driver.cc`` and
  ``qpdf-ctest.c``. These programs only use the public API.

- In some cases, there are true "unit tests", but they are exercised
  through various stand-alone programs that exercise the library in
  particular ways, including some that have access to library
  internals. These are in the ``libtests`` directory.

Coverage
~~~~~~~~

You wil see calls to ``QTC::TC`` throughout the code. This is a
"manual coverage" system described in depth in the qtest documentation
linked above. It works by ensuring that ``QTC::TC`` is called sometime
during the test in each configured way. In brief:

- ``QTC::TC`` takes two mandatory options and an optional one:

  - The first two arguments must be *string literals*. This is because
    ``qtest`` finds coverage cases lexically.

  - The first argument is the scope name, usually ``qpdf``. This means
    there is a ``qpdf.testcov`` file in the source directory.

  - The second argument is a case name. Each case name appears in
    ``qpdf.testcov`` with a number after it, usually ``0``.

  - If the third argument is present, it is a number. ``qtest``
    ensures that the ``QTC::TC`` is called for that scope and case at
    least once with the third argument set to every value from ``0``
    to ``n`` inclusive, where ``n`` is the number after the coverage
    call.

- ``QTC::TC`` does nothing unless certain environment variables are
  set. Therefore, ``QTC:TC`` calls should have no side effects. (In
  some languages, they may be disabled at compile-time, though qpdf
  does not actually do this.)

So, for example, if you have this code:

.. code-block:: C++

   QTC::TC("qpdf", "QPDF eof skipping spaces before xref",
	   skipped_space ? 0 : 1);


and this line in `qpdf.testcov`:

::

   QPDF eof skipping spaces before xref 1

the test suite will only pass if that line of code was called at least
once with ``skipped_space == 0`` and at least once with ``skipped_space
== 1``.

The manual coverage approach ensures the reader that certain
conditions were covered in testing. Use of ``QTC::TC`` is only part of
the overall strategy.

I do not require testing on pull requests, but they are appreciated,
and I will not merge any code that is not tested. Often someone will
submit a pull request that is not adequately tested but is a good
contribution. In those cases, I will often take the code, add it with
tests, and accept the changes that way rather than merging the pull
request as submitted.

Personal Comments
-----------------

QPDF started as a work project in 2002. The first open source release
was in 2008. While there have been a handful of contributors, the vast
majority of the code was written by one person over many years as a
side project.

I maintain a very strong commitment to backward compatibility. As
such, there are many aspects of the code that are showing their age.
While I believe the codebase to have high quality, there are things
that I would do differently if I were doing them from scratch today.
Sometimes people will suggest changes that I like but can't accept for
backward compatibility reasons.

While I welcome contributions and am eager to collaborate with
contributors, I have a high bar. I only accept things I'm willing to
maintain over the long haul, and I am happy to help people get
submissions into that state.
