# Pages

This file contains plans and notes regarding implementing of the "pages epic." The pages epic consists of the following features:
* Proper handling of document-level features when splitting and merging documents
* Insertion of blank pages
* More flexible aways of
  * selecting pages from one or more documents
  * composing pages out of other pages
    * underlay and overlay with control over position, transformation, and bounding box selection
  * organizing pages
    * n-up
    * booklet generation ("signatures", as in what `psbook` does)
* Possibly others pending analysis of open issues and public discussion

# Architectural Thoughts

I want to encapsulate various aspects of the logic into interfaces that can be implemented by developers to add their own logic. It should be easy to contribute these. Here are some rough ideas.

A page group is just a group of pages.

* PageSelector -- creates page groups from other page groups
* PageTransformer -- selects a part of a page and possibly transforms it; applies to all pages of a group. Based on the page dictionary; does not look at the content stream
* PageFilter -- apply arbitrary code to a page; may access the content stream
* PageAssembler -- combines pages from groups into new groups whose pages are each assembled from corresponding pages of the input groups

These should be able to be composed in arbitrary ways. There should be a natural API for doing this, and it there should be some specification, probably based on JSON, that can be provided on the command line or embedded in the job JSON format. I have been considering whether a lisp-like S-expression syntax may be less cumbersome to work with. I'll have to decide whether to support this or some other syntax in addition to a JSON representation.

There also needs to be something to represent how document-level structures relate to this. I'm not sure exactly how this should work, but we need things like
* what to do with page labels, especially when assembling pages from other pages
* whether to preserve destinations (outlines, links, etc.), particularly when pages are duplicated
  * If A refers to B and there is more than one copy of B, how do you decide which copies of A link to which copies of B?
* what to do with pages that belong to more than one group, e.g., what happens if you used document structure or outlines to form page groups and a group boundary lies in the middle of the page

Maybe pages groups can have arbitrary, user-defined tags so we can specify that links should only point to other pages with the same value of some tag. We can probably many-to-one links if the source is duplicated.

We probably need to hold onto the concept of the primary input file. If there is a primary input file, there may need to be a way to specify what gets preserved it. The behavior of qpdf prior to all of this is to preserve all document-level constructs from the primary input file and to try to preserve page labels from other input files when combining pages.

Here are some examples.

* PageSelector
  * all pages from an input file
  * pages from a group using a NumericRange
  * concatenate groups
  * pages from a group in reverse order
  * a group repeated as often as necessary until a specified number of pages is reached
  * a group padded with blank pages to create a multiple of n pages
  * odd or even pages from a group
  * every nth page from a group
  * pages interleaved from multiple groups
  * the left-front (left-back, right-front, right-back) pages of a booklet with signatures of n pages
  * all pages reachable from a section of the outline hierarchy or something based on threads or other structure
  * selection based on page labels
* PageTransformer
  * clip to media box (trim box, crop box, etc.)
  * clip to specific absolute or relative size
  * scale
  * translate
  * rotate
  * apply transformation matrix
* PageFilter
  * optimize images
  * flatten annotations
* PageAssembler
  * Overlay/underlay all pages from one group onto corresponding pages from another group
    * Control placement based on properties of all the groups, so higher order than a stand-alone transformer
    * Examples
      * Scale the smaller page up to the size of the larger page
      * Center the smaller page horizontally and bottom-align the trim boxes
  * Generalized overlay/underlay allowing n pages in a given order with transformations.
  * n-up -- application of generalized overlay/underlay

It should be possible to represent all of the existing qpdf operations using the above framework. It would be good to re-implement all of them in terms of this framework to exercise it. We will have to look through all the command-line arguments and make sure. Of course also make sure suggestions from issues can be implemented or at least supported by adding new selectors.

Here are a few bits of scratch work. The top-level call is a selector. This doesn't capture everything. Implementing this would be tedious and challenging. It could be done using JSON arrays, but it would be clunky. This feels over-designed and possibly in conflict with QPDFJob.

```
(concat
 (primary-input)
 (file "file2.pdf")
 (page-range (file "file3.pdf") "1-4,5-8")
)

(with
 ("a"
  (concat
   (primary-input)
   (file "file2.pdf")
   (page-range (file "file3.pdf") "1-4,5-8")
  )
 )
 (concat
  (even-pages (from "a"))
  (reverse (odd-pages (from "a")))
 )
)

(with
 ("a"
  (concat
   (primary-input)
   (file "file2.pdf")
   (page-range (file "file3.pdf") "1-4,5-8")
  )
  "b-even"
  (even-pages (from "a"))
  "b-odd"
  (reverse (odd-pages (from "a")))
 )
 (stack
  (repeat-range (from "a") "z")
  (pad-end (from "b"))
 )
)
```

Easier to parse but yuck:
```json
["with",
 ["a",
  ["concat",
   ["primary-input"],
   ["file", "file2.pdf"],
   ["page-range", ["file", "file3.pdf"], "1-4,5-8"]
  ],
  "b-even",
  ["even-pages", ["from", "a"]],
  "b-odd",
  ["reverse", ["odd-pages", ["from", "a"]]]
 ],
 ["stack",
  ["repeat-range", ["from", "a"], "z"],
  ["pad-end", ["from", "b"]]
 ]
]
```

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
