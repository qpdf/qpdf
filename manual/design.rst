.. _design:

Design and Library Notes
========================

.. _design.intro:

Introduction
------------

This section was written prior to the implementation of the qpdf package
and was subsequently modified to reflect the implementation. In some
cases, for purposes of explanation, it may differ slightly from the
actual implementation. As always, the source code and test suite are
authoritative. Even if there are some errors, this document should serve
as a road map to understanding how this code works.

In general, one should adhere strictly to a specification when writing
but be liberal in reading. This way, the product of our software will be
accepted by the widest range of other programs, and we will accept the
widest range of input files. This library attempts to conform to that
philosophy whenever possible but also aims to provide strict checking
for people who want to validate PDF files. If you don't want to see
warnings and are trying to write something that is tolerant, you can
call ``setSuppressWarnings(true)``. If you want to fail on the first
error, you can call ``setAttemptRecovery(false)``. The default behavior
is to generating warnings for recoverable problems. Note that recovery
will not always produce the desired results even if it is able to get
through the file. Unlike most other PDF files that produce generic
warnings such as "This file is damaged,", qpdf generally issues a
detailed error message that would be most useful to a PDF developer.
This is by design as there seems to be a shortage of PDF validation
tools out there. This was, in fact, one of the major motivations behind
the initial creation of qpdf.

.. _design-goals:

Design Goals
------------

The QPDF package includes support for reading and rewriting PDF files.
It aims to hide from the user details involving object locations,
modified (appended) PDF files, the directness/indirectness of objects,
and stream filters including encryption. It does not aim to hide
knowledge of the object hierarchy or content stream contents. Put
another way, a user of the qpdf library is expected to have knowledge
about how PDF files work, but is not expected to have to keep track of
bookkeeping details such as file positions.

A user of the library never has to care whether an object is direct or
indirect, though it is possible to determine whether an object is direct
or not if this information is needed. All access to objects deals with
this transparently. All memory management details are also handled by
the library.

The ``PointerHolder`` object is used internally by the library to deal
with memory management. This is basically a smart pointer object very
similar in spirit to C++-11's ``std::shared_ptr`` object, but predating
it by several years. This library also makes use of a technique for
giving fine-grained access to methods in one class to other classes by
using public subclasses with friends and only private members that in
turn call private methods of the containing class. See
``QPDFObjectHandle::Factory`` as an example.

The top-level qpdf class is ``QPDF``. A ``QPDF`` object represents a PDF
file. The library provides methods for both accessing and mutating PDF
files.

The primary class for interacting with PDF objects is
``QPDFObjectHandle``. Instances of this class can be passed around by
value, copied, stored in containers, etc. with very low overhead.
Instances of ``QPDFObjectHandle`` created by reading from a file will
always contain a reference back to the ``QPDF`` object from which they
were created. A ``QPDFObjectHandle`` may be direct or indirect. If
indirect, the ``QPDFObject`` the ``PointerHolder`` initially points to
is a null pointer. In this case, the first attempt to access the
underlying ``QPDFObject`` will result in the ``QPDFObject`` being
resolved via a call to the referenced ``QPDF`` instance. This makes it
essentially impossible to make coding errors in which certain things
will work for some PDF files and not for others based on which objects
are direct and which objects are indirect.

Instances of ``QPDFObjectHandle`` can be directly created and modified
using static factory methods in the ``QPDFObjectHandle`` class. There
are factory methods for each type of object as well as a convenience
method ``QPDFObjectHandle::parse`` that creates an object from a string
representation of the object. Existing instances of ``QPDFObjectHandle``
can also be modified in several ways. See comments in
:file:`QPDFObjectHandle.hh` for details.

An instance of ``QPDF`` is constructed by using the class's default
constructor. If desired, the ``QPDF`` object may be configured with
various methods that change its default behavior. Then the
``QPDF::processFile()`` method is passed the name of a PDF file, which
permanently associates the file with that QPDF object. A password may
also be given for access to password-protected files. QPDF does not
enforce encryption parameters and will treat user and owner passwords
equivalently. Either password may be used to access an encrypted file.
``QPDF`` will allow recovery of a user password given an owner password.
The input PDF file must be seekable. (Output files written by
``QPDFWriter`` need not be seekable, even when creating linearized
files.) During construction, ``QPDF`` validates the PDF file's header,
and then reads the cross reference tables and trailer dictionaries. The
``QPDF`` class keeps only the first trailer dictionary though it does
read all of them so it can check the ``/Prev`` key. ``QPDF`` class users
may request the root object and the trailer dictionary specifically. The
cross reference table is kept private. Objects may then be requested by
number of by walking the object tree.

When a PDF file has a cross-reference stream instead of a
cross-reference table and trailer, requesting the document's trailer
dictionary returns the stream dictionary from the cross-reference stream
instead.

There are some convenience routines for very common operations such as
walking the page tree and returning a vector of all page objects. For
full details, please see the header files
:file:`QPDF.hh` and
:file:`QPDFObjectHandle.hh`. There are also some
additional helper classes that provide higher level API functions for
certain document constructions. These are discussed in :ref:`helper-classes`.

.. _helper-classes:

Helper Classes
--------------

QPDF version 8.1 introduced the concept of helper classes. Helper
classes are intended to contain higher level APIs that allow developers
to work with certain document constructs at an abstraction level above
that of ``QPDFObjectHandle`` while staying true to qpdf's philosophy of
not hiding document structure from the developer. As with qpdf in
general, the goal is to take away some of the more tedious bookkeeping
aspects of working with PDF files, not to remove the need for the
developer to understand how the PDF construction in question works. The
driving factor behind the creation of helper classes was to allow the
evolution of higher level interfaces in qpdf without polluting the
interfaces of the main top-level classes ``QPDF`` and
``QPDFObjectHandle``.

