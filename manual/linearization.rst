.. _linearization:

Linearization
=============

This chapter describes how ``QPDF`` and ``QPDFWriter`` implement
creation and processing of linearized PDFS.

.. _linearization-strategy:

Basic Strategy for Linearization
--------------------------------

To avoid the incestuous problem of having the qpdf library validate its
own linearized files, we have a special linearized file checking mode
which can be invoked via :command:`qpdf
--check-linearization` (or :command:`qpdf
--check`). This mode reads the linearization parameter
dictionary and the hint streams and validates that object ordering,
parameters, and hint stream contents are correct. The validation code
was first tested against linearized files created by external tools
(Acrobat and pdlin) and then used to validate files created by
``QPDFWriter`` itself.

.. _linearized.preparation:

Preparing For Linearization
---------------------------

Before creating a linearized PDF file from any other PDF file, the PDF
file must be altered such that all page attributes are propagated down
to the page level (and not inherited from parents in the ``/Pages``
tree). We also have to know which objects refer to which other objects,
being concerned with page boundaries and a few other cases. We refer to
this part of preparing the PDF file as
*optimization*, discussed in
:ref:`optimization`. Note the, in this context, the
term *optimization* is a qpdf term, and the
term *linearization* is a term from the PDF
specification. Do not be confused by the fact that many applications
refer to linearization as optimization or web optimization.

When creating linearized PDF files from optimized PDF files, there are
really only a few issues that need to be dealt with:

- Creation of hints tables

- Placing objects in the correct order

- Filling in offsets and byte sizes

.. _optimization:

Optimization
------------

In order to perform various operations such as linearization and
splitting files into pages, it is necessary to know which objects are
referenced by which pages, page thumbnails, and root and trailer
dictionary keys. It is also necessary to ensure that all page-level
attributes appear directly at the page level and are not inherited from
parents in the pages tree.

We refer to the process of enforcing these constraints as
*optimization*. As mentioned above, note
that some applications refer to linearization as optimization. Although
this optimization was initially motivated by the need to create
linearized files, we are using these terms separately.

PDF file optimization is implemented in the
:file:`QPDF_optimization.cc` source file. That file
is richly commented and serves as the primary reference for the
optimization process.

After optimization has been completed, the private member variables
``obj_user_to_objects`` and ``object_to_obj_users`` in ``QPDF`` have
been populated. Any object that has more than one value in the
``object_to_obj_users`` table is shared. Any object that has exactly one
value in the ``object_to_obj_users`` table is private. To find all the
private objects in a page or a trailer or root dictionary key, one
merely has make this determination for each element in the
``obj_user_to_objects`` table for the given page or key.

Note that pages and thumbnails have different object user types, so the
above test on a page will not include objects referenced by the page's
thumbnail dictionary and nothing else.

.. _linearization.writing:

Writing Linearized Files
------------------------

We will create files with only primary hint streams. We will never write
overflow hint streams. (As of PDF version 1.4, Acrobat doesn't either,
and they are never necessary.) The hint streams contain offset
information to objects that point to where they would be if the hint
stream were not present. This means that we have to calculate all object
positions before we can generate and write the hint table. This means
that we have to generate the file in two passes. To make this reliable,
``QPDFWriter`` in linearization mode invokes exactly the same code twice
to write the file to a pipeline.

In the first pass, the target pipeline is a count pipeline chained to a
discard pipeline. The count pipeline simply passes its data through to
the next pipeline in the chain but can return the number of bytes passed
through it at any intermediate point. The discard pipeline is an end of
line pipeline that just throws its data away. The hint stream is not
written and dummy values with adequate padding are stored in the first
cross reference table, linearization parameter dictionary, and /Prev key
of the first trailer dictionary. All the offset, length, object
renumbering information, and anything else we need for the second pass
is stored.

At the end of the first pass, this information is passed to the ``QPDF``
class which constructs a compressed hint stream in a memory buffer and
returns it. ``QPDFWriter`` uses this information to write a complete
hint stream object into a memory buffer. At this point, the length of
the hint stream is known.

In the second pass, the end of the pipeline chain is a regular file
instead of a discard pipeline, and we have known values for all the
offsets and lengths that we didn't have in the first pass. We have to
adjust offsets that appear after the start of the hint stream by the
length of the hint stream, which is known. Anything that is of variable
length is padded, with the padding code surrounding any writing code
that differs in the two passes. This ensures that changes to the way
things are represented never results in offsets that were gathered
during the first pass becoming incorrect for the second pass.

Using this strategy, we can write linearized files to a non-seekable
output stream with only a single pass to disk or wherever the output is
going.

.. _linearization-data:

Calculating Linearization Data
------------------------------

Once a file is optimized, we have information about which objects access
which other objects. We can then process these tables to decide which
part (as described in "Linearized PDF Document Structure" in the PDF
specification) each object is contained within. This tells us the exact
order in which objects are written. The ``QPDFWriter`` class asks for
this information and enqueues objects for writing in the proper order.
It also turns on a check that causes an exception to be thrown if an
object is encountered that has not already been queued. (This could
happen only if there were a bug in the traversal code used to calculate
the linearization data.)

.. _linearization-issues:

Known Issues with Linearization
-------------------------------

There are a handful of known issues with this linearization code. These
issues do not appear to impact the behavior of linearized files which
still work as intended: it is possible for a web browser to begin to
display them before they are fully downloaded. In fact, it seems that
various other programs that create linearized files have many of these
same issues. These items make reference to terminology used in the
linearization appendix of the PDF specification.

- Thread Dictionary information keys appear in part 4 with the rest of
  Threads instead of in part 9. Objects in part 9 are not grouped
  together functionally.

- We are not calculating numerators for shared object positions within
  content streams or interleaving them within content streams.

- We generate only page offset, shared object, and outline hint tables.
  It would be relatively easy to add some additional tables. We gather
  most of the information needed to create thumbnail hint tables. There
  are comments in the code about this.

.. _linearization-debugging:

Debugging Note
--------------

The :command:`qpdf --show-linearization` command can show
the complete contents of linearization hint streams. To look at the raw
data, you can extract the filtered contents of the linearization hint
tables using :command:`qpdf --show-object=n
--filtered-stream-data`. Then, to convert this into a bit
stream (since linearization tables are bit streams written without
regard to byte boundaries), you can pipe the resulting data through the
following perl code:

.. code-block:: perl

   use bytes;
   binmode STDIN;
   undef $/;
   my $a = <STDIN>;
   my @ch = split(//, $a);
   map { printf("%08b", ord($_)) } @ch;
   print "\n";
