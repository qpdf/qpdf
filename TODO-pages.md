# Pages

**THIS IS A WORK IN PROGRESS. THE ACTUAL IMPLEMENTATION MAY NOT LOOK ANYTHING LIKE THIS. When this
gets to the stage where it is starting to congeal into an actual plan, I will remove this disclaimer
and open a discussion ticket in GitHub to work out details.**

This document describes a project known as the _pages epic_. The goal of the pages epic is to enable
qpdf to properly preserve all functionality associated with a page as pages are copied from one PDF
to another (or back to the same PDF).

Terminology:
* _Page-level data_: information that is contained within objects reachable from the page dictionary
  without traversing through any `/Parent` pointers
* _Document-level data_: information that is reachable from the document catalog (`/Root`) that is
  not reachable from a page dictionary as well as the `/Info` dictionary

Some document-level data references specific pages by page object ID, such as outlines or
interactive forms. Some document-level data doesn't reference any pages, such as embedded files or
optional content (layers). Some document-level data contains information that pertains to a specific
page but does not reference the page, such as page labels (explicit page numbers). Some page-level
data may sometimes depend on document-level data. For example, a _named destination_ depends on the
document-level _names tree_.

As long as qpdf has had the ability to copy pages from one PDF to another, it has had robust
handling of page-level data. Prior to the implementation of the pages epic, with the exception of
page labels, qpdf has ignored document-level data during page copy operations. Specifically, when
qpdf creates a new PDF file from existing PDF files, it always starts with a specific PDF, known as
the _primary input_. The primary input may be the built-in _empty PDF_. With the exception of page
labels, document-level constructs that appear in the primary input are preserved, and document-level
constructs from the other PDF files are ignored. The exception to this is page labels. With page
labels, qpdf always ensures that any given page has the same label in the final output as it had in
whichever input file it originated from, which is usually (but not always) the desired behavior.

Here are several examples of problems in qpdf prior to the implementation of the pages epic:
* If two files with optional content (layers) are merged, all layers in all but the primary input
  will be visible in the combined file.
* If two files with file attachments are merged, attachments will be retained on the primary input
  but dropped on the others. (qpdf has other ways to copy attachments from one file to another.)
* If two files with hyperlinks are merged, any hyperlink from other than primary input whose
  destination is a named destination will become non-functional.
* If two files with outlines are merged, the outlines from the original file will appear in their
  entirety, including outlines that point to pages that are no longer there, and outlines will be
  lost from all files except the primary input.

With the above limitations, qpdf allows combining pages from arbitrary numbers of input PDFs to
create an output PDF, or in the case of page splitting, multiple output PDFs. The API allows
arbitrary combinations of input and output files. The command-line allows only the following:
* Merge: creation of a single output file from a primary input and any number of other inputs by
  selecting pages by index from the beginning or end of the file
* Split: creation of multiple output files from a single input or the result of a merge into files
  whose primary input is the empty PDF and that contain a fixed number of pages per group
* Overlay/underlay: layering pages on top of each other with a maximum of one underlay and one
  overlay and with no ability to specify transformation of the pages (such as scaling, placing them
  in a particular spot).

The pages epic consists of two broad categories of work:
* Proper handling of document-level features when splitting and merging documents
* Greatly increased flexibility in the ways in which pages can be selected from the various input
  files and combined for the output file. This includes creation of blank pages.

Here are some examples of things that will become possible:

* Stacking arbitrary pages on top of each other with full control over transformation and cropping,
  including being able to access information about the various bounding boxes associated with the
  pages
* Inserting blank pages
* Doing n-up page layouts
* Re-ordering pages for printing booklets (also called signatures or printer spreads)
* Selecting pages based on the outline hierarchy, tags, or article threads
* Keeping only and all relevant parts of the outline hierarchies from all input files
* Creating single very long or wide pages with output from other pages

The rest of this document describes the details of what how these features will work and what needs
to be done to make them possible to build.

# Architectural Thoughts

Create a new top-level class called `QPDFAssembler` that will be used to perform page-level
operations. Its implementation will use existing APIs, and it will add many new APIs. It should be
possible to perform all existing page splitting and merging operations using `QPDFAssembler` without
having to worry about details such as copying annotations, remapping destinations, and adjusting
document-level data.

Early strategy: keep `QPDFAssembler` private to the library, and start with a pure C++ API (no JSON
support). Migrate splitting and merging from `QPDFJob` into `QPDFAssembler`, then build in
document-level support. Also work the difference between normal write and split, which are two
separate ways to write output files.