There are two kinds of helper classes: *document* helpers and *object*
helpers. Document helpers are constructed with a reference to a ``QPDF``
object and provide methods for working with structures that are at the
document level. Object helpers are constructed with an instance of a
``QPDFObjectHandle`` and provide methods for working with specific types
of objects.

Examples of document helpers include ``QPDFPageDocumentHelper``, which
contains methods for operating on the document's page trees, such as
enumerating all pages of a document and adding and removing pages; and
``QPDFAcroFormDocumentHelper``, which contains document-level methods
related to interactive forms, such as enumerating form fields and
creating mappings between form fields and annotations.

Examples of object helpers include ``QPDFPageObjectHelper`` for
performing operations on pages such as page rotation and some operations
on content streams, ``QPDFFormFieldObjectHelper`` for performing
operations related to interactive form fields, and
``QPDFAnnotationObjectHelper`` for working with annotations.

It is always possible to retrieve the underlying ``QPDF`` reference from
a document helper and the underlying ``QPDFObjectHandle`` reference from
an object helper. Helpers are designed to be helpers, not wrappers. The
intention is that, in general, it is safe to freely intermix operations
that use helpers with operations that use the underlying objects.
Document and object helpers do not attempt to provide a complete
interface for working with the things they are helping with, nor do they
attempt to encapsulate underlying structures. They just provide a few
methods to help with error-prone, repetitive, or complex tasks. In some
cases, a helper object may cache some information that is expensive to
gather. In such cases, the helper classes are implemented so that their
own methods keep the cache consistent, and the header file will provide
a method to invalidate the cache and a description of what kinds of
operations would make the cache invalid. If in doubt, you can always
discard a helper class and create a new one with the same underlying
objects, which will ensure that you have discarded any stale
information.

By Convention, document helpers are called
``QPDFSomethingDocumentHelper`` and are derived from
``QPDFDocumentHelper``, and object helpers are called
``QPDFSomethingObjectHelper`` and are derived from ``QPDFObjectHelper``.
For details on specific helpers, please see their header files. You can
find them by looking at
:file:`include/qpdf/QPDF*DocumentHelper.hh` and
:file:`include/qpdf/QPDF*ObjectHelper.hh`.

In order to avoid creation of circular dependencies, the following
general guidelines are followed with helper classes:

- Core class interfaces do not know about helper classes. For example,
  no methods of ``QPDF`` or ``QPDFObjectHandle`` will include helper
  classes in their interfaces.

- Interfaces of object helpers will usually not use document helpers in
  their interfaces. This is because it is much more useful for document
  helpers to have methods that return object helpers. Most operations
  in PDF files start at the document level and go from there to the
  object level rather than the other way around. It can sometimes be
  useful to map back from object-level structures to document-level
  structures. If there is a desire to do this, it will generally be
  provided by a method in the document helper class.

- Most of the time, object helpers don't know about other object
  helpers. However, in some cases, one type of object may be a
  container for another type of object, in which case it may make sense
  for the outer object to know about the inner object. For example,
  there are methods in the ``QPDFPageObjectHelper`` that know
  ``QPDFAnnotationObjectHelper`` because references to annotations are
  contained in page dictionaries.

- Any helper or core library class may use helpers in their
  implementations.

Prior to qpdf version 8.1, higher level interfaces were added as
"convenience functions" in either ``QPDF`` or ``QPDFObjectHandle``. For
compatibility, older convenience functions for operating with pages will
remain in those classes even as alternatives are provided in helper
classes. Going forward, new higher level interfaces will be provided
using helper classes.

.. _implementation-notes:

Implementation Notes
--------------------

This section contains a few notes about QPDF's internal implementation,
particularly around what it does when it first processes a file. This
section is a bit of a simplification of what it actually does, but it
could serve as a starting point to someone trying to understand the
implementation. There is nothing in this section that you need to know
to use the qpdf library.

``QPDFObject`` is the basic PDF Object class. It is an abstract base
class from which are derived classes for each type of PDF object.
Clients do not interact with Objects directly but instead interact with
``QPDFObjectHandle``.

When the ``QPDF`` class creates a new object, it dynamically allocates
the appropriate type of ``QPDFObject`` and immediately hands the pointer
to an instance of ``QPDFObjectHandle``. The parser reads a token from
the current file position. If the token is a not either a dictionary or
array opener, an object is immediately constructed from the single token
and the parser returns. Otherwise, the parser iterates in a special mode
in which it accumulates objects until it finds a balancing closer.
During this process, the "``R``" keyword is recognized and an indirect
``QPDFObjectHandle`` may be constructed.

The ``QPDF::resolve()`` method, which is used to resolve an indirect
object, may be invoked from the ``QPDFObjectHandle`` class. It first
checks a cache to see whether this object has already been read. If not,
it reads the object from the PDF file and caches it. It the returns the
resulting ``QPDFObjectHandle``. The calling object handle then replaces
its ``PointerHolder<QDFObject>`` with the one from the newly returned
``QPDFObjectHandle``. In this way, only a single copy of any direct
object need exist and clients can access objects transparently without
knowing caring whether they are direct or indirect objects.
Additionally, no object is ever read from the file more than once. That
means that only the portions of the PDF file that are actually needed
are ever read from the input file, thus allowing the qpdf package to
take advantage of this important design goal of PDF files.

If the requested object is inside of an object stream, the object stream
itself is first read into memory. Then the tokenizer reads objects from
the memory stream based on the offset information stored in the stream.
Those individual objects are cached, after which the temporary buffer
holding the object stream contents are discarded. In this way, the first
time an object in an object stream is requested, all objects in the
stream are cached.

