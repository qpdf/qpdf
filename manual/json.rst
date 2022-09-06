.. cSpell:ignore moddifyannotations
.. cSpell:ignore feff

.. _json:

qpdf JSON
=========

.. _json-overview:

Overview
--------

Beginning with qpdf version 11.0.0, the qpdf library and command-line
program can produce a JSON representation of a PDF file. qpdf version
11 introduces JSON format version 2. Prior to qpdf 11, versions 8.3.0
onward had a more limited JSON representation accessible only from the
command-line. For details on what changed, see :ref:`json-v2-changes`.
The rest of this chapter documents qpdf JSON version 2.

Please note: this chapter discusses *qpdf JSON format*, which
represents the contents of a PDF file. This is distinct from the
*QPDFJob JSON format* which provides a higher-level interface
interacting with qpdf the way the command-line tool does. For
information about that, see :ref:`qpdf-job`.

The qpdf JSON format is specific to qpdf. The :qpdf:ref:`--json`
command-line flag causes creation of a JSON representation the objects
in a PDF file along with JSON-formatted summaries of other information
about the file. This functionality is built into ``QPDFJob`` and can
be accessed from the ``qpdf`` command-line tool or from the
``QPDFJob`` C or C++ API.

Starting with qpdf JSON version 2, from qpdf 11.0.0, the JSON output
includes an unambiguous and complete representation of the PDF objects
and header. The information without the JSON-formatted summaries of
other information is also available using the ``QPDF::writeJSON``
method.

By default, stream data is omitted from the JSON data, but it can be
included by specifying the :qpdf:ref:`--json-stream-data` option. With
stream data included, the generated JSON file completely represents a
PDF file. You can think of this as using JSON as an *alternative
syntax* for representing a PDF file. Using qpdf JSON, it is possible
to convert a PDF file to JSON, manipulate the structure or contents of
the objects at a low level, and convert the results back to a PDF
file. This functionality can be accessed from the command-line with
the :qpdf:ref:`--json-input`, and :qpdf:ref:`--update-from-json`
flags, or from the API using the ``QPDF::createFromJSON``, and
``QPDF::updateFromJSON`` methods. The :qpdf:ref:`--json-output` flag
changes a handful of defaults so that the resulting JSON is as close
as possible to the original input and is ready for being converted
back to PDF.

The qpdf JSON data includes unreferenced objects. This may be
addressed in a future version of qpdf. For now, that means that
certain objects that are not useful in the JSON representation are
included. This includes linearization and encryption dictionaries,
linearization hint streams, object streams, and the cross-reference
(xref) stream associated with the trailer dictionary where applicable.
For the best experience with qpdf JSON, you can run the file through
qpdf first to remove encryption, linearization, and object streams.
For example:

::

   qpdf --decrypt --object-streams=disable in.pdf out.pdf
   qpdf --json-output out.pdf out.json


.. _json-terminology:

JSON Terminology
----------------

Notes about terminology:

- In JavaScript and JSON, that thing that has keys and values is
  typically called an *object*.

- In PDF, that thing that has keys and values is typically called a
  *dictionary*. An *object* is a PDF object such as integer, real,
  boolean, null, string, array, dictionary, or stream.

- Some languages that use JSON call an *object* a *dictionary*, a
  *map*, or a *hash*.

- Sometimes, it's called on *object* if it has fixed keys and a
  *dictionary* if it has variable keys.

This manual is not entirely consistent about its use of *dictionary*
vs. *object* because sometimes one term or another is clearer in
context. Just be aware of the ambiguity when reading the manual. We
frequently use the term *dictionary* to refer to a JSON object because
of the consistency with PDF terminology, particular when referring to
a dictionary that contains information PDF objects.

.. _what-qpdf-json-is-not:

What qpdf JSON is not
---------------------

Please note that qpdf JSON offers a convenient syntax for manipulating
PDF files at a low level using JSON syntax. JSON syntax is much easier
to work with than native PDF syntax, and there are good JSON libraries
in virtually every commonly used programming language. Working with
PDF objects in JSON removes the need to worry about stream lengths,
cross reference tables, and PDF-specific representations of Unicode or
binary strings that appear outside of content streams. It does not
eliminate the need to understand the semantic structure of PDF files.
Working with qpdf JSON still requires familiarity with the PDF
specification.

In particular, qpdf JSON *does not* provide any of the following
capabilities:

