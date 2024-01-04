# Pages

**This is a work in progress, but it's getting close. When this gets to the stage where it is
starting to congeal into an actual plan, I will remove this disclaimer and open a discussion ticket
in GitHub to work out details.**

This document describes a project known as the _pages epic_. The goal of the pages epic is to enable
qpdf to properly preserve all functionality associated with a page as pages are copied from one PDF
to another (or back to the same PDF). A secondary goal is to add more flexiblity to the ways in
which documents can be split and combined (flexible assembly).

Terminology:
* _Page-level data_: information that is contained within objects reachable from the page dictionary
  without traversing through any `/Parent` pointers
* _Document-level data_: information that is reachable from the document catalog (`/Root`) that is
  not reachable from a page dictionary as well as the `/Info` dictionary

PDF uses document-level data in a variety of ways. There is some document-level data that has each
of the following properties, among others:
* References pages by object ID (outlines, interactive forms)
* Doesn't reference any pages (embedded files)
* Doesn't reference any pages but influences page rendering (optional content/layers)
* Doesn't reference any pages but contains information about pages (page labels)
* Contains information used by pages (named destinations)

As long as qpdf has had the ability to copy pages from one PDF to another, it has had robust
handling of page-level data. When qpdf creates a new PDF file from existing PDF files, it starts
with a specific PDF, known as the _primary input_. The primary input may be a file or the built-in
_empty PDF_. Prior to the implementation of the pages epic, qpdf has ignored document-level data
(except for page labels and interactive form fields) when merging and splitting files. Any
document-level data in the primary input was preserved, and any document-level data other than form
fields and page labels was discarded from the other files. After this work is complete, qpdf will
handle other document-level data in a manner that preserves the functionality of all pages in the
final PDF. Here are several examples of problems in qpdf prior to the implementation of the pages
epic:
* If two files with optional content (layers) are merged, all layers in all but the primary input
  will be visible in the combined file.
* If two files with file attachments are merged, attachments will be retained on the primary input
  but dropped on the others. (qpdf has other ways to copy attachments from one file to another.)
* If two files with hyperlinks are merged, any hyperlink from other than primary input become
  non-functional.
* If two files with outlines are merged, the outlines from the original file will appear in their
  entirety, including outlines that point to pages that are no longer there, and outlines will be
  lost from all files except the primary input.

Regarding page assembly, prior to the pages epic, qpdf allows combining pages from arbitrary numbers
of input PDFs to create an output PDF, or in the case of page splitting, multiple output PDFs. The
API allows arbitrary combinations of input and output files. The command-line allows only the
following:
* Merge: creation of a single output file from a primary input and any number of other inputs by
  selecting pages by index from the beginning or end of the file
* Split: creation of multiple output files from a single input or the result of a merge into files
  whose primary input is the empty PDF and that contain a fixed number of pages per group
* Overlay/underlay: layering pages on top of each other with a maximum of one underlay and one
  overlay and with no ability to specify transformation of the pages (such as scaling, placing them
  in a particular spot).

The pages epic consists of two broad categories of work:
* Proper handling of document-level features when splitting and merging documents
* Flexible assembly: greatly increased flexibility in the ways in which pages can be selected from
  the various input files and combined for the output file. This includes creation of blank pages
  and composition of pages (n-up or other ways of combining multiple input pages into one output
  page)

Here are some examples of things that will become possible:

* Stacking arbitrary pages on top of each other with full control over transformation and cropping,
  including being able to access information about the various bounding boxes associated with the
  pages (generalization of underlay/overlay)
* Inserting blank pages
* Doing n-up page layouts
* Creating single very long or wide pages with output from other pages
* Re-ordering pages for printing booklets (also called signatures or printer spreads)
* Selecting pages based on the outline hierarchy, tags, or article threads
* Keeping only and all relevant parts of the outline hierarchies from all input files

The rest of this document describes the details of what how these features will work and what needs
to be done to make them possible to build.

# Architecture

Create a `QPDFAssembler` class to handle merging and a `QPDFSplitter` to handle splitting. The
complex assembly logic can be handled by `QPDFAssembler`. `QPDFSplitter` can invoke `QPDFAssembler`
with a previous `QPDFAssembler`'s output (or any `QPDF`) multiple times to create the split files.
This will mostly involve moving code from `QPDFJob` to `QPDFAssembler` and `QPDFSplitter` and having
`QPDFJob` invoke them.

Prior to implementation of the pages epic, `QPDFJob` goes through the following stages:

* create QPDF
  * update from JSON
  * page specs (`--pages`)
    * Create a QPDF for each input source
    * Figure out whether to keep files open
    * Remove unreferenced resources if needed
    * Remove pages from the pages tree
    * Handle collation
    * Copy or revive all final pages
      * When copying foreign pages, possibly remove unreferenced resources
      * Handle the same page copied more than once by doing a shallow copy
      * Preserve form fields and page labels
    * Delete pages from the primary input that were not used in the output
    * Delete unreferenced form fields
  * rotation
  * underlay/overlay
  * transformations
    * disable signatures
    * externalize images
    * optimize images
    * generate appearances
    * flatten annotations
    * coalesce contents
    * flatten rotation
    * remove page labels
    * remove attachments
    * add attachments
    * copy attachments
* write QPDF
  * One of:
    * Do inspections
    * Write single file
    * Split pages
      * Remove unreference resources if needed
      * Preserve form fields and page labels

Broadly, the above has to be modified in the following ways:
* The transformations step has to be pulled out as that wil stay in `QPDFJob`.
* Most of write QPDF will stay in `QPDFJob`, but the split logic will move to `QPDFSplitter`.
* The entire create QPDF logic will move into `QPDFAssembler`.
* `QPDFAssembler`'s API will allow using an arbitrary QPDF as an input rather than having to start
  with a file. That makes it possible to do arbitrary work on the PDF prior to passing it to
  `QPDFAssembler`.
* `QPDFAssembler` and `QPDFSplitter` may need a C API, or perhaps C users will have to work through
  `QPDFJob`, which will expose nearly all of the functionality.

Within `QPDFAssembler`, we will extend the create QPDF logic in the following ways:
* Allow creation of blank pages as an additional input source
* Generalize underlay/overlay
  * Enable controlling placement
  * Make repeatable
* Add additional reordering options
  * We don't need to provide hooks for this. If someone is going to code a hook, they can just
    compute the page ordering directly.
* Have a page composition stage after the overlay/underlay stage
  * Allow n-up, left-to-right (can reverse page order to get rtl), top-to-bottom, or modular
    composition like pstops
* Add additional ways to select pages besides range (e.g. based on outlines)
* Enhance existing logic to handle other document-level structures, preferably in a way that
  requires less duplication between split and merge.
  * We don't need to turn on and off most types of document constructs individually. People can
    preprocess using the API or qpdf JSON if they want fine-grained control.
  * For things like attachments and outlines, we can add additional flags.

Within `QPDFSplitter`, we will add additional ways to specify boundaries for splitting.

We must take care with the implementations and APIs for `QPDFSplitter`, `QPDFAssembler`, and
`QPDFJob` to avoid excessive duplication. Perhaps `QPDFJob` can create and configure a
`QPDFAssembler` and `QPDFSplitter` on the fly to avoid too much duplication of state.

Much of the logic will actually reside in other helper classes. For example, `QPDFAssembler` will
probably not operate with numeric ranges, leaving that to `QPDFJob` and `QUtil` but will instead
have vectors of page numbers. The logic for creating page groups from outlines, threads, or
structure will most likely live in the document helpers for those bits of functionality. This keeps
needless clutter out of `QPDFAssembler` and also makes it possible for people to perform their own
subset of functionality by calling lower-level interfaces. The main power of `QPDFAssembler` will be
to manage sequencing and destination tracking as well as to provide a future-proof API that will
allow developers to automatically benefit from additional document-level support as it is added to
qpdf.

## Flexible Assembly

This section discusses modifications to the command-line syntax to make it easier to add flexibility
going forward without breaking backward compatibility. The main thrust will be to create
non-positional alternatives to some things that currently use positional arguments (`--pages`,
`--overlay`, `--underlay`), as was done for `--encrypt` in 11.7.0, to make it possible to add
additional flags.

In several cases, we allow specification of transformations or placements. In this context:
* The origin is always lower-left corner.
* A _dimension_ may be absolute or relative.
  * An _absolute dimension_ is `{n}` (in points), `{n}in` (inches), `{n}cm` (centimeters),
  * A _relative dimension_ is expressed in terms of the corresponding dimension of one of a page's
    boxes. Which dimension is determined by context.
    * `{n}{M|C|B|T|A}` is `{n}` times the corresopnding dimension of the media, crop, bleed, trim,
      or art box. Example: `0.5M` would be half the width or height of the media box.
    * `{n}+{M|C|B|T|A}` is `{n}` plus the corresponding dimension. Example: `-0.5in+T` is half an
      inch (36 points) less than the width or height of the trim box.
* A _size_ is
  * `{w}x{h}`, where `{w}` and `{h}` are dimensions
  * `letter|a4` (potentially add other page sizes)
* A _position_ is `{x}x{y}` where `{x}` and `{y}` are dimensions offset from the origin
* A _rectangle_ is `{llx},{lly},{urx},{ury}` (lower|upper left|right x|y) with `llx` < `urx` and
  `lly` < `ury`
  * Examples:
    * `0.1M,0.1M,0.9M,0.9M` is a box whose llx is 10% of the media box width, lly is 10% of the
      height, urx is 90% of the width, and ury is 90% of the height
    * `0,0,612,792` is a box whose size is that of a US Letter page.
  * A rectangle may also be just one of `M|C|B|T|A` to refer to a page's media, crop, bleed, trim,
    or art box.

