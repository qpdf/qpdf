.. _ref.using-library:

Using the QPDF Library
======================

.. _ref.using.from-cxx:

Using QPDF from C++
-------------------

The source tree for the qpdf package has an
:file:`examples` directory that contains a few
example programs. The :file:`qpdf/qpdf.cc` source
file also serves as a useful example since it exercises almost all of
the qpdf library's public interface. The best source of documentation on
the library itself is reading comments in
:file:`include/qpdf/QPDF.hh`,
:file:`include/qpdf/QPDFWriter.hh`, and
:file:`include/qpdf/QPDFObjectHandle.hh`.

All header files are installed in the
:file:`include/qpdf` directory. It is recommend that
you use ``#include <qpdf/QPDF.hh>`` rather than adding
:file:`include/qpdf` to your include path.

When linking against the qpdf static library, you may also need to
specify ``-lz -ljpeg`` on your link command. If your system understands
how to read libtool :file:`.la` files, this may not
be necessary.

The qpdf library is safe to use in a multithreaded program, but no
individual ``QPDF`` object instance (including ``QPDF``,
``QPDFObjectHandle``, or ``QPDFWriter``) can be used in more than one
thread at a time. Multiple threads may simultaneously work with
different instances of these and all other QPDF objects.

.. _ref.using.other-languages:

Using QPDF from other languages
-------------------------------

The qpdf library is implemented in C++, which makes it hard to use
directly in other languages. There are a few things that can help.

"C"
   The qpdf library includes a "C" language interface that provides a
   subset of the overall capabilities. The header file
   :file:`qpdf/qpdf-c.h` includes information about
   its use. As long as you use a C++ linker, you can link C programs
   with qpdf and use the C API. For languages that can directly load
   methods from a shared library, the C API can also be useful. People
   have reported success using the C API from other languages on Windows
   by directly calling functions in the DLL.

Python
   A Python module called
   `pikepdf <https://pypi.org/project/pikepdf/>`__ provides a clean and
   highly functional set of Python bindings to the qpdf library. Using
   pikepdf, you can work with PDF files in a natural way and combine
   qpdf's capabilities with other functionality provided by Python's
   rich standard library and available modules.

Other Languages
   Starting with version 8.3.0, the :command:`qpdf`
   command-line tool can produce a JSON representation of the PDF file's
   non-content data. This can facilitate interacting programmatically
   with PDF files through qpdf's command line interface. For more
   information, please see :ref:`ref.json`.

.. _ref.unicode-files:

A Note About Unicode File Names
-------------------------------

When strings are passed to qpdf library routines either as ``char*`` or
as ``std::string``, they are treated as byte arrays except where
otherwise noted. When Unicode is desired, qpdf wants UTF-8 unless
otherwise noted in comments in header files. In modern UNIX/Linux
environments, this generally does the right thing. In Windows, it's a
bit more complicated. Starting in qpdf 8.4.0, passwords that contain
Unicode characters are handled much better, and starting in qpdf 8.4.1,
the library attempts to properly handle Unicode characters in filenames.
In particular, in Windows, if a UTF-8 encoded string is used as a
filename in either ``QPDF`` or ``QPDFWriter``, it is internally
converted to ``wchar_t*``, and Unicode-aware Windows APIs are used. As
such, qpdf will generally operate properly on files with non-ASCII
characters in their names as long as the filenames are UTF-8 encoded for
passing into the qpdf library API, but there are still some rough edges,
such as the encoding of the filenames in error messages our CLI output
messages. Patches or bug reports are welcome for any continuing issues
with Unicode file names in Windows.
