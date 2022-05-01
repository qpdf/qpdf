.. _weak-crypto:

Weak Cryptography
=================

For help with compiler errors in qpdf 11.0 or newer, see
:ref:`breaking-crypto-api`.

Since 2006, the PDF specification has offered ways to create encrypted
PDF files without using weak cryptography, though it took a few years
for many PDF readers and writers to catch up. It is still necessary to
support weak encryption algorithms to read encrypted PDF files that
were created using weak encryption algorithms, including all PDF files
created before the modern formats were introduced or widely supported.

Starting with version 10.4, qpdf began taking steps to reduce the
likelihood of a user *accidentally* creating PDF files with insecure
cryptography but will continue to allow creation of such files
indefinitely with explicit acknowledgment. The restrictions on use of
weak cryptography were made stricter with qpdf 11.

Definition of Weak Cryptographic Algorithm
------------------------------------------

We divide weak cryptographic algorithms into two categories: weak
encryption and weak hashing. Encryption is encoding data such that a
key of some sort is required to decode it. Hashing is creating a short
value from data in such a way that it is extremely improbable to find
two documents with the same hash (known has a hash collision) and
extremely difficult to intentionally create a document with a specific
hash or two documents with the same hash.

When we say that an encryption algorithm is weak, we either mean that
a mathematical flaw has been discovered that makes it inherently
insecure or that it is sufficiently simple that modern computer
technology makes it possible to use "brute force" to crack. For
example, when 40-bit keys were originally introduced, it wasn't
practical to consider trying all possible keys, but today such a thing
is possible.

When we say that a hashing algorithm is weak, we mean that, either
because of mathematical flaw or insufficient complexity, it is
computationally feasible to intentionally construct a hash collision.

While weak encryption should always be avoided, there are cases in
which it is safe to use a weak hashing algorithm when security is not
a factor. For example, a weak hashing algorithm should not be used as
the only mechanism to test whether a file has been tampered with. In
other words, you can't use a weak hash as a digital signature. There
is no harm, however, in using a weak hash as a way to sort or index
documents as long as hash collisions are tolerated. It is also common
to use weak hashes as checksums, which are often used a check that a
file wasn't damaged in transit or storage, though for true integrity,
a strong hash would be better.

Note that qpdf must always retain support for weak cryptographic
algorithms since this is required for reading older PDF files that use
it. Additionally, qpdf will always retain the ability to create files
using weak cryptographic algorithms since, as a development tool, qpdf
explicitly supports creating older or deprecated types of PDF files
since these are sometimes needed to test or work with older versions of
software. Even if other cryptography libraries drop support for RC4 or
MD5, qpdf can always fall back to its internal implementations of those
algorithms, so they are not going to disappear from qpdf.

Uses of Weak Encryption in qpdf
---------------------------------

When PDF files are encrypted using 40-bit encryption or 128-bit
encryption without AES, then the weak *RC4* algorithm is used. You can
avoid using weak encryption in qpdf by always using 256-bit
encryption. Unless you are trying to create files that need to be
opened with PDF readers from before about 2010 (by which time most
readers had added support for the stronger encryption algorithms) or
are creating insecure files explicitly for testing or some similar
purpose, there is no reason to use anything other than 256-bit
encryption.

By default, qpdf refuses to write a file that uses weak encryption.
You can explicitly allow this by specifying the
:qpdf:ref:`--allow-weak-crypto` option.

In qpdf 11, all library methods that could potentially cause files to
be written with weak encryption were deprecated, and methods to enable
weak encryption were either given explicit names indicating this or
take required arguments to enable the insecure behavior.

