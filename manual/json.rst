.. _json:

QPDF JSON
=========

.. _json-overview:

Overview
--------

Beginning with qpdf version 8.3.0, the :command:`qpdf`
command-line program can produce a JSON representation of the
non-content data in a PDF file. It includes a dump in JSON format of all
objects in the PDF file excluding the content of streams. This JSON
representation makes it very easy to look in detail at the structure of
a given PDF file, and it also provides a great way to work with PDF
files programmatically from the command-line in languages that can't
call or link with the qpdf library directly. Note that stream data can
be extracted from PDF files using other qpdf command-line options.

.. _json-guarantees:

JSON Guarantees
---------------

The qpdf JSON representation includes a JSON serialization of the raw
objects in the PDF file as well as some computed information in a more
easily extracted format. QPDF provides some guarantees about its JSON
format. These guarantees are designed to simplify the experience of a
developer working with the JSON format.

Compatibility
   The top-level JSON object output is a dictionary. The JSON output
   contains various nested dictionaries and arrays. With the exception
   of dictionaries that are populated by the fields of objects from the
   file, all instances of a dictionary are guaranteed to have exactly
   the same keys. Future versions of qpdf are free to add additional
   keys but not to remove keys or change the type of object that a key
   points to. The qpdf program validates this guarantee, and in the
   unlikely event that a bug in qpdf should cause it to generate data
   that doesn't conform to this rule, it will ask you to file a bug
   report.

   The top-level JSON structure contains a "``version``" key whose value
   is simple integer. The value of the ``version`` key will be
   incremented if a non-compatible change is made. A non-compatible
   change would be any change that involves removal of a key, a change
   to the format of data pointed to by a key, or a semantic change that
   requires a different interpretation of a previously existing key. A
   strong effort will be made to avoid breaking compatibility.

Documentation
   The :command:`qpdf` command can be invoked with the
   :qpdf:ref:`--json-help` option. This will output a JSON
   structure that has the same structure as the JSON output that qpdf
   generates, except that each field in the help output is a description
   of the corresponding field in the JSON output. The specific
   guarantees are as follows:

   - A dictionary in the help output means that the corresponding
     location in the actual JSON output is also a dictionary with
     exactly the same keys; that is, no keys present in help are absent
     in the real output, and no keys will be present in the real output
     that are not in help. As a special case, if the dictionary has a
     single key whose name starts with ``<`` and ends with ``>``, it
     means that the JSON output is a dictionary that can have any keys,
     each of which conforms to the value of the special key. This is
     used for cases in which the keys of the dictionary are things like
     object IDs.

   - A string in the help output is a description of the item that
     appears in the corresponding location of the actual output. The
     corresponding output can have any format.

   - An array in the help output always contains a single element. It
     indicates that the corresponding location in the actual output is
     also an array, and that each element of the array has whatever
     format is implied by the single element of the help output's
     array.

   For example, the help output indicates includes a "``pagelabels``"
   key whose value is an array of one element. That element is a
   dictionary with keys "``index``" and "``label``". In addition to
   describing the meaning of those keys, this tells you that the actual
   JSON output will contain a ``pagelabels`` array, each of whose
   elements is a dictionary that contains an ``index`` key, a ``label``
   key, and no other keys.

Directness and Simplicity
   The JSON output contains the value of every object in the file, but
   it also contains some processed data. This is analogous to how qpdf's
   library interface works. The processed data is similar to the helper
   functions in that it allows you to look at certain aspects of the PDF
   file without having to understand all the nuances of the PDF
   specification, while the raw objects allow you to mine the PDF for
   anything that the higher-level interfaces are lacking.

.. _json.limitations:

Limitations of JSON Representation
----------------------------------

There are a few limitations to be aware of with the JSON structure:

- Strings, names, and indirect object references in the original PDF
  file are all converted to strings in the JSON representation. In the
  case of a "normal" PDF file, you can tell the difference because a
  name starts with a slash (``/``), and an indirect object reference
  looks like ``n n R``, but if there were to be a string that looked
  like a name or indirect object reference, there would be no way to
  tell this from the JSON output. Note that there are certain cases
  where you know for sure what something is, such as knowing that
  dictionary keys in objects are always names and that certain things
  in the higher-level computed data are known to contain indirect
  object references.

- The JSON format doesn't support binary data very well. Mostly the
  details are not important, but they are presented here for
  information. When qpdf outputs a string in the JSON representation,
  it converts the string to UTF-8, assuming usual PDF string semantics.
  Specifically, if the original string is UTF-16, it is converted to
  UTF-8. Otherwise, it is assumed to have PDF doc encoding, and is
  converted to UTF-8 with that assumption. This causes strange things
  to happen to binary strings. For example, if you had the binary
  string ``<038051>``, this would be output to the JSON as ``\u0003•Q``
  because ``03`` is not a printable character and ``80`` is the bullet
  character in PDF doc encoding and is mapped to the Unicode value
  ``2022``. Since ``51`` is ``Q``, it is output as is. If you wanted to
  convert back from here to a binary string, would have to recognize
  Unicode values whose code points are higher than ``0xFF`` and map
  those back to their corresponding PDF doc encoding characters. There
  is no way to tell the difference between a Unicode string that was
  originally encoded as UTF-16 or one that was converted from PDF doc
  encoding. In other words, it's best if you don't try to use the JSON
  format to extract binary strings from the PDF file, but if you really
  had to, it could be done. Note that qpdf's
  :qpdf:ref:`--show-object` option does not have this
  limitation and will reveal the string as encoded in the original
  file.

.. _json.considerations:

JSON: Special Considerations
----------------------------

For the most part, the built-in JSON help tells you everything you need
to know about the JSON format, but there are a few non-obvious things to
be aware of:

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
  1-based. If you were going to write a program that looked through the
  JSON for information about specific pages and then use the
  command-line to extract those pages, 1-based indexing is easier.
  Besides, it's more convenient to subtract 1 from a program in a real
  programming language than it is to add 1 from shell code.

- The image information included in the ``page`` section of the JSON
  output includes the key "``filterable``". Note that the value of this
  field may depend on the :qpdf:ref:`--decode-level` that
  you invoke qpdf with. The JSON output includes a top-level key
  "``parameters``" that indicates the decode level used for computing
  whether a stream was filterable. For example, jpeg images will be
  shown as not filterable by default, but they will be shown as
  filterable if you run :command:`qpdf --json
  --decode-level=all`.

- The ``encrypt`` key's values will be populated for non-encrypted
  files. Some values will be null, and others will have values that
  apply to unencrypted files.