One of the main responsibilities of `QPDFAssembler` will be to remap destinations as data from a
page is moved or copied. For example, if an outline has a destination that points to a particular
rectangle on page 5 of the second file, and we end up dropping a portion of that page into an n-up
configuration on a specific output page, we will have to keep track of enough information to replace
the destination with a new one that points to the new physical location of the same material. For
another example, consider a case in which the left side of page 3 of the primary input ends up as
page 5 of the output and the right side of page 3 ends up as page 6. We would have to map
destinations from a single source page to different destination pages based on which part of the
page it was on. If part of the rectangle points to one page and part to another, what do we do? I
suggest we go with the top/center of the rectangle.

A destination consists of a QPDF, page object, and rectangle in user coordinates. When
`QPDFAssembler` copies a page or converts it to a form XObject, possibly with transformations
applied, it will have to be able to map a destination to the same triple (QPDF, page object,
rectangle) on all pages that contain data from the original page. When writing the final output, any
destination that no longer points anywhere should be dropped, and any destination that points to
multiple places will need to be handled according to some specification.

Whenever we create any new thing from a page, we create _derived page data_. Examples of derived
page data would include a copy of the page and a form XObject created from a page. `QPDFAssembler`
will have to keep a mapping from any source page to all of its derived objects along with any
transformations or clipping. When a derived page data object is placed on a final page, that
information can be combined with the position and any transformations onto the final page to be able
to map any destination to a new one or to determine that it points outside of the visible area.

If a source page is copied multiple times, then if exactly one copy is explicitly marked as the
target, that becomes the target. Otherwise, the first derived object to be placed becomes the
target.

## Overall Structure

A single instance of `QPDFAssembler` creates a single assembly job. `QPDFJob` can create one
assembly job but does other things, such as setting writer options, inspection operations, etc. An
assembly job consists of the following:
* Global document-level data handling information
  * Mode
    * intelligent: try to combine everything using latest capabilities of qpdf; this is the default
    * legacy: document-level features are kept from primary input; this is for compatibility and can
      be selected from the CLI
* Input sources
  * File/password
    * Whether to keep attachments: yes, no, if-all-pages (default)
  * Empty
* Output mode
  * Single file
  * Split -- this must include definitions of the split groups
* Description of the output in terms of the input sources and some series of transformations

## Cases to support

Here is a list of cases that need to be expressible.

* Create output by concatenating pages from page groups where each page group is pages specified by
  a numeric range. This is what `--pages` does now.
* Collation, including different sized groups.
* Overlay/underlay, generalized to support a stack consisting of various underlays, the base page,
  and various overlays, with flexibility around posititioning. It should be natural to express
  exactly whate underlay and overlay do now.
* Split into groups of fixed size (what `--split-pages` does) with the ability to define split
  groups based on other things, like outlines, article threads, and document structure
* Examples from the manual:
  * `qpdf in.pdf --pages . a.pdf b.pdf:even -- out.pdf`
  * `qpdf --empty --pages a.pdf b.pdf --password=x z-1 c.pdf 3,6`
  * `qpdf --collate odd.pdf --pages . even.pdf -- all.pdf`
  * `qpdf --collate --empty --pages odd.pdf even.pdf -- all.pdf`
  * `qpdf --collate --empty --pages a.pdf 1-5 b.pdf 6-4 c.pdf r1 -- out.pdf`
  * `qpdf --collate=2 --empty --pages a.pdf 1-5 b.pdf 6-4 c.pdf r1 -- out.pdf`
  * `qpdf file2.pdf --pages file1.pdf 1-5 . 15-11 -- outfile.pdf`
  *
    ```
    qpdf --empty --copy-encryption=encrypted.pdf \
          --encryption-file-password=pass \
          --pages encrypted.pdf --password=pass 1 \
                ./encrypted.pdf --password=pass 1 -- \
          outfile.pdf
    ```
  * `qpdf --collate=2,6 a.pdf --pages . b.pdf -- all.pdf`
    * Take A 1-2, B 1-6, A 3-4, C 7-12, A 5-6, B 13-18, ...
* Ideas from pstops. The following is an excerpt from the pstops manual page.

  This section contains some sample re‐arrangements. To put two pages on one sheet (of A4 paper),
  the pagespec to use is:
  ```
  2:0L@.7(21cm,0)+1L@.7(21cm,14.85cm)
  ```
  To select all of the odd pages in reverse order, use:
  ```
  2:‐0
  ```
  To re‐arrange pages for printing 2‐up booklets, use
  ```
  4:‐3L@.7(21cm,0)+0L@.7(21cm,14.85cm)
  ```
  for the front sides, and
  ```
  4:1L@.7(21cm,0)+‐2L@.7(21cm,14.85cm)
  ```
  for the reverse sides (or join them with a comma for duplex printing).