- Text extraction. While you could use qpdf JSON syntax to navigate to
  a page's content streams and font structures, text within pages is
  still encoded using PDF syntax within content streams, and there is
  no assistance for text extraction.

- Reflowing text, document structure. qpdf JSON does not add any new
  information or insight into the content of PDF files. If you have a
  PDF file that lacks any structural information, qpdf JSON won't help
  you solve any of those problems.

This is what we mean when we say that JSON provides a *alternative
syntax* for working with PDF data. Semantically, it is identical to
native PDF.

.. _qpdf-json:

qpdf JSON Format
----------------

This section describes how qpdf represents PDF objects in JSON format.
It also describes how to work with qpdf JSON to create or
modify PDF files.

.. _json.objects:

qpdf JSON Object Representation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This section describes the representation of PDF objects in qpdf JSON
version 2. An example appears in :ref:`json.example`.

PDF objects are represented within the ``"qpdf"`` entry of a qpdf JSON
file. The ``"qpdf"`` entry is a two-element array. The first element
is a dictionary containing header-like information about the file such
as the PDF version. The second element is a dictionary containing all
the objects in the PDF file. We refer to this as the *objects
dictionary*.

The first element contains the following keys:

- ``"jsonversion"`` -- a number indicating the JSON version used for
  writing. This will always be ``2``.

- ``"pdfversion"`` -- a string containing PDF version as indicated in
  the PDF header (e.g. ``"1.7"``, ``"2.0"``)

- ``pushedinheritedpageresources`` -- a boolean indicating whether the
  library pushed inherited resources down to the page level. Certain
  library calls cause this to happen, and qpdf needs to know when
  reading a JSON file back in whether it should do this as it may
  cause certain objects to be renumbered. This field is ignored when
  :qpdf:ref:`--update-from-json` was not given.

- ``calledgetallpages`` -- a boolean indicating whether
  ``getAllPages`` was called prior to writing the JSON output. This
  method causes page tree repair to occur, which may renumber some
  objects (in very rare cases of corrupted page trees), so qpdf needs
  to know this information when reading a JSON file back in. This
  field is ignored when :qpdf:ref:`--update-from-json` was not given.

- ``"maxobjectid"`` -- a number indicating the object ID of the
  highest numbered object in the file. This is provided to make it
  easier for software that wants to add new objects to the file as you
  can safely start with one above that number when creating new
  objects. Note that the value of ``"maxobjectid"`` may be higher than
  the actual maximum object that appears in the input PDF since it
  takes into consideration any dangling indirect object references
  from the original file. This prevents you from unwittingly creating
  an object that doesn't exist but that is referenced, which may have
  unintended side effects. (The PDF specification explicitly allows
  dangling references and says to treat them as nulls. This can happen
  if objects are removed from a PDF file.)

The second element is the objects dictionary. Each key in the objects
dictionary is either ``"trailer"`` or a string of the form
:samp:`"obj:{O} {G} R"` where :samp:`{O}` and :samp:`{G}` are the
object and generation numbers and ``R`` is the literal string ``R``.
This is the PDF syntax for the indirect object reference prepended by
``obj:``. The value, representing the object itself, is a JSON object
whose structure is described below.

Top-level Stream Objects
  Stream objects are represented as a JSON object with the single key
  ``"stream"``. The stream object has a key called ``"dict"`` whose
  value is the stream dictionary as an object value (described below)
  with the ``"/Length"`` key omitted. Other keys are determined by the
  value for json stream data (:qpdf:ref:`--json-stream-data`, or a
  parameter of type ``qpdf_json_stream_data_e``) as follows:

  - ``none``: stream data is not represented; no other keys are
    present
    specified.

  - ``inline``: the stream data appears as a base64-encoded string as
    the value of the ``"data"`` key

  - ``file``: the stream data is written to a file, and the path to
    the file is stored in the ``"datafile"`` key. A relative path is
    interpreted as relative to the current directory when qpdf is
    invoked.

  Keys other than ``"dict"``, ``"data"``, and ``"datafile"`` are
  ignored. This is primarily for future compatibility in case a newer
  version of qpdf includes additional information.

  As with the native PDF representation, the stream data must be
  consistent with whatever filters and decode parameters are specified
  in the stream dictionary.

