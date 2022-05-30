.. _object-and-xref-streams:

Object and Cross-Reference Streams
==================================

This chapter provides information about the implementation of object
stream and cross-reference stream support in qpdf.

.. _object-streams:

Object Streams
--------------

Object streams can contain any regular object except the following:

- stream objects

- objects with generation > 0

- the encryption dictionary

- objects containing the /Length of another stream

In addition, Adobe reader (at least as of version 8.0.0) appears to not
be able to handle having the document catalog appear in an object stream
if the file is encrypted, though this is not specifically disallowed by
the specification.

There are additional restrictions for linearized files. See
:ref:`object-streams-linearization` for details.

The PDF specification refers to objects in object streams as "compressed
objects" regardless of whether the object stream is compressed.

The generation number of every object in an object stream must be zero.
It is possible to delete and replace an object in an object stream with
a regular object.

The object stream dictionary has the following keys:

- ``/N``: number of objects

- ``/First``: byte offset of first object

- ``/Extends``: indirect reference to stream that this extends

Stream collections are formed with ``/Extends``. They must form a
directed acyclic graph. These can be used for semantic information and
are not meaningful to the PDF document's syntactic structure. Although
qpdf preserves stream collections, it never generates them and doesn't
make use of this information in any way.

The specification recommends limiting the number of objects in object
stream for efficiency in reading and decoding. Acrobat 6 uses no more
than 100 objects per object stream for linearized files and no more 200
objects per stream for non-linearized files. ``QPDFWriter``, in object
stream generation mode, never puts more than 100 objects in an object
stream.

Object stream contents consists of *N* pairs of integers, each of which
is the object number and the byte offset of the object relative to the
first object in the stream, followed by the objects themselves,
concatenated.

.. _xref-streams:

Cross-Reference Streams
-----------------------

For non-hybrid files, the value following ``startxref`` is the byte
offset to the xref stream rather than the word ``xref``.

For hybrid files (files containing both xref tables and cross-reference
streams), the xref table's trailer dictionary contains the key
``/XRefStm`` whose value is the byte offset to a cross-reference stream
that supplements the xref table. A PDF 1.5-compliant application should
read the xref table first. Then it should replace any object that it has
already seen with any defined in the xref stream. Then it should follow
any ``/Prev`` pointer in the original xref table's trailer dictionary.
The specification is not clear about what should be done, if anything,
with a ``/Prev`` pointer in the xref stream referenced by an xref table.
The ``QPDF`` class ignores it, which is probably reasonable since, if
this case were to appear for any sensible PDF file, the previous xref
table would probably have a corresponding ``/XRefStm`` pointer of its
own. For example, if a hybrid file were appended, the appended section
would have its own xref table and ``/XRefStm``. The appended xref table
would point to the previous xref table which would point the
``/XRefStm``, meaning that the new ``/XRefStm`` doesn't have to point to
it.

Since xref streams must be read very early, they may not be encrypted,
and the may not contain indirect objects for keys required to read them,
which are these:

- ``/Type``: value ``/XRef``

- ``/Size``: value *n+1*: where *n* is highest object number (same as
  ``/Size`` in the trailer dictionary)

- ``/Index`` (optional): value
  ``[:samp:`{n count}` ...]`` used to determine
  which objects' information is stored in this stream. The default is
  ``[0 /Size]``.

- ``/Prev``: value :samp:`{offset}`: byte
  offset of previous xref stream (same as ``/Prev`` in the trailer
  dictionary)

- ``/W [...]``: sizes of each field in the xref table

The other fields in the xref stream, which may be indirect if desired,
are the union of those from the xref table's trailer dictionary.

.. _xref-stream-data:

Cross-Reference Stream Data
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The stream data is binary and encoded in big-endian byte order. Entries
are concatenated, and each entry has a length equal to the total of the
entries in ``/W`` above. Each entry consists of one or more fields, the
first of which is the type of the field. The number of bytes for each
field is given by ``/W`` above. A 0 in ``/W`` indicates that the field
is omitted and has the default value. The default value for the field
type is ``1``. All other default values are ``0``.

PDF 1.5 has three field types:

- 0: for free objects. Format: ``0 obj next-generation``, same as the
  free table in a traditional cross-reference table

- 1: regular non-compressed object. Format: ``1 offset generation``

- 2: for objects in object streams. Format: ``2 object-stream-number
  index``, the number of object stream containing the object and the
  index within the object stream of the object.

It seems standard to have the first entry in the table be ``0 0 0``
instead of ``0 0 ffff`` if there are no deleted objects.

.. _object-streams-linearization:

Implications for Linearized Files
---------------------------------

For linearized files, the linearization dictionary, document catalog,
and page objects may not be contained in object streams.

Objects stored within object streams are given the highest range of
object numbers within the main and first-page cross-reference sections.

It is okay to use cross-reference streams in place of regular xref
tables. There are on special considerations.

Hint data refers to object streams themselves, not the objects in the
streams. Shared object references should also be made to the object
streams. There are no reference in any hint tables to the object numbers
of compressed objects (objects within object streams).

When numbering objects, all shared objects within both the first and
second halves of the linearized files must be numbered consecutively
after all normal uncompressed objects in that half.

.. _object-stream-implementation:

Implementation Notes
--------------------

There are three modes for writing object streams:
:samp:`disable`, :samp:`preserve`, and
:samp:`generate`. In disable mode, we do not generate
any object streams, and we also generate an xref table rather than xref
streams. This can be used to generate PDF files that are viewable with
older readers. In preserve mode, we write object streams such that
written object streams contain the same objects and ``/Extends``
relationships as in the original file. This is equal to disable if the
file has no object streams. In generate, we create object streams
ourselves by grouping objects that are allowed in object streams
together in sets of no more than 100 objects. We also ensure that the
PDF version is at least 1.5 in generate mode, but we preserve the
version header in the other modes. The default is
:samp:`preserve`.

We do not support creation of hybrid files. When we write files, even in
preserve mode, we will lose any xref tables and merge any appended
sections.
