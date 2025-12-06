.. _inspection:

Appendix 1: Inspection Mode
===========================

.. _inspection.intro:

Introduction
------------

This appendix provides further detail about inspection mode.

Inspection mode is primarily intended for manual investigation and
analysis of PDF files. API stability is not a major concern and
the details of what is and is not supported may change from version to
version. Please check this section for updates with each new release of qpdf.

**NOTE** Updates in the release notes may not apply to inspection mode
despite wording such as "unconditionally" or "always." Look out for
footnotes indicating that statements don't apply to inspection mode.

**NOTE** Warning messages are not adapted for inspection mode. In
particular, messages that state that an issue has been repaired may
not apply.

.. _inspection.supported:

Supported Operations
--------------------

The only supported operations in inspection mode are the retrieval of
objects with methods such as ``QPDF::getObject``, ``QPDF::getRoot`` and
``QPDF::getTrailer`` and the use of accessor methods of
``QPDFObjectHandle`` that do not require any modification of the object,
including the ``unparse`` and ``writeJSON`` methods.

Other operations not specifically flagged as unsupported may also work,
but there are no guarantees and they may cause crashes or hang.

.. _inspection.unsupported:

Unsupported Operations
----------------------

The following operations are explicitly unsupported in inspection mode:

- use of any document and object helper methods
- use of any linearization methods, including check methods
- creation of object streams
- operations involving more than one ``QPDF`` object, such as copying objects
  between documents

If any of these operations are desired, inspection mode should be used
to manually repair the files sufficiently to allow qpdf to successfully repair
them in normal mode.

.. _inspection.disabled:

Disabled Operations
-------------------

The following operations are disabled and will throw a logic error:

- retrieval of a document helper using static ``get`` methods

.. _inspection.disabled_repairs:

Disabled Automatic Repairs
--------------------------

The following automatic repairs are disabled in inspection mode:

- root object:
    - repairing missing or incorrect ``/Type`` attributes