There is one exception: when encryption parameters are copied from the
input file or another file to the output file, there is no prohibition
or even warning against using insecure encryption. The reason is that
many qpdf operations simply preserve whatever encryption is there, and
requiring confirmation to *preserve* insecure encryption would cause
qpdf to break when non-encryption-related operations were performed on
files that happened to be encrypted. Failing or generating warnings in
this case would likely have the effect of making people use the
:qpdf:ref:`--allow-weak-crypto` option blindly, which would be worse
than just letting those files go so that explicit, conscious selection
of weak crypto would be more likely to be noticed. Why, you might ask,
does this apply to :qpdf:ref:`--copy-encryption` as well as to the
default behavior preserving encryption? The answer is that
:qpdf:ref:`--copy-encryption` works with an unencrypted file as input,
which enables workflows where one may start with a file, decrypt it
*just in case*, perform a series of operations, and then reapply the
original encryption, *if any*. Also, one may have a template used for
encryption that one may apply to a variety of output files, and it
would be annoying to be warned about it for every output file.

Uses of Weak Hashing In QPDF
----------------------------

The PDF specification makes use the weak *MD5* hashing algorithm in
several places. While it is used in the encryption algorithms,
breaking MD5 would not be adequate to crack an encrypted file when
256-bit encryption is in use, so using 256-bit encryption is adequate
for avoiding the use of MD5 for anything security-sensitive.

MD5 is used in the following non-security-sensitive ways:

- Generation of the document ID. The document ID is an input parameter
  to the document encryption but is not itself considered to be
  secure. They are supposed to be unique, but they are not
  tamper-resistent in non-encrypted PDF files, and hash collisions
  must be tolerated.

  The PDF specification recommends but does not require the use of MD5
  in generation of document IDs. Usually there is also a random
  component to document ID generation. There is a qpdf-specific
  feature of generating a *deterministic ID* (see
  :qpdf:ref:`--deterministic-id`) which also uses MD5. While it would
  certainly be possible to change the deterministic ID algorithm to
  not use MD5, doing so would break all previous deterministic IDs
  (which would render the feature useless for many cases) and would
  offer very little benefit since even a securely generated document
  ID is not itself a security-sensitive value.

- Checksums in embedded file streams -- the PDF specification
  specifies the use of MD5.

It is therefore not possible completely avoid the use of MD5 with
qpdf, but as long as you are using 256-bit encryption, it is not used
in a security-sensitive fashion.

.. _breaking-crypto-api:

API-Breaking Changes in qpdf 11.0
---------------------------------

In qpdf 11, several deprecated functions and methods were removed.
These methods provided an incomplete API. Alternatives were added in
qpdf 8.4.0. The removed functions are

- C API: ``qpdf_set_r3_encryption_parameters``,
  ``qpdf_set_r4_encryption_parameters``,
  ``qpdf_set_r5_encryption_parameters``,
  ``qpdf_set_r6_encryption_parameters``

- ``QPDFWriter``: overloaded versions of these methods with fewer
  arguments: ``setR3EncryptionParameters``,
  ``setR4EncryptionParameters``, ``setR5EncryptionParameters``, and
  ``setR6EncryptionParameters``

Additionally, remaining functions/methods had their names changed to
signal that they are insecure and to force developers to make a
decision. If you intentionally want to continue to use insecure
cryptographic algorithms and create insecure files, you can change
your code just add ``_insecure`` or ``Insecure`` to the end of the
function as needed. (Note the disappearance of ``2`` in some of the C
functions as well.) Better, you should migrate your code to use more
secure encryption as documented in :file:`QPDFWriter.hh`. Use the
``R6`` methods (or their corresponding C functions) to create files
with 256-bit encryption.

.. list-table:: Renamed Functions
   :widths: 50 50
   :header-rows: 1

   - - Old Name
     - New Name

   - - qpdf_set_r2_encryption_parameters
     - qpdf_set_r2_encryption_parameters_insecure

   - - qpdf_set_r3_encryption_parameters2
     - qpdf_set_r3_encryption_parameters_insecure

   - - qpdf_set_r4_encryption_parameters2
     - qpdf_set_r2_encryption_parameters_insecure

   - - QPDFWriter::setR2EncryptionParameters
     - QPDFWriter::setR2EncryptionParametersInsecure

   - - QPDFWriter::setR3EncryptionParameters
     - QPDFWriter::setR3EncryptionParametersInsecure

   - - QPDFWriter::setR4EncryptionParameters
     - QPDFWriter::setR4EncryptionParametersInsecure