Top-level Non-stream Objects
  Non-stream objects are represented as a dictionary with the single
  key ``"value"``. Other keys are ignored for future compatibility.
  The value's structure is described in "Object Values" below.

  Note: in files that use object streams, the trailer "dictionary" is
  actually a stream, but in the JSON representation, the value of the
  ``"trailer"`` key is always written as a dictionary (with a
  ``"value"`` key like other non-stream objects). There will also be a
  a stream object whose key is the object ID of the cross-reference
  stream, even though this stream will generally be unreferenced. This
  makes it possible to assume ``"trailer"`` points to a dictionary
  without having to consider whether the file uses object streams or
  not. It is also consistent with how ``QPDF::getTrailer`` behaves in
  the C++ API.

Object Values
  Within ``"value"`` or ``"stream"."dict"``, PDF objects are
  represented as follows:

  - Objects of type Boolean or null are represented as JSON objects of
    the same type.

  - Objects that are numeric are represented as numeric in the JSON
    without regard to precision. Internally, qpdf stores numeric
    values as strings, so qpdf will preserve arbitrary precision
    numerical values when reading and writing JSON. It is likely that
    other JSON readers and writers will have implementation-dependent
    ways of handling numerical values that are out of range.

  - Name objects are represented as JSON strings that start with ``/``
    and are followed by the PDF name in canonical form with all PDF
    syntax resolved. For example, the name whose canonical form (per
    the PDF specification) is ``text/plain`` would be represented in
    JSON as ``"/text/plain"`` and in PDF as ``"/text#2fplain"``.

  - Indirect object references are represented as JSON strings that
    look like a PDF indirect object reference and have the form
    :samp:`"{O} {G} R"` where :samp:`{O}` and :samp:`{G}` are the
    object and generation numbers and ``R`` is the literal string
    ``R``. For example, ``"3 0 R"`` would represent a reference to the
    object with object ID 3 and generation 0.

  - PDF strings are represented as JSON strings in one of two ways:

    - ``"u:utf8-encoded-string"``: this format is used when the PDF
      string can be unambiguously represented as a Unicode string and
      contains no unprintable characters. This is the case whether the
      input string is encoded as UTF-16, UTF-8 (as allowed by PDF
      2.0), or PDF doc encoding. Strings are only represented this way
      if they can be encoded without loss of information.

    - ``"b:hex-string"``: this format is used to represent any binary
      string value that can't be represented as a Unicode string.
      ``hex-string`` must have an even number of characters that range
      from ``a`` through ``f``, ``A`` through ``F``, or ``0`` through
      ``9``.

    qpdf writes empty strings as ``"u:"``, but both ``"b:"`` and
    ``"u:"`` are valid representations of the empty string.

    There is full support for UTF-16 surrogate pairs. Binary strings
    encoded with ``"b:..."`` are the internal PDF representations.
    As such, the following are equivalent:

    - ``"u:\ud83e\udd54"`` -- representation of U+1F954 as a surrogate
      pair in JSON syntax

    - ``"b:FEFFD83EDD54"`` -- representation of U+1F954 as the bytes
      of a UTF-16 string in PDF syntax with the leading ``FEFF``
      indicating UTF-16

    - ``"b:efbbbff09fa594"`` -- representation of U+1F954 as the
      bytes of a UTF-8 string in PDF syntax (as allowed by PDF 2.0)
      with the leading ``EF``, ``BB``, ``BF`` sequence (which is just
      UTF-8 encoding of ``FEFF``).

    - A JSON string whose contents are ``u:`` followed by the UTF-8
      representation of U+1F954. This is the potato emoji.
      Unfortunately, I am not able to render it in the PDF version
      of this manual.

  - PDF arrays are represented as JSON arrays of objects as described
    above

  - PDF dictionaries are represented as JSON objects whose keys are
    the string representations of names and whose values are
    representations of PDF objects.

Note that writing JSON output is done by ``QPDF``, not ``QPDFWriter``.
As such, none of the things ``QPDFWriter`` does apply. This includes
recompression of streams, renumbering of objects, removal of
unreferenced objects, encryption, decryption, linearization, QDF
mode, etc. See :ref:`rewriting` for a more in-depth discussion. This
has a few noteworthy implications:

- Decryption is handled transparently by qpdf. As there are no QPDF
  APIs, even internal to the library, that allow retrieval of
  encrypted data in its raw, encrypted form, qpdf JSON always includes
  decrypted data. It is possible that a future version of qpdf may
  allow access to raw, encrypted string and stream data.