Tweak `--pages` similarly to `--encrypt`. As an alternative to `--pages file [--password=p] range
--`, support `--pages --file=x --password=y --range=z --`. This allows for a more flexible syntax.
If `--file` appears, positional arguments are disallowed. The same applies to `--overlay` and
`--underlay`.

```
OLD: qpdf 2.pdf --pages 1.pdf --password=x . 3.pdf 1-z -- out.pdf
NEW: qpdf 2.pdf --pages --file=1.pdf --password=x --file=. --file 3.pdf --range=1-z -- out.pdf
```

This makes it possible to add additional flags to do things like control how document-level features
are handled, specify placement options, etc. Given the above framework, it would be possible to add
additional features incrementally, without breaking compatibility, such as selecting or splitting
pages based on tags, article threads, or outlines.

It's tempting to allow assemblies to be nested, but this gets very complicated. From the C++ API,
there is no problem using the output of one `QPDFAssembler` as the input to another, but supporting
this from the CLI is hard because of the way JSON/arg parsing is set up. If people need to do that,
they can just create intermediate files.

Proposed CLI enhancements:

```
# --pages: inputs
--file=x [ --password=x ]
--blank=n  [ --size={size} [ --size-from-page=n ] ]  # see below
# modifiers refer to most recent input
--range=...
--with-attachments={none|all|referenced}  # default = referenced
--with-outlines={none|all|referenced}  # default = referenced
--...  # future options to select pages based on outlines, article threads, tags, etc.
# placement (matrix transformation -- see notes below)
--rotate=[+-]angle[:page-range]  # existing
--scale=x,y[:page-range]
--translate=dx,dy[:page-range]  # dx and dy are dimensions
--flip={h|v}[:page-range]
--transform=a,b,c,d,e,f[:page-range]
--set-box={M|C|B|T|A}=rect[:page-range]  # change a bounding box
# stacking -- make --underlay and --overlay repeatbale
--{underlay|overlay} ... --
--file=x [ --password=x ]
--from, --to, --repeat  # same as current --overlay, --underlay
--from-rect={rect} # default = T -- see notes
--to-rect={rect} # default = M -- see notes
# composition -- a new QPDFJob stage between stacking and transformation
--compose=...  # see notes
--n-up={2,4,6,9,16}
--concat={h|v}  # concatenate all pages to a single big page
# reordering
--collate=a,b,c  # exists
--booklet=...  # re-order pages for book signatures like psbook -- see notes
# split
--split-pages=n  # existing
--split-after=a,b,c  # split after each named page
--...  # future options to split based on outlines, article threads, tags, etc.
# post-processing (with transformations like optimize images)
--set-page-labels ... # See issue #939
```

Notes:
* For `--blank`, `--size` specifies the size of the blank page. If any relative dimensions are used,
  `--size-from-page=n` must be used to specify the page (from n in the overall input) that relative
  dimensions should be taken from. It is an error to specify a relative size based on another blank
  page. (Let's not complicate things by doing a graph traversal to find an eventual absolute page.
  Just disallow a blank page to specified relative to another blank page.)
* For stacking, the default is to map the source page's trim box onto the destination page's
  mediabox. This is a weird default, but it's there for compatibility. The `--from-rect` and
  `--to-rect` may be used to map an arbitrary region of the over/underlay file into an arbitrary
  region of a page. With the defaults, an overlay or underlay page will be stretched or shrunk if
  pages are of variable size. Absolute rectangles can be used to avoid this. If a rectangle uses
  relative dimensions, they are relative to the page that has the rectangle. You can't create a
  `--to-rect` relative to the size of the from page or vice versa. If you need to do this, use
  external logic to compute the rectangles and then use absolute rectangles.
* `--compose`: XXX
* `--booklet`: XXX
* The `--set-page-labels` option would be done at the very end and is actually not blocked by
  anything else here. It can be done near removing page labels in `handleTransformations`.
* I'm not sure what impact composition should have on page labels. Most likely, we should drop page
  labels on composition. If someone wants them, they can use `--set-page-labels`.

### Compose, Booklet

This section needs to be fleshed out. It is probably lower priority than document-level work.

Here are some ideas from pstops. The following is an excerpt from the pstops manual page. Maybe we
can come up with something similar using our enhanced rectangle syntax.

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

From issue #493
```
 pdf2ps infile.pdf infile.ps
 ps2ps -pa4 "2:0R(4.5cm,26.85cm)+1R(4.5cm,14.85cm)" infile.ps outfile.ps
 ps2pdf outfile.ps outfile.pdf
 ```