* From #493
  ```
   pdf2ps infile.pdf infile.ps
   ps2ps -pa4 "2:0R(4.5cm,26.85cm)+1R(4.5cm,14.85cm)" infile.ps outfile.ps
   ps2pdf outfile.ps outfile.pdf
   ```
* Like psbook. Signature size n:
  * take groups of 4n
  * shown for n=3 in order such that, if printed so that the front of the first page is on top, the
    whole stack can be folded in half.
    * front: 6,7, back: 8,5
    * front: 4,9, back: 10,3
    * front: 2,11, back: 12,1

    This is the same as dupex 2-up with pages in order 6, 7, 8, 5, 4, 9, 10, 3, 2, 11, 12, 1
* n-up:
  * For 2-up, calculate new w and h such that w/h maintains a fixed ratio and w and h are the
    largest values that can fit within 1/2 the page with specified margins.
  * Can support 1, 2, 4, 6, 9, 16. 2 and 6 require rotation. The others don't. Will probably need to
    change getFormXObjectForPage to handle other boxes than trim box.
  * Maybe define n-up a scale and rotate followed by fitting the result into a specified rectangle.
    I might already have this logic in QPDFAnnotationObjectHelper::getPageContentForAppearance.


# Feature to Issue Mapping

Last checked: 2023-12-29

```
gh search issues label:pages --repo qpdf/qpdf --limit 200 --state=open
```

* Generate a mapping from source to destination for all destinations
  * Issues: #1077
  * Notes:
    * Source can be an outline or link, either directly or via action. If link, it should include
      the page.
    * Destination can be a structure destination, which should map to a regular destination.
    * source: page X -> link -> action -> dest: page Y
    * source: page X -> link -> action -> dest: structure -> page Y
    * Consider something in json that dumps this.
    * We will need to associate this with a QPDF. It would be great if remote or embedded go-to
      actions could be handled, but that's ambitious.
    * It will be necessary to keep some global map that includes all QPDF objects that are part of
      the final file.
    * An interesting use case to consider would be to create a QPDF object from an embedded file and
      append the embedded file and make the embedded actions work. This would probably require some
      way to tell qpdf that a particular external file came from an embedded file.
* Control size of page and position/transformation of overlay/underlay
  * Issues: #1031, #811, #740, #559
  * Notes:
    * It should be possible to define a destination page from scratch or in terms of other pages and
      then place page contents onto it with arbitrary transformations applied.
    * It should be possible to compute the size of the destination page in terms of the source
      pages, e.g., to create one long or wide page from other pages.
    * Also allow specification of which page box to use
* Preserve hyperlinks when doing any page operations
  * See also "Generate a mapping from source to destination for all destinations"
  * Issues: #1003, #797, #94
  * Notes:
    * A link annotation that points to a destination rather than an external URL should continue to
      work when files are split or merged.
* Awareness of structured and tagged PDF (14.7, 14.8)
  * Issues: #957, #953, #490
  * Notes:
    * This looks complicated. It may be not be possible to do this fully in the first increment, but
      we have to keep it in mind and warn if we can't and we see /SD in an action.
    * #490 has some good analysis
* Assign page labels
  * Issues: #939
  * Notes:
    * #939 has a good proposal
    * This could be applied to page groups, and we could have an option to keep the labels as they
      are in a given group, which is what qpdf does now.
* Interleave pages with ordering
  * Issues: #921
  * Notes:
    * From 921: interleave odd pages and reversed even pages. This might require different handling
      for even/odd numbers of pages. Make sure it's natural for the cases of len(odd) == len(even)
      or len(odd) == 1+len(even)
* Preserve all attachments when merging files
  * Issues: #856
  * Notes:
    * If all pages of a file are selected, keep all attachments
    * If some pages of a file are selected
      * Keep all attachments if there are any embedded file annotations
      * Otherwise, what? Do we have a keep-attachments flag of some sort? Or do we just make the
        user copy attachments from one file to another?
* Apply clipping to a page
  * Issues: #771
  * Notes:
    * Create a form xobject from a page, then apply a specific clipping region expressed in
      coordinates or as a percentage
* Ability to create a blank page
  * Issues: #753
  * Notes:
    * Create a blank page of a specific size or of the same size as another page
* Split groups with explicit boundaries
  * Issues: #741, #616
  * Notes:
    * Example: --split-after a,b,c
* Handle Optional Content (layers) (8.11)
  * Issues: #672, #9, #570