- Objects that are related to a PDF file's structure, rather than its
  content, are included in the JSON output, even though they are not
  particularly useful. In a future version of qpdf, this may be fixed,
  and the :qpdf:ref:`--preserve-unreferenced` flag may be able to be
  used to get the existing behavior. For now, to avoid this, run the
  file through ``qpdf --decrypt --object-streams=disable in.pdf
  out.pdf`` to generate a new PDF file that contains no unreferenced
  or structural objects.

  - Linearized PDF files include a linearization dictionary which is not
    referenced from any other object and which references the
    linearization hint stream by offset. The JSON from a linearized PDF
    file contains both of these objects, even though they are not useful
    in the JSON. Offset information is not represented in the JSON, so
    there's no way to find the linearization hint stream from the
    JSON. If a new PDF is created from JSON that was written, the
    objects will be read back in but will just be unreferenced objects
    that will be ignored by ``QPDFWriter`` when the file is rewritten.

  - The JSON from a file with object streams will include the original
    object stream and will also include all the objects in the stream
    as top-level objects.

  - In files with object streams, the trailer "dictionary" is a
    stream. In qpdf JSON files, the ``"trailer"`` key will contain a
    dictionary with all the keys in it relating to the stream, and the
    stream will also appear as an unreferenced object.

  - Encrypted files are decrypted, but the encryption dictionary still
    appears in the JSON output.

.. _json.example:

qpdf JSON Example
~~~~~~~~~~~~~~~~~

The JSON below shows an example of a simple PDF file represented in
qpdf JSON format.

.. code-block:: json

   {
     "qpdf": [
       {
         "jsonversion": 2,
         "pdfversion": "1.3",
         "pushedinheritedpageresources": false,
         "calledgetallpages": false,
         "maxobjectid": 6
       },
       {
         "obj:1 0 R": {
           "value": {
             "/Pages": "3 0 R",
             "/Type": "/Catalog"
           }
         },
         "obj:2 0 R": {
           "value": {
             "/Author": "u:Digits of π",
             "/CreationDate": "u:D:20220731155308-05'00'",
             "/Creator": "u:A person typing in Emacs",
             "/Keywords": "u:potato, example",
             "/ModDate": "u:D:20220731155308-05'00'",
             "/Producer": "u:qpdf",
             "/Subject": "u:Example",
             "/Title": "u:Something potato-related"
           }
         },
         "obj:3 0 R": {
           "value": {
             "/Count": 1,
             "/Kids": [
               "4 0 R"
             ],
             "/Type": "/Pages"
           }
         },
         "obj:4 0 R": {
           "value": {
             "/Contents": "5 0 R",
             "/MediaBox": [
               0,
               0,
               612,
               792
             ],
             "/Parent": "3 0 R",
             "/Resources": {
               "/Font": {
                 "/F1": "6 0 R"
               }
             },
             "/Type": "/Page"
           }
         },
         "obj:5 0 R": {
           "stream": {
             "data": "eJxzCuFSUNB3M1QwMlEISQOyzY2AyEAhJAXI1gjIL0ksyddUCMnicg3hAgDLAQnI",
             "dict": {
               "/Filter": "/FlateDecode"
             }
           }
         },
         "obj:6 0 R": {
           "value": {
             "/BaseFont": "/Helvetica",
             "/Encoding": "/WinAnsiEncoding",
             "/Subtype": "/Type1",
             "/Type": "/Font"
           }
         },
         "trailer": {
           "value": {
             "/ID": [
               "b:98b5a26966fba4d3a769b715b2558da6",
               "b:6bea23330e0b9ff0ddb47b6757fb002e"
             ],
             "/Info": "2 0 R",
             "/Root": "1 0 R",
             "/Size": 7
           }
         }
       }
     ]
   }

.. _json.input:

qpdf JSON Input
~~~~~~~~~~~~~~~

The qpdf JSON output can be used in two different ways:

- By using the :qpdf:ref:`--json-input` flag or calling
  ``QPDF::createFromJSON`` in place of ``QPDF::processFile``, a qpdf
  JSON file can be used in place of a PDF file as the input to qpdf.

