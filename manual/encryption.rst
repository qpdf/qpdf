.. _pdf-encryption:

PDF Encryption
==============

This chapter discusses PDF encryption in a general way with an angle
toward how it works in :command:`qpdf`. This chapter is not intended
to replace the PDF specification. Please consult the spec for full
details.

PDF Encryption Concepts
-----------------------

Encryption
  Encryption is the replacement of *clear text* with encrypted text,
  also known as *ciphertext*. The clear text may be retrieved from the
  ciphertext if the encryption key is known.

  PDF files consist of an object structure. PDF objects may be of a
  variety of types including (among others) numbers, boolean values,
  names, arrays, dictionaries, strings, and streams. In a PDF file,
  only strings and streams are encrypted.

Security Handler
  Since the inception of PDF, there have been several modifications to
  the way files are encrypted. Encryption is handled by a *security
  handler*. The *standard security handler* is password-based. This is
  the only security handler implemented by qpdf, and this material is
  all focused on the standard security handler. There are various
  flags that control the specific details of encryption with the
  standard security handler. These are discussed below.

Encryption Key
  This refers to the actual key used by the encryption and decryption
  algorithms. It is distinct from the password. The main encryption
  key is generated at random and stored encrypted in the PDF file. The
  passwords used to protect a PDF file, if any, are used to protect
  the encryption key. This design makes it possible to use different
  passwords (e.g., user and owner passwords) to retrieve the
  encryption key or even to change the password on a file without
  changing the encryption key. qpdf can expose the encryption key when
  run with the :qpdf:ref:`--show-encryption-key` option and can accept
  a hex-encoded encryption key in place of a password when run with
  the :qpdf:ref:`--password-is-hex-key` option.

Password Protection
  Password protection is distinct from encryption. This point is often
  misunderstood. A PDF file can be encrypted without being
  password-protected. The intent of PDF encryption was that there
  would be two passwords: a *user password* and an *owner password*.
  Either password can be used to retrieve the encryption key. A
  conforming reader is supposed to obey the security restrictions
  if the file is opened using the user password but not if the file is
  opened with the owner password. :command:`qpdf` makes no distinction
  between which password is used to open the file. The distinction
  made by conforming readers between the user and owner password is
  what makes it common to create encrypted files with no password
  protection. This is done by using the empty string as the user
  password and some secret string as the owner password. When a user
  opens the PDF file, the empty string is used to retrieve the
  encryption key, making the file usable, but a conforming reader
  restricts certain operations from the user.

What does all this mean? Here are a few things to realize.

- Since the user password and the owner password are both used to
  recover the single encryption key, there is *fundamentally no way*
  to prevent an application from disregarding the security
  restrictions on a file. Any software that can read the encrypted
  file at all has the encryption key. Therefore, the security of the
  restrictions placed on PDF files is solely enforced by the software.
  Any open source PDF reader could be trivially modified to ignore the
  security restrictions on a file. The PDF specification is clear
  about this point. This means that PDF restrictions on
  non-password-protected files only restrict users who don't know how
  to circumvent them.

- If a file is password-protected, you have to know at least one of
  the user or owner password to retrieve the encryption key. However,
  in the case of 40-bit encryption, the actual encryption key is only
  5 bytes long and can be easily brute-forced. As such, files
  encrypted with 40-bit encryption are not secure regardless of how
  strong the password is. With 128-bit encryption, the default
  security handler uses RC4 encryption, which is also known to be
  insecure. As such, the only way to securely encrypt a PDF file using
  the standard security handler (as of the last review of this chapter
  in 2022) is to use AES encryption. This is the only supported
  algorithm with 256-bit encryption, and it can be selected to be used
  with 128-bit encryption as well. However there is no reason to use
  128-bit encryption with AES. If you are going to use AES, just use
  256-bit encryption instead. The security of a 256-bit AES-encrypted
  PDF file with a strong password is comparable to using a
  general-purpose encryption tool like :command:`gpg` or
  :command:`openssl` to encrypt the PDF file with the same password,
  but the advantage of using PDF encryption is that no software is
  required beyond a regular PDF viewer.

PDF Encryption Details
----------------------

This section describes a few details about PDF encryption. It does not
describe all the details. For that, read the PDF specification. The
details presented here, however, should go a long way toward helping a
casual user/developer understand what's going on with encrypted PDF
files.

Here are more concepts to understand.