The following example should clarify how ``QPDF`` processes a simple
file.

- Client constructs ``QPDF`` ``pdf`` and calls
  ``pdf.processFile("a.pdf");``.

- The ``QPDF`` class checks the beginning of
  :file:`a.pdf` for a PDF header. It then reads the
  cross reference table mentioned at the end of the file, ensuring that
  it is looking before the last ``%%EOF``. After getting to ``trailer``
  keyword, it invokes the parser.

- The parser sees "``<<``", so it calls itself recursively in
  dictionary creation mode.

- In dictionary creation mode, the parser keeps accumulating objects
  until it encounters "``>>``". Each object that is read is pushed onto
  a stack. If "``R``" is read, the last two objects on the stack are
  inspected. If they are integers, they are popped off the stack and
  their values are used to construct an indirect object handle which is
  then pushed onto the stack. When "``>>``" is finally read, the stack
  is converted into a ``QPDF_Dictionary`` which is placed in a
  ``QPDFObjectHandle`` and returned.

- The resulting dictionary is saved as the trailer dictionary.

- The ``/Prev`` key is searched. If present, ``QPDF`` seeks to that
  point and repeats except that the new trailer dictionary is not
  saved. If ``/Prev`` is not present, the initial parsing process is
  complete.

  If there is an encryption dictionary, the document's encryption
  parameters are initialized.

- The client requests root object. The ``QPDF`` class gets the value of
  root key from trailer dictionary and returns it. It is an unresolved
  indirect ``QPDFObjectHandle``.

- The client requests the ``/Pages`` key from root
  ``QPDFObjectHandle``. The ``QPDFObjectHandle`` notices that it is
  indirect so it asks ``QPDF`` to resolve it. ``QPDF`` looks in the
  object cache for an object with the root dictionary's object ID and
  generation number. Upon not seeing it, it checks the cross reference
  table, gets the offset, and reads the object present at that offset.
  It stores the result in the object cache and returns the cached
  result. The calling ``QPDFObjectHandle`` replaces its object pointer
  with the one from the resolved ``QPDFObjectHandle``, verifies that it
  a valid dictionary object, and returns the (unresolved indirect)
  ``QPDFObject`` handle to the top of the Pages hierarchy.

  As the client continues to request objects, the same process is
  followed for each new requested object.

.. _casting:

Casting Policy
--------------

This section describes the casting policy followed by qpdf's
implementation. This is no concern to qpdf's end users and largely of no
concern to people writing code that uses qpdf, but it could be of
interest to people who are porting qpdf to a new platform or who are
making modifications to the code.

The C++ code in qpdf is free of old-style casts except where unavoidable
(e.g. where the old-style cast is in a macro provided by a third-party
header file). When there is a need for a cast, it is handled, in order
of preference, by rewriting the code to avoid the need for a cast,
calling ``const_cast``, calling ``static_cast``, calling
``reinterpret_cast``, or calling some combination of the above. As a
last resort, a compiler-specific ``#pragma`` may be used to suppress a
warning that we don't want to fix. Examples may include suppressing
warnings about the use of old-style casts in code that is shared between
C and C++ code.

The ``QIntC`` namespace, provided by
:file:`include/qpdf/QIntC.hh`, implements safe
functions for converting between integer types. These functions do range
checking and throw a ``std::range_error``, which is subclass of
``std::runtime_error``, if conversion from one integer type to another
results in loss of information. There are many cases in which we have to
move between different integer types because of incompatible integer
types used in interoperable interfaces. Some are unavoidable, such as
moving between sizes and offsets, and others are there because of old
code that is too in entrenched to be fixable without breaking source
compatibility and causing pain for users. QPDF is compiled with extra
warnings to detect conversions with potential data loss, and all such
cases should be fixed by either using a function from ``QIntC`` or a
``static_cast``.

When the intention is just to switch the type because of exchanging data
between incompatible interfaces, use ``QIntC``. This is the usual case.
However, there are some cases in which we are explicitly intending to
use the exact same bit pattern with a different type. This is most
common when switching between signed and unsigned characters. A lot of
qpdf's code uses unsigned characters internally, but ``std::string`` and
``char`` are signed. Using ``QIntC::to_char`` would be wrong for
converting from unsigned to signed characters because a negative
``char`` value and the corresponding ``unsigned char`` value greater
than 127 *mean the same thing*. There are also
cases in which we use ``static_cast`` when working with bit fields where
we are not representing a numerical value but rather a bunch of bits
packed together in some integer type. Also note that ``size_t`` and
``long`` both typically differ between 32-bit and 64-bit environments,
so sometimes an explicit cast may not be needed to avoid warnings on one
platform but may be needed on another. A conversion with ``QIntC``
should always be used when the types are different even if the
underlying size is the same. QPDF's CI build builds on 32-bit and 64-bit
platforms, and the test suite is very thorough, so it is hard to make
any of the potential errors here without being caught in build or test.

Non-const ``unsigned char*`` is used in the ``Pipeline`` interface. The
pipeline interface has a ``write`` call that uses ``unsigned char*``
without a ``const`` qualifier. The main reason for this is
to support pipelines that make calls to third-party libraries, such as
zlib, that don't include ``const`` in their interfaces. Unfortunately,
there are many places in the code where it is desirable to have
``const char*`` with pipelines. None of the pipeline implementations
in qpdf
currently modify the data passed to write, and doing so would be counter
to the intent of ``Pipeline``, but there is nothing in the code to
prevent this from being done. There are places in the code where
``const_cast`` is used to remove the const-ness of pointers going into
``Pipeline``\ s. This could theoretically be unsafe, but there is
adequate testing to assert that it is safe and will remain safe in
qpdf's code.