- By using the :qpdf:ref:`--update-from-json` flag or calling
  ``QPDF::updateFromJSON`` on an initialized ``QPDF`` object, a qpdf
  JSON file can be used to apply changes to an existing ``QPDF``
  object. That ``QPDF`` object can have come from any source including
  a PDF file, a qpdf JSON file, or the result of any other process
  that results in a valid, initialized ``QPDF`` object.

Here are some important things to know about qpdf JSON input.

- When a qpdf JSON file is used as the primary input file, it must be
  complete. This means

  - A JSON version number must be specified with the ``"jsonversion"``
    key in the first array element

  - A PDF version number must be specified with the ``"pdfversion"``
    key in the first array element

  - Stream data must be present for all streams

  - The trailer dictionary must be present, though only the
    ``"/Root"`` key is required.

- Certain fields from the input are ignored whether creating or
  updating from a JSON file:

  - ``"maxobjectid"`` is ignored, so it is not necessary to update it
    when adding new objects.

  - ``"/Length"`` is ignored in all stream dictionaries. qpdf doesn't
    put it there when it creates JSON output, and it is not necessary
    to add it.

  - ``"/Size"`` is ignored if it appears in a trailer dictionary as
    that is always recomputed by ``QPDFWriter``.

  - Unknown keys at the top level of the file, within ``"qpdf"``, and
    at the top level of each individual PDF object (inside the
    dictionary that has the ``"value"`` or ``"stream"`` key) and
    directly within ``"stream"`` are ignored for future compatibility.
    This includes other top-level keys generated by ``qpdf`` itself
    (such as ``"pages"``). As such, those keys don't have to be
    consistent with the ``"qpdf"`` key if modifying a JSON file for
    conversion back to PDF. If you wish to store application-specific
    metadata, you can do so by adding a key whose name starts with
    ``x-``. qpdf is guaranteed not to add any of its own keys that
    starts with ``x-``. Note that any ``"version"`` key at the top
    level is ignored. The JSON version is obtained from the
    ``"jsonversion"`` key of the first element of the ``"qpdf"``
    field.

- The values of ``"calledgetallpages"`` and
  ``"pushedinheritedpageresources"`` are ignored when creating a file.
  When updating a file, they treated as ``false`` if omitted.

- When qpdf reads a PDF file, the internal object numbers are always
  preserved. However, when qpdf writes a file using ``QPDFWriter``,
  ``QPDFWriter`` does its own numbering and, in general, does not
  preserve input object numbers. That means that a qpdf JSON file that
  is used to update an existing PDF must have object numbers that
  match the input file it is modifying. In practical terms, this means
  that you can't use a JSON file created from one PDF file to modify
  the *output of running qpdf on that file*.

  To put this more concretely, the following is valid:

  ::

    qpdf --json-output in.pdf pdf.json
    # edit pdf.json
    qpdf in.pdf out.pdf --update-from-json=pdf.json

  The following will produce unpredictable and probably incorrect
  results because ``out.pdf`` won't have the same object numbers as
  ``pdf.json`` and ``in.pdf``.

  ::

    qpdf --json-output in.pdf pdf.json
    # edit pdf.json
    qpdf in.pdf out.pdf --update-from-json=pdf.json
    # edit pdf.json again
    # Don't do this
    qpdf out.pdf out2.pdf --update-from-json=pdf.json

- When updating from a JSON file (:qpdf:ref:`--update-from-json`,
  ``QPDF::updateFromJSON``), existing objects are updated in place.
  This has the following implications:

  - If the object you are updating is a stream, you may omit both
    ``"data"`` and ``"datafile"``. In that case the original stream
    data is preserved. You must always provide a stream dictionary,
    but it may be empty. Note that an empty stream dictionary will
    clear the old dictionary. There is no way to indicate that an old
    stream dictionary should be left alone, so if your intention is to
    replace the stream data and preserve the dictionary, the original
    dictionary must appear in the JSON file.

  - You can change one object type to another object type including
    replacing a stream with a non-stream or a non-stream with a
    stream. If you replace a non-stream with a stream, you must
    provide data for the stream.

  - Objects that you do not wish to modify can be omitted from the
    JSON. That includes the trailer. That means you can use the output
    of a qpdf JSON file that was written using
    :qpdf:ref:`--json-object` to have it include only the objects you
    intend to modify.

  - You can omit the ``"pdfversion"`` key. The input PDF version will
    be preserved.

.. _json.workflow-cli:

qpdf JSON Workflow: CLI
~~~~~~~~~~~~~~~~~~~~~~~

This section includes a few examples of using qpdf JSON.

- Convert a PDF file to JSON format, edit the JSON, and convert back
  to PDF. This is an alternative to using QDF mode (see :ref:`qdf`) to
  modify PDF files in a text editor. Each method has its own
  advantages and disadvantages.

  ::

     qpdf --json-output in.pdf pdf.json
     # edit pdf.json
     qpdf --json-input pdf.json out.pdf

- Extract only a specific object into a JSON file, modify the object
  in JSON, and use the modified object to update the original PDF. In
  this case, we're editing object 4, whatever that may happen to be.
  You would have to know through some other means which object you
  wanted to edit, such as by looking at other JSON output or using a
  tool (possibly but not necessarily qpdf) to identify the object.

  ::

     qpdf --json-output in.pdf pdf.json --json-object=4,0
     # edit pdf.json
     qpdf in.pdf --update-from-json=pdf.json out.pdf

  Rather than using :qpdf:ref:`--json-object` as in the above example,
  you could edit the JSON file to remove the objects you didn't need.
  You could also just leave them there, though the update process
  would be slower.

  You could also add new objects to a file by adding them to
  ``pdf.json``. Just be sure the object number doesn't conflict with
  an existing object. The ``"maxobjectid"`` field in the original
  output can help with this. You don't have to update it if you add
  objects as it is ignored when the file is read back in.

- Use :qpdf:ref:`--json-input` and :qpdf:ref:`--json-output` together
  to demonstrate preservation of object numbers. In this example,
  ``a.json`` and ``b.json`` will have the same objects and object
  numbers. The files may not be identical since strings may be
  normalized, fields may appear in a different order, etc. However
  ``b.json`` and ``c.json`` are probably identical.

  ::

     qpdf --json-output in.pdf a.json
     qpdf --json-input --json-output a.json b.json
     qpdf --json-input --json-output b.json c.json


.. _json.workflow-api:

qpdf JSON Workflow: API
~~~~~~~~~~~~~~~~~~~~~~~

Everything that can be done using the qpdf CLI can be done using the
C++ API. See comments in :file:`QPDF.hh` for ``writeJSON``,
``createFromJSON``, and ``updateFromJSON`` for details.

.. _json-guarantees:

JSON Compatibility Guarantees
-----------------------------

The qpdf JSON representation includes a JSON serialization of the raw
objects in the PDF file as well as some computed information in a more
easily extracted format. QPDF provides some guarantees about its JSON
format. These guarantees are designed to simplify the experience of a
developer working with the JSON format.

Compatibility
   The top-level JSON object is a dictionary (JSON "object"). The JSON
   output contains various nested dictionaries and arrays. With the
   exception of dictionaries that are populated by the fields of
   PDF objects from the file, all instances of a dictionary are
   guaranteed to have exactly the same keys.

   The top-level JSON structure contains a ``"version"`` key whose
   value is simple integer. The value of the ``version`` key will be
   incremented if a non-compatible change is made. A non-compatible
   change would be any change that involves removal of a key, a change
   to the format of data pointed to by a key, or a semantic change
   that requires a different interpretation of a previously existing
   key. Note that, starting with version 2, the JSON version also
   appears in the ``"jsonversion"`` field of the first element of
   ``"qpdf"`` field.

   Within a specific qpdf JSON version, future versions of qpdf are
   free to add additional keys but not to remove keys or change the
   type of object that a key points to. That means that consumers of
   qpdf JSON should ignore keys they don't know about.