Algorithm parameters ``V`` and ``R``
  There are two parameters that control the details of encryption
  using the standard security handler: ``V`` and ``R``.

  ``V`` is a code specifying the algorithms that are used for
  encrypting the file, handling keys, etc. It may have any of the
  following values:

  .. list-table:: Encryption Algorithms: ``V``
     :widths: 10 80
     :header-rows: 1

     - - V
       - Meaning

     - - 1
       - The original algorithm, which encrypted files using 40-bit keys.

     - - 2
       - An extension of the original algorithm allowing longer keys.
         Introduced in PDF 1.4.

     - - 3
       - An unpublished algorithm that permits file encryption key
         lengths ranging from 40 to 128 bits. Introduced in PDF 1.4.
         qpdf is believed to be able to read files with ``V`` = 3 but
         does not write such files.

     - - 4
       - An extension of the algorithm that allows it to be
         parameterized by additional rules for handling strings and
         streams. Introduced in PDF 1.5.

     - - 5
       - An algorithm that allows specification of separate security
         handlers for strings and streams as well as embedded files,
         and which supports 256-bit keys. Introduced in PDF 1.7
         extension level 3 and later extended in extension level 8.
         This is the encryption system in the PDF 2.0 specification,
         ISO-32000.

  ``R`` is a code specifying the revision of the standard handler. It
  is tightly coupled with the value of ``V``. ``R`` may have any of
  the following values:

  .. list-table:: Relationship between ``R`` and ``V``
     :widths: 10 80
     :header-rows: 1

     - - R
       - Expected V

     - - 2
       - ``V`` must be 1

     - - 3
       - ``V`` must be 2 or 3

     - - 4
       - ``V`` must be 4

     - - 5
       - ``V`` must be 5; this extension was never fully specified and
         existed for a short time in some versions of Acrobat.
         :command:`qpdf` is able to read and write this format, but it
         should not be used for any purpose other than testing
         compatibility with the format.

     - - 6
       - ``V`` must be 5. This is the only value that is not
         deprecated in the PDF 2.0 specification, ISO-32000.

Encryption Dictionary
  Encrypted PDF files have an encryption dictionary. There are several
  fields, but these are the important ones for our purposes:

  - ``V`` and ``R`` as described above

  - ``O``, ``U``, ``OE``, ``UE``: values used by the algorithms that
    recover the encryption key from the user and owner password. Which
    of these are defined and how they are used vary based on the value
    of ``R``.

  - ``P``: a bit field that describes which restrictions are in place.
    This is discussed below in :ref:`security-restrictions`

Encryption Algorithms
  PDF files may be encrypted with the obsolete, insecure RC4 algorithm
  or the more secure AES algorithm. See also :ref:`weak-crypto` for a
  discussion. 40-bit encryption always uses RC4. 128-bit can use
  either RC4 (the default for compatibility reasons) or, starting with
  PDF 1.6, AES. 256-bit encryption always uses AES.

.. _security-restrictions:

PDF Security Restrictions
-------------------------

PDF security restrictions are described by a bit field whose value is
stored in the ``P`` field in the encryption dictionary. The value of
``P`` is used by the algorithms to recover the encryption key given
the password, which makes the value of ``P`` tamper-resistent.

``P`` is a 32-bit integer, treated as a signed twos-complement number.
A 1 in any bit position means the permission is granted. The PDF
specification numbers the bits from 1 (least significant bit) to 32
(most significant bit) rather than the more customary 0 to 31. For
consistency with the spec, the remainder of this section uses the
1-based numbering.

Only bits 3, 4, 5, 6, 9, 10, 11, and 12 are used. All other bits are
set to 1. Since bit 32 is always set to 1, the value of ``P`` is
always a negative number. (:command:`qpdf` recognizes a positive
number on behalf of buggy writers that treat ``P`` as unsigned. Such
files have been seen in the wild.)

Here are the meanings of the bit positions. All bits not listed must
have the value 1 except bits 1 and 2, which must have the value 0.
However, the values of bits other than those in the table are ignored,
so having incorrect values probably doesn't break anything in most
cases. A value of 1 indicates that the permission is granted.

