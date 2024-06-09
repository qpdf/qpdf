.. _overview:

What is qpdf?
=============

qpdf is a program and C++ library for structural, content-preserving
transformations on PDF files. qpdf's website is located at
https://qpdf.sourceforge.io/. qpdf's source code is hosted on github
at https://github.com/qpdf/qpdf. You can find the latest version of
this documentation at https://qpdf.readthedocs.io/.

qpdf provides many useful capabilities to developers of PDF-producing
software or for people who just want to look at the innards of a PDF
file to learn more about how they work. With qpdf, it is possible to
copy objects from one PDF file into another and to manipulate the list
of pages in a PDF file. This makes it possible to merge and split PDF
files. The qpdf library also makes it possible for you to create PDF
files from scratch. In this mode, you are responsible for supplying
all the contents of the file, while the qpdf library takes care of all
the syntactical representation of the objects, creation of cross
references tables and, if you use them, object streams, encryption,
linearization, and other syntactic details. You are still responsible
for generating PDF content on your own.

qpdf has been designed with very few external dependencies, and it is
intentionally very lightweight. qpdf is *not* a PDF content creation
library, a PDF viewer, or a program capable of converting PDF into other
formats. In particular, qpdf knows nothing about the semantics of PDF
content streams. If you are looking for something that can do that, you
should look elsewhere. However, once you have a valid PDF file, qpdf can
be used to transform that file in ways that perhaps your original PDF
creation tool can't handle. For example, many programs generate simple PDF
files but can't password-protect them, web-optimize them, or perform
other transformations of that type.

This documentation aims to be comprehensive, but there is also a `wiki
<https://github.com/qpdf/qpdf/wiki>`__ for less polished material and
ongoing work.