Documentation
   The :command:`qpdf` command can be invoked with the
   :qpdf:ref:`--json-help` option. This will output a JSON
   structure that has the same structure as the JSON output that qpdf
   generates, except that each field in the help output is a description
   of the corresponding field in the JSON output. The specific
   guarantees are as follows:

   - A dictionary in the help output means that the corresponding
     location in the actual JSON output is also a dictionary with
     exactly the same keys; that is, no keys present in help are
     absent in the real output, and no keys will be present in the
     real output that are not in help. It is possible for a key to be
     present and have a value that is explicitly ``null``. As a
     special case, if the dictionary has a single key whose name
     starts with ``<`` and ends with ``>``, it means that the JSON
     output is a dictionary that can have any value as a key. This is
     used for cases in which the keys of the dictionary are things
     like object IDs.

   - A string in the help output is a description of the item that
     appears in the corresponding location of the actual output. The
     corresponding output can have any value including ``null``.

   - A single-element array in the help output indicates that the
     corresponding location in the actual output is either a single
     item or is an array of any length. The single item or each
     element of the array has whatever format is implied by the single
     element of the help output's array.

   - A multi-element array in the help output indicates that the
     corresponding location in the actual output is an array of the
     same length. Each element of the output array has whatever format
     is implied by the corresponding element of the help output's
     array.

   For example, the help output indicates includes a ``"pagelabels"``
   key whose value is an array of one element. That element is a
   dictionary with keys ``"index"`` and ``"label"``. In addition to
   describing the meaning of those keys, this tells you that the actual
   JSON output will contain a ``pagelabels`` array, each of whose
   elements is a dictionary that contains an ``index`` key, a ``label``
   key, and no other keys.

Directness and Simplicity
   The JSON output contains the value of every object in the file, but
   it also contains some summary data. This is analogous to how qpdf's
   library interface works. The summary data is similar to the helper
   functions in that it allows you to look at certain aspects of the
   PDF file without having to understand all the nuances of the PDF
   specification, while the raw objects allow you to mine the PDF for
   anything that the higher-level interfaces are lacking. It is
   especially useful to create a JSON file with the ``"pages"`` and
   ``"qpdf"`` keys and to use the ``"pages"`` information to find a
   page rather than navigating the pages tree manually. This can be
   done safely, and changes can made to the objects dictionary without
   worrying about keeping ``"pages"`` up to date since it is ignored
   when reading the file back in.

.. _json.considerations:

JSON: Special Considerations
----------------------------

For the most part, the built-in JSON help tells you everything you need
to know about the JSON format, but there are a few non-obvious things to
be aware of:

- If a PDF file has certain types of errors in its pages tree (such as
  page objects that are direct or multiple pages sharing the same
  object ID), qpdf will automatically repair the pages tree. If you
  specify ``"qpdf"`` (or, with qpdf JSON version 1, ``"objects"`` or
  ``"objectinfo"``) without any other keys, you will see the original
  pages tree without any corrections. If you specify any of keys that
  require page tree traversal (for example, ``"pages"``,
  ``"outlines"``, or ``"pagelabel"``), then ``"qpdf"`` (and
  ``"objects"`` and ``"objectinfo"``) will show the repaired page
  tree so that object references will be consistent throughout the
  file. You can tell if this has happened by looking at the
  ``"calledgetallpages"`` and ``"pushedinheritedpageresources"``
  fields in the first element of the ``"qpdf"`` array.

- While qpdf guarantees that keys present in the help will be present
  in the output, those fields may be null or empty if the information
  is not known or absent in the file. Also, if you specify
  :qpdf:ref:`--json-key`, the keys that are not listed
  will be excluded entirely except for those that
  :qpdf:ref:`--json-help` says are always present.

- In a few places, there are keys with names containing
  ``pageposfrom1``. The values of these keys are null or an integer. If
  an integer, they point to a page index within the file numbering from
  1. Note that JSON indexes from 0, and you would also use 0-based
  indexing using the API. However, 1-based indexing is easier in this
  case because the command-line syntax for specifying page ranges is
  1-based. If you were going to write a program that looked through
  the JSON for information about specific pages and then use the
  command-line to extract those pages, 1-based indexing is easier.
  Besides, it's more convenient to subtract 1 in a real programming
  language than it is to add 1 in shell code.

- The image information included in the ``page`` section of the JSON
  output includes the key ``"filterable"``. Note that the value of
  this field may depend on the :qpdf:ref:`--decode-level` that you
  invoke qpdf with. The JSON output includes a top-level key
  ``"parameters"`` that indicates the decode level that was used for
  computing whether a stream was filterable. For example, jpeg images
  will be shown as not filterable by default, but they will be shown
  as filterable if you run :command:`qpdf --json
  --decode-level=all`.

- The ``encrypt`` key's values will be populated for non-encrypted
  files. Some values will be null, and others will have values that
  apply to unencrypted files.