.. _encryption:

Encryption
----------

Encryption is supported transparently by qpdf. When opening a PDF file,
if an encryption dictionary exists, the ``QPDF`` object processes this
dictionary using the password (if any) provided. The primary decryption
key is computed and cached. No further access is made to the encryption
dictionary after that time. When an object is read from a file, the
object ID and generation of the object in which it is contained is
always known. Using this information along with the stored encryption
key, all stream and string objects are transparently decrypted. Raw
encrypted objects are never stored in memory. This way, nothing in the
library ever has to know or care whether it is reading an encrypted
file.

An interface is also provided for writing encrypted streams and strings
given an encryption key. This is used by ``QPDFWriter`` when it rewrites
encrypted files.

When copying encrypted files, unless otherwise directed, qpdf will
preserve any encryption in force in the original file. qpdf can do this
with either the user or the owner password. There is no difference in
capability based on which password is used. When 40 or 128 bit
encryption keys are used, the user password can be recovered with the
owner password. With 256 keys, the user and owner passwords are used
independently to encrypt the actual encryption key, so while either can
be used, the owner password can no longer be used to recover the user
password.

Starting with version 4.0.0, qpdf can read files that are not encrypted
but that contain encrypted attachments, but it cannot write such files.
qpdf also requires the password to be specified in order to open the
file, not just to extract attachments, since once the file is open, all
decryption is handled transparently. When copying files like this while
preserving encryption, qpdf will apply the file's encryption to
everything in the file, not just to the attachments. When decrypting the
file, qpdf will decrypt the attachments. In general, when copying PDF
files with multiple encryption formats, qpdf will choose the newest
format. The only exception to this is that clear-text metadata will be
preserved as clear-text if it is that way in the original file.

One point of confusion some people have about encrypted PDF files is
that encryption is not the same as password protection. Password
protected files are always encrypted, but it is also possible to create
encrypted files that do not have passwords. Internally, such files use
the empty string as a password, and most readers try the empty string
first to see if it works and prompt for a password only if the empty
string doesn't work. Normally such files have an empty user password and
a non-empty owner password. In that way, if the file is opened by an
ordinary reader without specification of password, the restrictions
specified in the encryption dictionary can be enforced. Most users
wouldn't even realize such a file was encrypted. Since qpdf always
ignores the restrictions (except for the purpose of reporting what they
are), qpdf doesn't care which password you use. QPDF will allow you to
create PDF files with non-empty user passwords and empty owner
passwords. Some readers will require a password when you open these
files, and others will open the files without a password and not enforce
restrictions. Having a non-empty user password and an empty owner
password doesn't really make sense because it would mean that opening
the file with the user password would be more restrictive than not
supplying a password at all. QPDF also allows you to create PDF files
with the same password as both the user and owner password. Some readers
will not ever allow such files to be accessed without restrictions
because they never try the password as the owner password if it works as
the user password. Nonetheless, one of the powerful aspects of qpdf is
that it allows you to finely specify the way encrypted files are
created, even if the results are not useful to some readers. One use
case for this would be for testing a PDF reader to ensure that it
handles odd configurations of input files.

.. _random-numbers:

Random Number Generation
------------------------

QPDF generates random numbers to support generation of encrypted data.
Starting in qpdf 10.0.0, qpdf uses the crypto provider as its source of
random numbers. Older versions used the OS-provided source of secure
random numbers or, if allowed at build time, insecure random numbers
from stdlib. Starting with version 5.1.0, you can disable use of
OS-provided secure random numbers at build time. This is especially
useful on Windows if you want to avoid a dependency on Microsoft's
cryptography API. You can also supply your own random data provider. For
details on how to do this, please refer to the top-level README.md file
in the source distribution and to comments in
:file:`QUtil.hh`.

.. _adding-and-remove-pages:

Adding and Removing Pages
-------------------------

While qpdf's API has supported adding and modifying objects for some
time, version 3.0 introduces specific methods for adding and removing
pages. These are largely convenience routines that handle two tricky
issues: pushing inheritable resources from the ``/Pages`` tree down to
individual pages and manipulation of the ``/Pages`` tree itself. For
details, see ``addPage`` and surrounding methods in
:file:`QPDF.hh`.

.. _reserved-objects:

Reserving Object Numbers
------------------------

Version 3.0 of qpdf introduced the concept of reserved objects. These
are seldom needed for ordinary operations, but there are cases in which
you may want to add a series of indirect objects with references to each
other to a ``QPDF`` object. This causes a problem because you can't
determine the object ID that a new indirect object will have until you
add it to the ``QPDF`` object with ``QPDF::makeIndirectObject``. The
only way to add two mutually referential objects to a ``QPDF`` object
prior to version 3.0 would be to add the new objects first and then make
them refer to each other after adding them. Now it is possible to create
a *reserved object* using
``QPDFObjectHandle::newReserved``. This is an indirect object that stays
"unresolved" even if it is queried for its type. So now, if you want to
create a set of mutually referential objects, you can create
reservations for each one of them and use those reservations to
construct the references. When finished, you can call
``QPDF::replaceReserved`` to replace the reserved objects with the real
ones. This functionality will never be needed by most applications, but
it is used internally by QPDF when copying objects from other PDF files,
as discussed in :ref:`foreign-objects`. For an example of how to use reserved
objects, search for ``newReserved`` in
:file:`test_driver.cc` in qpdf's sources.

.. _foreign-objects:

Copying Objects From Other PDF Files
------------------------------------

