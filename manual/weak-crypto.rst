.. _weak-crypto:

Weak Cryptography
=================

Start with version 10.4, qpdf is taking steps to reduce the likelihood
of a user *accidentally* creating PDF files with insecure cryptography
but will continue to allow creation of such files indefinitely with
explicit acknowledgment.

The PDF file format makes use of RC4, which is known to be a weak
cryptography algorithm, and MD5, which is a weak hashing algorithm. In
version 10.4, qpdf generates warnings for some (but not all) cases of
writing files with weak cryptography when invoked from the command-line.
These warnings can be suppressed using the
:qpdf:ref:`--allow-weak-crypto` option.

It is planned for qpdf version 11 to be stricter, making it an error to
write files with insecure cryptography from the command-line tool in
most cases without specifying the
:qpdf:ref:`--allow-weak-crypto` flag and also to require
explicit steps when using the C++ library to enable use of insecure
cryptography.

Note that qpdf must always retain support for weak cryptographic
algorithms since this is required for reading older PDF files that use
it. Additionally, qpdf will always retain the ability to create files
using weak cryptographic algorithms since, as a development tool, qpdf
explicitly supports creating older or deprecated types of PDF files
since these are sometimes needed to test or work with older versions of
software. Even if other cryptography libraries drop support for RC4 or
MD5, qpdf can always fall back to its internal implementations of those
algorithms, so they are not going to disappear from qpdf.