- The qpdf library itself never loads an entire PDF into memory. This
  remains true for PDF files represented in JSON format. In general,
  qpdf will hold the entire object structure in memory once a file has
  been fully read (objects are loaded into memory lazily but stay
  there once loaded), but it will never have more than two copies of a
  stream in memory at once. That said, if you ask qpdf to write JSON
  to memory, it will do so, so be careful about this if you are
  working with very large PDF files. There is nothing in the qpdf
  library itself that prevents working with PDF files much larger than
  available system memory. qpdf can both read and write such files in
  JSON format. If you need to work with a PDF file's json
  representation in memory, it is recommended that you use either
  ``none`` or ``file`` as the argument to
  :qpdf:ref:`--json-stream-data`, or if using the API, use
  ``qpdf_sj_none`` or ``pdf_sj_file`` as the json stream data value.
  If using ``none``, you can use other means to obtain the stream
  data.

.. _json-v2-changes:

Changes from JSON v1 to v2
--------------------------

The following changes were made to qpdf's JSON output format for
version 2.

- The representation of objects has changed. For details, see
  :ref:`json.objects`.

  - The representation of strings is now unambiguous for all strings.
    Strings a prefixed with either ``u:`` for Unicode strings or
    ``b:`` for byte strings.

  - Names are shown in qpdf's canonical form rather than in PDF
    syntax. (Example: the PDF-syntax name ``/text#2fplain`` appeared
    as ``"/text#2fplain"`` in v1 but appears as ``"/text/plain"`` in
    v2.

  - The top-level representation of an object in ``"objects"`` is a
    dictionary containing either a ``"value"`` key or a ``"stream"``
    key, making it possible to distinguish streams from other objects.

- The ``"objectinfo"`` and ``"objects"`` keys have been removed in
  favor of a representation in ``"qpdf"`` that includes header
  information and differentiates between a stream and other kinds of
  objects. In v1, it was not possible to tell a stream from a
  dictionary within ``"objects"``, and the PDF version was not
  captured at all.

- Within the objects dictionary, keys are now :samp:`"obj:{O} {G} R"`
  where :samp:`{O}` and :samp:`{G}` are the object and generation
  number. ``"trailer"`` remains the key for the trailer dictionary. In
  v1, the ``obj:`` prefix was not present. The rationale for this
  change is as follows:

  - Having a unique prefix (``obj:``) makes it much easier to search
    in the JSON file for the definition of an object

  - Having the key still contain ``O G R`` makes it much easier to
    construct the key from an indirect reference. You just have to
    prepend ``obj:``. There is no need to parse the indirect object
    reference.

- In the ``"encrypt"`` object, the ``"modifyannotations"`` was
  misspelled as ``"moddifyannotations"`` in v1. This has been
  corrected.

Motivation for qpdf JSON version 2
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

qpdf JSON version 2 was created to make it possible to manipulate PDF
files using JSON syntax instead of native PDF syntax. This makes it
possible to make low-level updates to PDF files from just about any
programming language or even to do so from the command-line using
tools like ``jq`` or any editor that's capable of working with JSON
files. There were several limitations of JSON format version 1 that
made this impossible:

- Strings, names, and indirect object references in the original PDF
  file were all converted to strings in the JSON representation. For
  casual human inspection, this was fine, but in the general case,
  there was no way to tell the difference between a string that looked
  like a name or indirect object reference from an actual name or
  indirect object reference.

- PDF strings were not unambiguously represented in the JSON format.
  The way qpdf JSON v1 represented a string was to try to convert the
  string to UTF-8. This was done by assuming a string that was not
  explicitly marked as Unicode was encoded in PDF doc encoding. The
  problem is that there is not a perfect bidirectional mapping between
  Unicode and PDF doc encoding, so if a binary string happened to
  contain characters that couldn't be bidirectionally mapped, there
  would be no way to get back to the original PDF string. Even when
  possible, trying to map from the JSON representation of a binary
  string back to the original string required knowledge of the mapping
  between PDF doc encoding and Unicode.

- There was no representation of stream data. If you wanted to extract
  stream data, you could use :qpdf:ref:`--show-object`, so this wasn't
  that important for inspection, but it was a blocker for being able
  to go from JSON back to PDF. qpdf JSON version 2 allows stream data
  to be included inline as base64-encoded data. There is also an
  option to write all stream data to external files, which makes it
  possible to work with very large PDF files in JSON format even with
  tools that try to read the entire JSON structure into memory.

- The PDF version from PDF header was not represented in qpdf JSON v1.