* Scale a page up or down to fit to a size
  * Issues: #611
* Place contents of pages adjacent horizontally or vertically on one page
  * Issues: #1040, #546
* nup, booklet
  * Issues: #493, #461, #152
  * Notes:
    * #461 may want the inverse of booklet and discusses reader and printer spreads
* Flexible multiplexing
  * Issues: #505 (already implemented with --collate)
* Split pages based on outlines
  * Issues: #477
* Keep relevant parts of outline hierarchy
  * Issues: #457, #356, #343, #323
  * Notes:
    * There is some helpful discussion in #343 including
      * Preserving open/closed status
      * Preserving javascript actions

# XXX OLD NOTES

I want to encapsulate various aspects of the logic into interfaces that can be implemented by
developers to add their own logic. It should be easy to contribute these. Here are some rough ideas.

A source is an input file, the output of another operation, or a blank page. In the API, it can be
any QPDF object.

A page group is just a group of pages.

* PageSelector -- creates page groups from other page groups
* PageTransformer -- selects a part of a page and possibly transforms it; applies to all pages of a
  group. Based on the page dictionary; does not look at the content stream
* PageFilter -- apply arbitrary code to a page; may access the content stream
* PageAssembler -- combines pages from groups into new groups whose pages are each assembled from
  corresponding pages of the input groups

These should be able to be composed in arbitrary ways. There should be a natural API for doing this,
and it there should be some specification, probably based on JSON, that can be provided on the
command line or embedded in the job JSON format. I have been considering whether a lisp-like
S-expression syntax may be less cumbersome to work with. I'll have to decide whether to support this
or some other syntax in addition to a JSON representation.

There also needs to be something to represent how document-level structures relate to this. I'm not
sure exactly how this should work, but we need things like
* what to do with page labels, especially when assembling pages from other pages
* whether to preserve destinations (outlines, links, etc.), particularly when pages are duplicated
  * If A refers to B and there is more than one copy of B, how do you decide which copies of A link
    to which copies of B?
* what to do with pages that belong to more than one group, e.g., what happens if you used document
  structure or outlines to form page groups and a group boundary lies in the middle of the page

Maybe pages groups can have arbitrary, user-defined tags so we can specify that links should only
point to other pages with the same value of some tag. We can probably many-to-one links if the
source is duplicated.