.. list-table:: ``P`` Bit Values
   :widths: 10 80
   :header-rows: 1

   - - Bit
     - Meaning

   - - 3
     - for ``R`` = 2 printing; for ``R`` ≥ 3, printing at low
       resolution

   - - 4
     - modifying the document except as controlled by bits 6,
       9, and 11

   - - 5
     - extracting text and graphics for purposes other than
       accessibility to visually impaired users

   - - 6
     - add or modify annotations, fill in interactive form fields;
       if bit 4 is also set, create or modify interactive form fields

   - - 9
     - for ``R`` ≥ 3, fill in interactive form fields even if bit 6 is
       clear

   - - 10
     - not used; formerly granted permission to extract material for
       accessibility, but the specification now disallows restriction of
       accessibility, and conforming readers are to treat this bit as if
       it is set regardless of its value

   - - 11
     - for ``R`` ≥ 3, assemble document including inserting, rotating,
       or deleting pages or creating document outlines or thumbnail
       images

   - - 12
     - for ``R`` ≥ 3, allow printing at full resolution

.. _qpdf-P:

How qpdf handles security restrictions
--------------------------------------

The section describes exactly what the qpdf library does with regard
to ``P`` based on the various settings of different security options.

- Start with all bits set except bits 1 and 2, which are cleared

- Clear bits and described in the table below:

  .. list-table:: Command-line Arguments and ``P`` Bit Values
     :widths: 20 25 45
     :header-rows: 1

     - - R
       - Argument
       - Bits Cleared

     - - R = 2
       - ``--print=n``
       - 3

     - - R = 2
       - ``--modify=n``
       - 4

     - - R = 2
       - ``--extract=n``
       - 5

     - - R = 2
       - ``--annotate=n``
       - 6

     - - R = 3
       - ``--accessibility=n``
       - 10

     - - R ≥ 4
       - ``--accessibility=n``
       - ignored

     - - R ≥ 3
       - ``--extract=n``
       - 5

     - - R ≥ 3
       - ``--print=none``
       - 3, 12

     - - R ≥ 3
       - ``--print=low``
       - 12

     - - R ≥ 3
       - ``--modify=none``
       - 4, 6, 9, 11

     - - R ≥ 3
       - ``--modify=assembly``
       - 4, 6, 9

     - - R ≥ 3
       - ``--modify=form``
       - 4, 6

     - - R ≥ 3
       - ``--modify=annotate``
       - 4

     - - R ≥ 3
       - ``--assemble=n``
       - 11

     - - R ≥ 3
       - ``--annotate=n``
       - 6

     - - R ≥ 3
       - ``--form=n``
       - 9

     - - R ≥ 3
       - ``--modify-other=n``
       - 4

Options to :command:`qpdf`, both at the CLI and library level, allow
more granular clearing of permission bits than do most tools,
including Adobe Acrobat. As such, PDF viewers may respond in
surprising ways based on options passed to qpdf. If you observe this,
it is probably not because of a bug in qpdf.

.. _pdf-passwords:

User and Owner Passwords
------------------------

When you use qpdf to show encryption parameters and you open a file
with the owner password, sometimes qpdf reveals the user password, and
sometimes it doesn't. Here's why.

For ``V`` < 5, the user password is actually stored in the PDF file
encrypted with a key that is derived from the owner password, and the
main encryption key is encrypted using a key derived from the user
password. When you open a PDF file, the reader first tries to treat
the given password as the user password, using it to recover the
encryption key. If that works, you're in with restrictions (assuming
the reader chooses to enforce them). If it doesn't work, then the
reader treats the password as the owner password, using it to recover
the user password, and then uses the user password to retrieve the
encryption key. This is why creating a file with the same user
password and owner password with ``V`` < 5 results in a file that some
readers will never allow you to open as the owner. When an empty owner
password is given at file creation, the user password is used as both
the user and owner password. Typically when a reader encounters a file
with ``V`` < 5, it will first attempt to treat the empty string as a
user password. If that works, the file is encrypted but not
password-protected. If it doesn't work, then a password prompt is
given.

For ``V`` ≥ 5, the main encryption key is independently encrypted
using the user password and the owner password. There is no way to
recover the user password from the owner password. Restrictions are
imposed or not depending on which password was used. In this case, the
password supplied, if any, is tried both as the user password and the
owner password, and whichever works is used. Typically the password is
tried as the owner password first. (This is what the PDF specification
says to do.) As such, specifying a user password and leaving the owner
password blank results in a file that is opened as owner with no
password, effectively rendering the security restrictions useless.
This is why :command:`qpdf` requires you to pass
:qpdf:ref:`--allow-insecure` to create a file with an empty owner
password when 256-bit encryption is in use.