Version 3.0 of qpdf introduced the ability to copy objects into a
``QPDF`` object from a different ``QPDF`` object, which we refer to as
*foreign objects*. This allows arbitrary
merging of PDF files. The "from" ``QPDF`` object must remain valid after
the copy as discussed in the note below. The
:command:`qpdf` command-line tool provides limited
support for basic page selection, including merging in pages from other
files, but the library's API makes it possible to implement arbitrarily
complex merging operations. The main method for copying foreign objects
is ``QPDF::copyForeignObject``. This takes an indirect object from
another ``QPDF`` and copies it recursively into this object while
preserving all object structure, including circular references. This
means you can add a direct object that you create from scratch to a
``QPDF`` object with ``QPDF::makeIndirectObject``, and you can add an
indirect object from another file with ``QPDF::copyForeignObject``. The
fact that ``QPDF::makeIndirectObject`` does not automatically detect a
foreign object and copy it is an explicit design decision. Copying a
foreign object seems like a sufficiently significant thing to do that it
should be done explicitly.

The other way to copy foreign objects is by passing a page from one
``QPDF`` to another by calling ``QPDF::addPage``. In contrast to
``QPDF::makeIndirectObject``, this method automatically distinguishes
between indirect objects in the current file, foreign objects, and
direct objects.

Please note: when you copy objects from one ``QPDF`` to another, the
source ``QPDF`` object must remain valid until you have finished with
the destination object. This is because the original object is still
used to retrieve any referenced stream data from the copied object.

.. _rewriting:

Writing PDF Files
-----------------

