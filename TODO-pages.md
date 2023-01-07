# Pages

This file contains plans and notes regarding implementing of the "pages epic." The pages epic consists of the following features:
* Proper handling of document-level features when splitting and merging documents
* More flexible aways of selecting pages from one or more documents
* More flexible ways of organizing pages, such as n-up, booklet generation ("signatures", as in what `psbook` does), scaling, and more control over overlay and underlay regarding scale and position
* Insertion of blank pages

# To-do list

* Go through all issues marked with the `pages` label and ensure that any ideas are represented here. Keep a list with mappings back to the issue number.
* When ready, open a discussion ticket.
* Flesh out an implementation plan.

# Supporting Document-level Features

qpdf needs full support for document-level features like article threads, outlines, etc. There is no support for some things and partial support for others. See notes below for a comprehensive list.

Most likely, this will be done by creating DocumentHelper and ObjectHelper classes.

It will be necessary not only to read information about these structures from a single PDF file as the existing document helpers do but also to reconstruct or update these based on modifications to the pages in a file. I'm not sure how to do that, but one idea would be to allow a document helper to register a callback with QPDFPageDocumentHelper that notifies it when a page is added or removed. This may be able to take other parameters such as a document helper from a foreign file.

Since these operations can be expensive, there will need to be a way to opt in and out. The default (to be clearly documented) should be that all supported document-level constructs are preserved. That way, as new features are added, changes to the output of previous operations to include information that was previously omitted will not constitute a non-backward compatible change that requires a major version bump. This will be a default for the API when using the higher-level page assemebly API (below) as well as the CLI.

There will also need to be some kind of support for features that are document-level and not tied to any pages, such as (sometimes) embedded files. When splitting/merging files, there needs to be a way to specify what should happen with those things. Perhaps the default here should be that these are preserved from files from which all pages are selected. For some things, like viewer preferences, it may make sense to take them from the first file.

# Page Assembly (page selection)

In addition to the existing numeric ranges of page numbers, page selection could be driven by document-level features like the outlines hierarchy or article threads. There have been a lot of suggestions about this in various tickets. There will need to be some kind of page manipulation class with configuration options. I'm thinking something similar to QPDFJob, where you construct a class and then call a bunch of methods to configure it, including the ability to configure with JSON. Several suggestions have been made in issues, which I will go through and distill into a list. Off hand, some ideas include being able to split based on explicit chunks and being able to do all pages except a list of pages.

For CLI, I'm probably going to have it take a JSON blob or JSON file on the CLI rather than having some absurd way of doing it with arguments (people have a lot of trouble with --pages as it is). See TODO for a feature on command-line/job JSON support for JSON specification arguments.

There are some other things, like allowing n-up and genearlizing overlay/underlay to allow different placement and scaling options, that I think may also be in scope.

# Scaling/Transforming Pages

* Keep in mind that destinations, such as links and outlines, may need to be adjusted when a page is scaled or otherwise transformed.

# Notes

PDF document structure

7.7.2 contains the list of all keys in the document catalog.

Document-level structures:
* Extensions
* PageLabels
* Names -- see below
  * Page: TemplateInstantiated
* Dests
* Outlines
* Threads (easy)
  * Page: B
* AA (Additional Actions)
* URI
* AcroForm
* StructTreeRoot
  * Page: StructParents
* MarkInfo
* SpiderInfo
  * Page: ID
* OutputIntents
  * Page: OutputIntents
* PieceInfo
  * Page: PieceInfo
* OCProperties
* Requirements
* Collection
* AF (file specification dictionaries)
  * Page: AF
* DPartRoot
  * Page: DPart

Things qpdf probably needs to drop
* Version
* Perms
* Legal
* DSS

Things that stay with the first document and/or will not be supported
* ViewerPreferences
* PageLayout
* PageMode
* OpenAction
* Metadata
* Lang
* NeedsRendering

Name dictionary (7.4)
* Dests
* AP (appearance strams)
* JavaScript
* Pages (named pages)
* Templates
* IDS
* URLS
* EmbeddedFiles
* AlternatePresentations
* Renditions

Most of chapter 12 applies.

Document-level navigation (12.3)

QPDF will need a global way to reference a page. This will most likely be in the form of the QPDF uuid and a QPDFObjectHandle to the page. If this can just be a QPDFObjectHandle, that would be better. I need to make sure we can meaningfully interact with QPDFObjectHandle objects from multiple QPDFs in a safe fashion. Figure out how this works with immediateCopyFrom, etc. Better to avoid this whole thing and make sure that we just keep all the document-level stuff specific to a PDF, but we will need to have some internal representation that can be used to reconstruct the document-level dictionaries when writing. Making this work with structures (structure destinations) will require more indirection.

I imagine that there will be some internal repreentation of what document-level things come along for the ride when we take a page from a document. I wonder whether this need to change the way linearization works.

There should be different ways to specify collections of pages. The existing one, which is using a numeric range, is just one. Other ideas include things related to document structure (all pages in an article thread, all pages in an outline hierarchy), page labels, book binding (Is that called folio? There's an issue for it.), all except, subgroups, or any number of things.

We will need to be able to start with document-level objects to get page groups and also to start with pages and reconstruct document level objects. For example, it should be possibe to reconstruct article threads to omit beads that don't belong to any of the pages. Likewise with outlines.