We probably need to hold onto the concept of the primary input file. If there is a primary input
file, there may need to be a way to specify what gets preserved it. The behavior of qpdf prior to
all of this is to preserve all document-level constructs from the primary input file and to try to
preserve page labels from other input files when combining pages.

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
  * the left-front (left-back, right-front, right-back) pages of a booklet with signatures of n
    pages
  * all pages reachable from a section of the outline hierarchy or something based on threads or
    other structure
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
    * Control placement based on properties of all the groups, so higher order than a stand-alone
      transformer
    * Examples
      * Scale the smaller page up to the size of the larger page
      * Center the smaller page horizontally and bottom-align the trim boxes
  * Generalized overlay/underlay allowing n pages in a given order with transformations.
  * n-up -- application of generalized overlay/underlay
  * make one long page with an arbitrary number of pages one after the other (#546)

It should be possible to represent all of the existing qpdf operations using the above framework. It
would be good to re-implement all of them in terms of this framework to exercise it. We will have to
look through all the command-line arguments and make sure. Of course also make sure suggestions from
issues can be implemented or at least supported by adding new selectors.

Here are a few bits of scratch work. The top-level call is a selector. This doesn't capture
everything. Implementing this would be tedious and challenging. It could be done using JSON arrays,
but it would be clunky. This feels over-designed and possibly in conflict with QPDFJob.

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

```json

```

# Supporting Document-level Features

qpdf needs full support for document-level features like article threads, outlines, etc. There is no
support for some things and partial support for others. See notes below for a comprehensive list.

Most likely, this will be done by creating DocumentHelper and ObjectHelper classes.

It will be necessary not only to read information about these structures from a single PDF file as
the existing document helpers do but also to reconstruct or update these based on modifications to
the pages in a file. I'm not sure how to do that, but one idea would be to allow a document helper
to register a callback with QPDFPageDocumentHelper that notifies it when a page is added or removed.
This may be able to take other parameters such as a document helper from a foreign file.

Since these operations can be expensive, there will need to be a way to opt in and out. The default
(to be clearly documented) should be that all supported document-level constructs are preserved.
That way, as new features are added, changes to the output of previous operations to include
information that was previously omitted will not constitute a non-backward compatible change that
requires a major version bump. This will be a default for the API when using the higher-level page
assemebly API (below) as well as the CLI.

There will also need to be some kind of support for features that are document-level and not tied to
any pages, such as (sometimes) embedded files. When splitting/merging files, there needs to be a way
to specify what should happen with those things. Perhaps the default here should be that these are
preserved from files from which all pages are selected. For some things, like viewer preferences, it
may make sense to take them from the first file.

# Page Assembly (page selection)

In addition to the existing numeric ranges of page numbers, page selection could be driven by
document-level features like the outlines hierarchy or article threads. There have been a lot of
suggestions about this in various tickets. There will need to be some kind of page manipulation
class with configuration options. I'm thinking something similar to QPDFJob, where you construct a
class and then call a bunch of methods to configure it, including the ability to configure with
JSON. Several suggestions have been made in issues, which I will go through and distill into a list.
Off hand, some ideas include being able to split based on explicit chunks and being able to do all
pages except a list of pages.

For CLI, I'm probably going to have it take a JSON blob or JSON file on the CLI rather than having
some absurd way of doing it with arguments (people have a lot of trouble with --pages as it is). See
TODO for a feature on command-line/job JSON support for JSON specification arguments.

There are some other things, like allowing n-up and genearlizing overlay/underlay to allow different
placement and scaling options, that I think may also be in scope.

# Scaling/Transforming Pages

* Keep in mind that destinations, such as links and outlines, may need to be adjusted when a page is
  scaled or otherwise transformed.

# Notes

PDF document structure

The trailer contains the catalog and the Info dictionary. We probably need to do something
intelligent with the info dictionary.


7.7.2 contains the list of all keys in the document catalog.

Document-level structures:
* Extensions
  * Must be combination of Extensions from all input files
* PageLabels
  * Ensure each page has its original label
  * Allow post-processing
* Names -- see below
  * Combined and disambiguated
  * Page: TemplateInstantiated
ombine from all files
* Dests
  * Keep referenced destinations across all files
  * May need to disambiguate or "flatten" or convert to named dests with the names tree
* Outlines
* Threads (easy)
  * Page: B
* AA (Additional Actions)
  * Merge from different files if possible
  * If duplicate, first contributor wins
* AcroForm
  * Merge
* StructTreeRoot
  * Combine
  * Page: StructParents
* MarkInfo (see 14.7 - Logical Structure, 14.8 Tagged PDF)
  * Combine
* SpiderInfo
  * Combine
  * Page: ID
* OutputIntents
  * Combine
  * Page: OutputIntents
* PieceInfo
  * Combine
  * Page: PieceInfo
* OCProperties
  * Combine across documents
* Requirements
  * Combine
* AF (file specification dictionaries)
  * Combine
  * Page: AF
* DPartRoot
  * Combine
  * Page: DPart

Things qpdf probably needs to drop
* Version
* Perms
* Legal
* DSS

Things that stay with the first document that has one and/or will not be supported
* Info (not part of document catalog)
* ViewerPreferences
* PageLayout
* PageMode
* OpenAction
* URI
* Metadata
* Lang
* NeedsRendering
* Collection

Name dictionary (7.7.4)
* Dests
* AP (appearance strams)
* JavaScript
* Pages (named pages)
* Templates
  * Combine across all documents
  * Page: TemplateInstantiated points to a named page
* IDS
* URLS
* EmbeddedFiles
* AlternatePresentations
* Renditions

Most of chapter 12 applies.

Document-level navigation (12.3)

QPDF will need a global way to reference a page. This will most likely be in the form of the QPDF
uuid and a QPDFObjectHandle to the page. If this can just be a QPDFObjectHandle, that would be
better. I need to make sure we can meaningfully interact with QPDFObjectHandle objects from multiple
QPDFs in a safe fashion. Figure out how this works with immediateCopyFrom, etc. Better to avoid this
whole thing and make sure that we just keep all the document-level stuff specific to a PDF, but we
will need to have some internal representation that can be used to reconstruct the document-level
dictionaries when writing. Making this work with structures (structure destinations) will require
more indirection.

I imagine that there will be some internal repreentation of what document-level things come along
for the ride when we take a page from a document. I wonder whether this need to change the way
linearization works.

There should be different ways to specify collections of pages. The existing one, which is using a
numeric range, is just one. Other ideas include things related to document structure (all pages in
an article thread, all pages in an outline hierarchy), page labels, book binding (Is that called
folio? There's an issue for it.), subgroups, or any number of things.

We will need to be able to start with document-level objects to get page groups and also to start
with pages and reconstruct document level objects. For example, it should be possibe to reconstruct
article threads to omit beads that don't belong to any of the pages. Likewise with outlines.