Notes on signatures (psbook). For a signature of size 3, we have the following assuming a 2-up
configuration that is printed double-sided so that, when the whole stack is placed face-up and
folded in half, page 1 is on top.
* front: 6,7, back: 8,5
* front: 4,9, back: 10,3
* front: 2,11, back: 12,1

This is the same as duplex 2-up with pages in order 6, 7, 8, 5, 4, 9, 10, 3, 2, 11, 12, 1

n-up:
* For 2-up, calculate new w and h such that w/h maintains a fixed ratio and w and h are the largest
  values that can fit within 1/2 the page with specified margins.
* Can support 1, 2, 4, 6, 9, 16. 2 and 6 require rotation. The others don't. Will probably need to
  change getFormXObjectForPage to handle other boxes than trim box.
* Maybe define n-up a scale and rotate followed by fitting the result into a specified rectangle. I
  might already have this logic in QPDFAnnotationObjectHelper::getPageContentForAppearance.

## Destinations

We will have to keep track of destinations that point to a page when the page is moved or copied.
For example, if an outline has a destination that points to a particular rectangle on page 5 of the
second file, and we end up dropping a portion of that page into an n-up configuration on a specific
output page, we will have to keep track of enough information to replace the destination with a new
one that points to the new physical location of the same material. For another example, consider a
case in which the left side of page 3 of the primary input ends up as page 5 of the output and the
right side of page 3 ends up as page 6. We would have to map destinations from a single source page
to different destination pages based on which part of the page it was on. If part of the rectangle
points to one page and part to another, what do we do? I suggest we go with the top/center of the
rectangle.

A destination consists of a QPDF, page object, and rectangle in user coordinates. When `QPDFJob`
copies a page or converts it to a form XObject, possibly with transformations applied, it will have
to be able to map a destination to the same triple (QPDF, page object, rectangle) on all pages that
contain data from the original page. When writing the final output, any destination that no longer
points anywhere should be dropped, and any destination that points to multiple places will need to
be handled according to some specification.

Whenever we create any new thing from a page, we create _derived page data_. Examples of derived
page data would include a copy of the page and a form XObject created from a page. We will have to
keep a mapping from any source page to all of its derived objects along with any transformations or
clipping. When a derived page data object is placed on a final page, that information can be
combined with the position and any transformations onto the final page to be able to map any
destination to a new one or to determine that it points outside of the visible area. There is
already code in placeFormXObject and the code that places appearance streams that deals with these
kinds of mappings.

What do we do if a source page is copied multiple times? I think we will have to just make the new
destination point to the first place that the target appears with precedence going to the original
location. If we can detect this, we can give a warning.

# Document-level Behavior

Both merging and splitting contain logic, sometimes duplicated, to handle page labels, form fields,
and annotations. We will need to build logic for other things. This section is a rough breakdown of
the different things in the document catalog (plus the info dictionary, which is referenced from the
trailer) and how we may have to handle them. We will need to implement various ObjectHelper and
DocumentHelper classes.

7.7.2 contains the list of all keys in the document catalog.

Document-level structures to merge:
* Extensions
  * Must be combination of Extensions from all input files
* PageLabels
  * Ensure each page has its original label
  * Allow post-processing
* Names -- see below
  * Combine per tree
  * May require disambiguation
  * Page: TemplateInstantiated
* Dests
  * Keep referenced destinations across all files
  * May need to disambiguate or "flatten" or convert to named dests with the names tree
* Outlines
* Threads (easy)
  * Page: B
* AcroForm
* StructTreeRoot
  * Page: StructParents
* MarkInfo (see 14.7 - Logical Structure, 14.8 Tagged PDF)
* SpiderInfo
  * Page: ID
* OutputIntents
  * Page: OutputIntents
* PieceInfo
  * Page: PieceInfo
* OCProperties
* Requirements
* AF (file specification dictionaries)
  * Page: AF
* DPartRoot
  * Page: DPart
* Version
  * Maximum

Things that stay with the first document that has one and/or will not be supported
* AA (Additional Actions)
  * Would be possible to combine and let the first contributor win, but it probably wouldn't usually
    be what we want.
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
* Perms
* Legal
* DSS

Name dictionary (7.7.4)
* Dests
* AP (appearance streams)
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

Most of chapter 12 applies. See Document-level navigation (12.3).

# Feature to Issue Mapping

Last checked: 2023-12-29

```
gh search issues label:pages --repo qpdf/qpdf --limit 200 --state=open
```

* Allow an existing `QPDF` to be an input to a merge or underly/overlay operation when using the
  `QPDFAssembler` C++ API
  * Issues: none
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
* Assign page labels (renumber pages)
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