The qpdf library supports file writing of ``QPDF`` objects to PDF files
through the ``QPDFWriter`` class. The ``QPDFWriter`` class has two
writing modes: one for non-linearized files, and one for linearized
files. See :ref:`linearization` for a description of
linearization is implemented. This section describes how we write
non-linearized files including the creation of QDF files (see :ref:`qdf`.

This outline was written prior to implementation and is not exactly
accurate, but it provides a correct "notional" idea of how writing
works. Look at the code in ``QPDFWriter`` for exact details.

- Initialize state:

  - next object number = 1

  - object queue = empty

  - renumber table: old object id/generation to new id/0 = empty

  - xref table: new id -> offset = empty

- Create a QPDF object from a file.

- Write header for new PDF file.

- Request the trailer dictionary.

- For each value that is an indirect object, grab the next object
  number (via an operation that returns and increments the number). Map
  object to new number in renumber table. Push object onto queue.

- While there are more objects on the queue:

  - Pop queue.

  - Look up object's new number *n* in the renumbering table.

  - Store current offset into xref table.

  - Write ``:samp:`{n}` 0 obj``.

  - If object is null, whether direct or indirect, write out null,
    thus eliminating unresolvable indirect object references.

  - If the object is a stream stream, write stream contents, piped
    through any filters as required, to a memory buffer. Use this
    buffer to determine the stream length.

  - If object is not a stream, array, or dictionary, write out its
    contents.

  - If object is an array or dictionary (including stream), traverse
    its elements (for array) or values (for dictionaries), handling
    recursive dictionaries and arrays, looking for indirect objects.
    When an indirect object is found, if it is not resolvable, ignore.
    (This case is handled when writing it out.) Otherwise, look it up
    in the renumbering table. If not found, grab the next available
    object number, assign to the referenced object in the renumbering
    table, and push the referenced object onto the queue. As a special
    case, when writing out a stream dictionary, replace length,
    filters, and decode parameters as required.

    Write out dictionary or array, replacing any unresolvable indirect
    object references with null (pdf spec says reference to
    non-existent object is legal and resolves to null) and any
    resolvable ones with references to the renumbered objects.

  - If the object is a stream, write ``stream\n``, the stream contents
    (from the memory buffer), and ``\nendstream\n``.

  - When done, write ``endobj``.

Once we have finished the queue, all referenced objects will have been
written out and all deleted objects or unreferenced objects will have
been skipped. The new cross-reference table will contain an offset for
every new object number from 1 up to the number of objects written. This
can be used to write out a new xref table. Finally we can write out the
trailer dictionary with appropriately computed /ID (see spec, 8.3, File
Identifiers), the cross reference table offset, and ``%%EOF``.

.. _filtered-streams:

Filtered Streams
----------------

Support for streams is implemented through the ``Pipeline`` interface
which was designed for this package.

When reading streams, create a series of ``Pipeline`` objects. The
``Pipeline`` abstract base requires implementation ``write()`` and
``finish()`` and provides an implementation of ``getNext()``. Each
pipeline object, upon receiving data, does whatever it is going to do
and then writes the data (possibly modified) to its successor.
Alternatively, a pipeline may be an end-of-the-line pipeline that does
something like store its output to a file or a memory buffer ignoring a
successor. For additional details, look at
:file:`Pipeline.hh`.

``QPDF`` can read raw or filtered streams. When reading a filtered
stream, the ``QPDF`` class creates a ``Pipeline`` object for one of each
appropriate filter object and chains them together. The last filter
should write to whatever type of output is required. The ``QPDF`` class
has an interface to write raw or filtered stream contents to a given
pipeline.

.. _object-accessors:

Object Accessor Methods
-----------------------

..
  This section is referenced in QPDFObjectHandle.hh

For general information about how to access instances of
``QPDFObjectHandle``, please see the comments in
:file:`QPDFObjectHandle.hh`. Search for "Accessor
methods". This section provides a more in-depth discussion of the
behavior and the rationale for the behavior.

*Why were type errors made into warnings?* When type checks were
introduced into qpdf in the early days, it was expected that type errors
would only occur as a result of programmer error. However, in practice,
type errors would occur with malformed PDF files because of assumptions
made in code, including code within the qpdf library and code written by
library users. The most common case would be chaining calls to
``getKey()`` to access keys deep within a dictionary. In many cases,
qpdf would be able to recover from these situations, but the old
behavior often resulted in crashes rather than graceful recovery. For
this reason, the errors were changed to warnings.

*Why even warn about type errors when the user can't usually do anything
about them?* Type warnings are extremely valuable during development.
Since it's impossible to catch at compile time things like typos in
dictionary key names or logic errors around what the structure of a PDF
file might be, the presence of type warnings can save lots of developer
time. They have also proven useful in exposing issues in qpdf itself
that would have otherwise gone undetected.

*Can there be a type-safe ``QPDFObjectHandle``?* It would be great if
``QPDFObjectHandle`` could be more strongly typed so that you'd have to
have check that something was of a particular type before calling
type-specific accessor methods. However, implementing this at this stage
of the library's history would be quite difficult, and it would make a
the common pattern of drilling into an object no longer work. While it
would be possible to have a parallel interface, it would create a lot of
extra code. If qpdf were written in a language like rust, an interface
like this would make a lot of sense, but, for a variety of reasons, the
qpdf API is consistent with other APIs of its time, relying on exception
handling to catch errors. The underlying PDF objects are inherently not
type-safe. Forcing stronger type safety in ``QPDFObjectHandle`` would
ultimately cause a lot more code to have to be written and would like
make software that uses qpdf more brittle, and even so, checks would
have to occur at runtime.

*Why do type errors sometimes raise exceptions?* The way warnings work
in qpdf requires a ``QPDF`` object to be associated with an object
handle for a warning to be issued. It would be nice if this could be
fixed, but it would require major changes to the API. Rather than
throwing away these conditions, we convert them to exceptions. It's not
that bad though. Since any object handle that was read from a file has
an associated ``QPDF`` object, it would only be type errors on objects
that were created explicitly that would cause exceptions, and in that
case, type errors are much more likely to be the result of a coding
error than invalid input.

*Why does the behavior of a type exception differ between the C and C++
API?* There is no way to throw and catch exceptions in C short of
something like ``setjmp`` and ``longjmp``, and that approach is not
portable across language barriers. Since the C API is often used from
other languages, it's important to keep things as simple as possible.
Starting in qpdf 10.5, exceptions that used to crash code using the C
API will be written to stderr by default, and it is possible to register
an error handler. There's no reason that the error handler can't
simulate exception handling in some way, such as by using ``setjmp`` and
``longjmp`` or by setting some variable that can be checked after
library calls are made. In retrospect, it might have been better if the
C API object handle methods returned error codes like the other methods
and set return values in passed-in pointers, but this would complicate
both the implementation and the use of the library for a case that is
actually quite rare and largely avoidable.

*How can I avoid type warnings altogether?* For each
``getSomethingValue`` accessor that returns a value of the requested
type and issues a warning for objects of the wrong type, there is also
a ``getValueAsSomething`` method (since qpdf 10.6) that returns false
for objects of the wrong type and otherwise returns true and
initializes a reference. These methods never generate type warnings
and provide an alternative to explicitly checking the type of an
object before calling an accessor method.

.. _smart-pointers:

Smart Pointers
--------------

This section describes changes to the use of smart pointers that were
made in qpdf 10.6.0 as well as some planned for 11.0.0.

Starting in qpdf 11, ``PointerHolder`` will be replaced with
``std::shared_ptr`` in qpdf's public API. A backward-compatible
``PointerHolder`` class will be provided that should make it possible
for most code to remain unchanged. ``PointerHolder`` may eventually be
removed from qpdf entirely, but this will not happen for a while to
make it easier for people who need to support multiple versions of
qpdf.

The ``POINTERHOLDER_TRANSITION`` preprocessor symbol has been
introduced to help people transition from ``PointerHolder`` to
``std::shared_ptr``. After qpdf 11 is released, to prepare for a
future qpdf without ``PointerHolder`` and to let them know that it is
no longer needed, a warning will be issued if
``<qpdf/PointerHolder.hh>`` is included, though it will be possible to
suppress the warning by defining ``POINTERHOLDER_TRANSITION``. In
10.6.0, there are some steps you can perform to prepare, but no action
is required.

The remainder of this section describes how to prepare if you want to
eliminate ``PointerHolder`` from your code or what to do if you want
to stick with the old interfaces.

Changes in 10.6.0
~~~~~~~~~~~~~~~~~

In qpdf 10.6.0, the following changes have been made to
``PointerHolder`` to make its behavior closer to that of
``std::shared_ptr``:

- ``get()`` has been added as an alternative to ``getPointer()``

- ``use_count()`` has been added as an alternative to ``getRefcount()``

- A new global helper function ``make_pointer_holder`` behaves
  similarly to ``std::make_shared``, so you can use
  ``make_pointer_holder<T>(args...)`` to create a ``PointerHolder<T>``
  with ``new T(args...)`` as the pointer.

- A new global helper function ``make_array_pointer_holder`` takes a
  size and creates a ``PointerHolder`` to an array. It is a
  counterpart to the newly added ``QUtil::make_shared_array`` method,
  which does the same thing with a ``std::shared_ptr``.

``PointerHolder`` has had a long-standing bug: a ``const
PointerHolder<T>`` would only provide a ``T const*`` with its
``getPointer`` method. This is incorrect and is not how standard
library C++ smart pointers or regular pointers behave. The correct
semantics would be that a ``const PointerHolder<T>`` would not accept
a new pointer after being created (``PointerHolder`` has always
behaved correctly in this way) but would still allow you to modify the
item being pointed to. If you don't want to mutate the thing it points
to, use ``PointerHolder<T const>`` instead. The new ``get()`` method
behaves correctly. It is therefore not exactly the same as
``getPointer()``, but it does behave the way ``get()`` behaves with
``std::shared_ptr``. This shouldn't make any difference to any
correctly written code.

Differences between ``PointerHolder`` and ``std::shared_ptr``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Here is a list of things you need to think about when migrating from
``PointerHolder`` to ``std::shared_ptr``. After the list, we will
discuss how to address each one using the ``POINTERHOLDER_TRANSITION``
preprocessor symbol or other C++ coding techniques.

- ``PointerHolder<T>`` has an *implicit* constructor that takes a
  ``T*``, which means you can assign a ``T*`` directly to a
  ``PointerHolder<T>`` or pass a ``T*`` to a function that expects a
  ``PointerHolder<T>`` as a parameter. ``std::shared_ptr<T>`` does not
  have this behavior, though you can still assign ``nullptr`` to a
  ``std::shared_ptr<T>`` and compare ``nullptr`` with a
  ``std::shared_ptr<T>``. Here are some examples of how you might need
  to change your code:

  Old code:
   .. code-block:: c++

      PointerHolder<X> x_p;
      X* x = new X();
      x_p = x;

  New code:
   .. code-block:: c++

      auto x_p = std::make_shared<X>();
      X* x = x_p.get();
      // or, less safe, but closer:
      std::shared_ptr<X> x_p;
      X* x = new X();
      x_p = std::shared_ptr<X>(x);

  Old code:
   .. code-block:: c++

      PointerHolder<Base> base_p;
      Derived* derived = new Derived();
      base_p = derived;

  New code:
   .. code-block:: c++

      std::shared_ptr<Base> base_p;
      Derived* derived = new Derived();
      base_p = std::shared_ptr<Base>(derived);

- ``PointerHolder<T>`` has ``getPointer()`` to get the underlying
  pointer. It also has the seldom-used ``getRefcount()`` method to get
  the reference count. ``std::shared_ptr<T>`` has ``get()`` and
  ``use_count()``. In qpdf 10.6, ``PointerHolder<T>`` also has
  would not be an issue unless you did this in your own code.

Addressing the Differences
~~~~~~~~~~~~~~~~~~~~~~~~~~

If you need to support versions of qpdf prior to qpdf 10.6, you don't
*need* to take any action at this time, but it is recommended that you
at least address the implicit constructor issue since this can be done
without breaking backward compatibility. (Explicit construction of
``PointerHolder<T>`` is and always has been allowed.)

There are two significant things you can do to minimize the impact of
switching from ``PointerHolder`` to ``std::shared_ptr``:

- Use ``auto`` and ``decltype`` whenever possible when working with
  ``PointerHolder`` variables that are exchanged with the qpdf API.

- Use the ``POINTERHOLDER_TRANSITION`` preprocessor symbol to identify
  and resolve the differences described above.

To use ``POINTERHOLDER_TRANSITION``, you will need to ``#define`` it
before including any qpdf header files or specify its value as part of
your build. The table below describes the values of
``POINTERHOLDER_TRANSITION``. This information is also summarized in
:file:`include/qpdf/PointerHolder.hh`, so you will have it handy
without consulting this manual.

.. list-table:: POINTERHOLDER_TRANSITION values
   :widths: 5 80
   :header-rows: 1

   - - value
     - meaning

   - - undefined
     - Same as ``0``, but starting with qpdf 11.0, issues a warning

   - - ``0``
     - Provide a backward compatible ``PointerHolder`` and suppress
       all deprecation warnings

   - - ``1``
     - Make the ``PointerHolder<T>(T*)`` constructor explicit

   - - ``2``
     - Deprecate ``getPointer()`` and ``getRefcount()``

   - - ``3``
     - Starting with qpdf 11.0, deprecate all uses of ``PointerHolder``

   - - ``4``
     - Starting with qpdf 11.0, disable all functionality from
       ``qpdf/PointerHolder.hh`` so that ``#include``-ing it has no
       effect.

Based on the above, here is a procedure for preparing your code. This
is the procedure that was used for the qpdf code itself.

If you need to support versions of qpdf prior to 10.6, you can still
do these steps:

- Find all occurrences of ``PointerHolder`` in the code. See whether
  any of them can just be outright replaced with ``std::shared_ptr``
  or ``std::unique_ptr``. If you have been using qpdf prior to
  adopting C++11 and were using ``PointerHolder`` as a general-purpose
  smart pointer, you may have cases that can be replaced in this way.

  For example:

  - Simple ``PointerHolder<T>`` construction can be replaced with
    either the equivalent ``std::shared_ptr<T>`` construction or, if
    the constructor is public, with ``std::make_shared<T>(args...)``.
    If you are creating a smart pointer that is never copied, you may
    be able to use ``std::unique_ptr<T>`` instead.

  - Array allocations will have to be rewritten.

    Allocating a ``PointerHolder`` to an array looked like this:

    .. code-block:: c++

       PointerHolder<X> p(true, new X[n]);

    To allocate a ``std::shared_ptr`` to an array:

    .. code-block:: c++

       auto p = std::shared_ptr<X>(new X[n], std::default_delete<X[]>());
       // If you don't mind using QUtil, there's QUtil::make_shared_array<X>(n).
       // If you are using c++20, you can use std::make_shared<X[]>(n)
       // to get a std::shared_ptr<X[]> instead of a std::shared_ptr<X>.

    To allocate a ``std::unique_ptr`` to an array:

    .. code-block:: c++

       auto p = std::make_unique<X[]>(n);
       // or, if X has a private constructor:
       auto p = std::unique_ptr<X[]>(new X[n]);

- If a ``PointerHolder<T>`` can't be replaced with a standard library
  smart pointer, perhaps it can be declared using ``auto`` or
  ``decltype`` so that, when the qpdf API changes, your code will just
  need to be recompiled.

- ``#define POINTERHOLDER_TRANSITION 1`` to enable deprecation
  warnings for all implicit constructions of ``PointerHolder<T>`` from
  a plain ``T*``. When you find one, explicitly construct the
  ``PointerHolder<T>``.

  - Old code:

    .. code-block:: c++

       PointerHolder<X> x = new X();

  - New code:

    .. code-block:: c++

       auto x = PointerHolder<X>(new X(...)); // all versions of qpdf
       // or, if X(...) is public:
       auto x = make_pointer_holder<X>(...); // only 10.6 and above

    Other examples appear above.

If you need to support older versions of qpdf than 10.6, this is as
far as you can go until qpdf 11 comes out.

If you only need to support the latest version of qpdf, proceed as
follows:

- ``#define POINTERHOLDER_TRANSITION 2`` to enable deprecation of
  ``getPointer()`` and ``getRefcount()``

- Replace ``getPointer()`` with ``get()`` and ``getRefcount()`` with
  ``use_count()``. These methods were not present prior to 10.6.0.

When you have gotten your code to compile cleanly with
``POINTERHOLDER_TRANSITION=2``, you are well on your way to being
ready for eliminating ``PointerHolder`` entirely after qpdf 11 is
released.

After qpdf 11 is out
~~~~~~~~~~~~~~~~~~~~

In the 10.6 manual, this section represents a plan and is subject to
change. However, it has been tested in practice using a version of the
qpdf 11 ``PointerHolder`` on a branch, so it is likely to be accurate.
In the meantime, think of this as a preview.

First, make sure you have done the steps in the 10.6 section. (Note:
once qpdf 11 comes out, the goal is to not have to migrate to 10.6
first, so it is likely that these sections will be combined.)

If you are explicitly choosing to stick with the backward compatible
``PointerHolder`` for now, you should define
``POINTERHOLDER_TRANSITION`` to ``0`` to suppress the warning from
including ``qpdf/PointerHolder.hh``. Be aware that you may eventually
have to deal with the transition, though the intention is to leave the
compatibility layer in place for a while. You should rebuild and test
your code. There may be compiler errors if you have containers of
``PointerHolder``, but most code should compile without any changes.
Even if you have errors, use of ``auto`` or ``decltype`` may enable
you to write code that works with the old and new API without having
to use conditional compilation. The
``POINTERHOLDER_IS_SHARED_POINTER`` is defined in qpdf 11 if you
``#include <qpdf/PointerHolder.hh>``.

If you want to support older versions of qpdf and still transition so
that the backward-compatible ``PointerHolder`` is not in use, you can
separate old code and new code by testing with the
``POINTERHOLDER_IS_SHARED_POINTER`` preprocessor symbol, as in

.. code-block:: c++

   #ifdef POINTERHOLDER_IS_SHARED_POINTER
   std::shared_ptr<X> x;
   #else
   PointerHolder<X> x;
   #endif // POINTERHOLDER_IS_SHARED_POINTER
   x = decltype(x)(new X())

or

.. code-block:: c++

   #ifdef POINTERHOLDER_IS_SHARED_POINTER
   auto x_p = std::make_shared<X>();
   X* x = x_p.get();
   #else
   auto x_p = PointerHolder<X>(new X());
   X* x = x_p.getPointer();
   #endif // POINTERHOLDER_IS_SHARED_POINTER
   x_p->doSomething();
   x->doSomethingElse();

If you don't need to support older versions of qpdf, you can proceed
with these steps without protecting changes with the preprocessor
symbol. Here are the remaining changes.

- Make sure you have a clean build with ``POINTERHOLDER_TRANSITION``
  set to ``2``. This means that you are using ``PointerHolder`` in a
  manner that is API-compatible with ``std::shared_ptr`` in all cases
  except for array pointers.

- Replace all occurrences of ``PointerHolder`` with
  ``std::shared_ptr`` except in ``#include <qpdf/PointerHolder.hh>``

- Replace all occurrences of ``make_pointer_holder`` with
  ``std::make_shared``

- Replace all occurrences of ``make_array_pointer_holder`` with
  ``QUtil::make_shared_array``. You will need to include
  ``<qpdf/QUtil.hh>`` if you haven't already done so.

- Make sure ``<memory>`` is included wherever you were including
  ``<qpdf/PointerHolder.hh>``.

- If you were using any array ``PointerHolder<T>`` objects, replace
  them as above. You can let the compiler find these for you.

- ``#define POINTERHOLDER_TRANSITION 3`` to enable deprecation of
  all ``PointerHolder<T>`` construction.

- Build and test. Fix any remaining issues.

- If not supporting older versions of qpdf, remove all references to
  ``<qpdf/PointerHolder.hh>``. Otherwise, you will still need to
  include it but can ``#define POINTERHOLDER_TRANSITION 4`` to prevent
  ``PointerHolder`` from being defined. The
  ``POINTERHOLDER_IS_SHARED_POINTER`` symbol will still be defined.

Historical Background
~~~~~~~~~~~~~~~~~~~~~

Since its inception, the qpdf library used its own smart pointer
class, ``PointerHolder``. The ``PointerHolder`` class was originally
created long before ``std::shared_ptr`` existed, and qpdf itself
didn't start requiring a C++11 compiler until version 9.1.0 released in
late 2019. With current C++ versions, it is no longer desirable for qpdf
to have its own smart pointer class.
