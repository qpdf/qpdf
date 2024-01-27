Contents
========

- [Always](#always)
- [Next](#always)
- [Possible future JSON enhancements](#possible-future-json-enhancements)
- [QPDFJob](#qpdfjob)
- [Documentation](#documentation)
- [Document-level work](#document-level-work)
- [Text Appearance Streams](#text-appearance-streams)
- [Fuzz Errors](#fuzz-errors)
- [External Libraries](#external-libraries)
- [ABI Changes](#abi-changes)
- [C++ Version Changes](#c-version-changes)
- [Page splitting/merging](#page-splittingmerging)
- [Analytics](#analytics)
- [General](#general)

- [HISTORICAL NOTES](#historical-notes)

Always
======

* Evaluate issues tagged with `next` and `bug`. Remember to check discussions and pull requests in
  addition to regular issues.
* When close to release, make sure external-libs is building and follow instructions in
  ../external-libs/README

Next
====

* Consider adding fuzzer code for JSON
* Add some additional code coverage analysis to CI
* Spell check: Have the spell-check script synchronize cSpell.json with .idea/dictionaries/qpdf.xml,
  which should be set to the union of all the validated user dictionaries.
* Maybe fix #553 -- use file times for attachments (trivial with C++-20)
* std::string_view transition -- work being done by m-holger
* Support incremental updates. See "incremental updates" in [General](#general). See also issue #22.
  * Make it possible to see incremental updates in qdf mode.
  * Make it possible to add incremental updates.
  * We may want a writing mode that preserves object IDs. See #339.
* Support digital signatures. This probably requires support for incremental updates. First, add
  support for verifying digital signatures. Then we can consider adding support for signing
  documents, though the ability to sign documents is less useful without an interactive process of
  filling in a field. We may want to support only a subset of digital signature with invisible
  signature fields or with existing fields.
* Support public key security handler (Section 7.6.5.)

Possible future JSON enhancements
=================================

* Consider not including unreferenced objects and trimming the trailer in the same way that
  QPDFWriter does (except don't remove `/ID`). This means excluding the linearization dictionary and
  hint stream, the encryption dictionary, all keys from trailer that are removed by
  QPDFWriter::getTrimmedTrailer except `/ID`, any object streams, and the xref stream as long as all
  those objects are unreferenced. (They always should be, but there could be some bizarre case of
  someone creating a PDF file that has an indirect reference to one of those, in which case we need
  to preserve it.) If this is done, make `--preserve-unreferenced` preserve unreference objects and
  also those extra keys. Search for "linear" and "trailer" in json.rst to update the various places
  in the documentation that discuss this. Also update the help for --json and
  --preserve-unreferenced.

* Add to JSON output the information available from a few additional informational options:

  * --check: add but maybe not by default?

  * --show-linearization: add but maybe not by default? Also figure out whether warnings reported
    for some of the PDF specs (1.7) are qpdf problems. This may not be worth adding in the first
    increment.

  * --show-xref: add

* Consider having --check, --show-encryption, etc., just select the right keys when in json mode. I
  don't think I want check on by default, so that might be different.

* Consider having warnings be included in the json in a "warnings" key in json mode.

QPDFJob
=======

Here are some ideas for QPDFJob that didn't make it into 10.6. Not all of these are necessarily
good -- just things to consider.

* How do we chain jobs? The idea would be that the input and/or output of a QPDFJob could be a QPDF
  object rather than a file. For input, it's pretty easy. For output, none of the output-specific
  options (encrypt, compress-streams, objects-streams, etc.) would have any affect, so we would have
  to treat this like inspect for error checking. The QPDF object in the state where it's ready to be
  sent off to QPDFWriter would be used as the input to the next QPDFJob. For the job json, I think
  we can have the output be an identifier that can be used as the input for another QPDFJob. For a
  json file, we could the top level detect if it's an array with the convention that exactly one has
  an output, or we could have a subkey with other job definitions or something. Ideally, any input
  (copy-attachments-from, pages, etc.) could use a QPDF object. It wouldn't surprise me if this
  exposes bugs in qpdf around foreign streams as this has been a relatively fragile area before.

Documentation
=============

* Do a full pass through the documentation.

  * Make sure `qpdf` is consistent. Use QPDF when just referring to the package.
  * Make sure markup is consistent
  * Autogenerate where possible
  * Consider which parts might be good candidates for moving to the wiki.

* Commit 'Manual - enable line wrapping in table cells' from Mon Jan 17 12:22:35 2022 +0000 enables
  table cell wrapping. See if this can be incorporated directly into sphinx_rtd_theme and the
  workaround can be removed.

* When possible, update the debian package to include docs again. See
  https://bugs.debian.org/1004159 for details.

Document-level work
===================

* Ideas here may by superseded by #593.

* QPDFPageCopier -- object for moving pages around within files or between files and performing
  various transformations. Reread/rewrite
  _page-selection in the manual if needed.

  * Handle all the stuff of pages and split-pages
  * Do n-up, booklet, collation
  * Look through cli and see what else...flatten-*?
  * See comments in QPDFPageDocumentHelper.hh for addPage -- search for "a future version".
  * Make it efficient for bulk operations
  * Make certain doc-level features selectable
  * qpdf.cc should do all its page operations, including overlay/underlay, splitting, and merging,
    using this
  * There should also be example code

* After doc-level checks are in, call --check on the output files in the "Copy Annotations" tests.

* Document-level checks. For example, for forms, make sure all form fields point to an annotation on
  exactly one page as well as that all widget annotations are associated with a form field. Hook
  this into QPDFPageCopier as well as the doc helpers. Make sure it is called from --check.

* See also issues tagged with "pages". Include closed issues.

* Add flags to CLI to select which document-level options to preserve or not preserve. We will
  probably need a pair of mutually exclusive, repeatable options with a way to specify all, none,
  only {x,y}, or all but {x,y}.

* If a page contains a reference a file attachment annotation, when that page is copied, if the file
  attachment appears in the top-level EmbeddedFiles tree, that entry should be preserved in the
  destination file. Otherwise, we probably will require the use of --copy-attachments-from to
  preserve these. What will the strategy be for deduplicating in the automatic case?

Text Appearance Streams
=======================

This is a list of known issues with text appearance streams and things we might do about it.

* For variable text, the spec says to pull any resources from /DR that are referenced in /DA but if
  the resource dictionary already has that resource, just use the one that's there. The current code
  looks only for /Tf and adds it if needed. We might want to instead merge /DR with resources and
  then remove anything that's unreferenced. We have all the code required for that in ResourceFinder
  except TfFinder also gets the font size, which ResourceFinder doesn't do.

* There are things we are missing because we don't look at font metrics. The code from TextBuilder
  (work) has almost everything in it that is required. Once we have knowledge of character widths,
  we can support quadding and multiline text fields (/Ff 4096), and we can potentially squeeze text
  to fit into a field. For multiline, first squeeze vertically down to the font height, then squeeze
  horizontally with Tz. For single line, squeeze horizontally with Tz. If we use Tz, issue a
  warning.

* When mapping characters to widths, we will need to care about character encoding. For built-in
  fonts, we can create a map from Unicode code point to width and then go from the font's encoding
  to unicode to the width. See misc/character-encoding/ (not on github)
  and font metric information for the 14 standard fonts in my local pdf-spec directory.

* Once we know about character widths, we can correctly support auto-sized variable text fields
  (0 Tf). If this is fixed, search for "auto-sized" in cli.rst.

Fuzz Errors
===========

* https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=<N>

* Ignoring these:
  * Out of memory in dct: 35001, 32516

External Libraries
==================

Current state (10.0.2):

* qpdf/external-libs repository builds external-libs on a schedule. It detects and downloads the
  latest versions of zlib, jpeg, and openssl and creates source and binary distribution zip files in
  an artifact called "distribution".

* Releases in qpdf/external-libs are made manually. They contain qpdf-external-libs-{bin,src}.zip.

* The qpdf build finds the latest non-prerelease release and downloads the qpdf-external-libs-*.zip
  files from the releases in the setup stage.

* To upgrade to a new version of external-libs, create a new release of qpdf/external-libs (see
  README-maintainer in external-libs) from the distribution artifact of the most recent successful
  build after ensuring that it works.

Desired state:

* The qpdf/external-libs repository should create release candidates. Ideally, every scheduled run
  would make its zip files available. A personal access token with actions:read scope for the
  qpdf/external-libs repository is required to download the artifact from an action run, and
  qpdf/qpdf's secrets.GITHUB_TOKEN doesn't have this access. We could create a service account for
  this purpose. As an alternative, we could have a draft release in qpdf/external-libs that the
  qpdf/external-libs build could update with each candidate. It may also be possible to solve this
  by developing a simple GitHub app.

* Scheduled runs of the qpdf build in the qpdf/qpdf repository (not a fork or pull request) could
  download external-libs from the release candidate area instead of the latest stable release.
  Pushes to the build branch should still use the latest release so it always matches the main
  branch.

* Periodically, we would create a release of external-libs from the release candidate zip files.
  This could be done safely because we know the latest qpdf works with it. This could be done at
  least before every release of qpdf, but potentially it could be done at other times, such as when
  a new dependency version is available or after some period of time.

Other notes:

* The external-libs branch in qpdf/qpdf was never documented. We might be able to get away with
  deleting it.

* See README-maintainer in qpdf/external-libs for information on creating a release. This could be
  at least partially scripted in a way that works for the qpdf/qpdf repository as well since they
  are very similar.

ABI Changes
===========

This is a list of changes to make next time there is an ABI change. Comments appear in the code
prefixed by "ABI".

Always:
* Search for ABI in source and header files
* Search for "[[deprecated" to find deprecated APIs that can be removed
* Search for issues, pull requests, and discussions with the "abi" label
* Check discussion "qpdf X planning" where X is the next major version. This should be tagged `abi`

For qpdf 12, see https://github.com/qpdf/qpdf/discussions/785

C++ Version Changes
===================

Use
```
// C++NN: ...
```
to mark places in the code that should be updated when we require at least that version of C++.

Page splitting/merging
======================

* Update page splitting and merging to handle document-level constructs with page impact such as
  interactive forms and article threading. Check keys in the document catalog for others, such as
  outlines, page labels, thumbnails, and zones. For threads, Subramanyam provided a test file; see
  ../misc/article-threads.pdf. Email Q-Count: 431864 from 2009-11-03.

* bookmarks (outlines) 12.3.3
  * support bookmarks when merging
  * prune bookmarks that don't point to a surviving page when merging or splitting
  * make sure conflicting named destinations work possibly test by including the same file by two
    paths in a merge
  * see also comments in issue 343

  Note: original implementation of bookmark preservation for split pages caused a very high
  performance hit. The problem was introduced in 313ba081265f69ac9a0324f9fe87087c72918191 and
  reverted in the commit that adds this paragraph. The revert includes marking a few tests cases as
  $td->EXPECT_FAILURE. When properly coded, the test cases will need to be adjusted to only include
  the parts of the outlines that are actually copied. The tests in question are
  "split page with outlines". When implementing properly, ensure that the performance is not
  adversely affected by timing split-pages on a large file with complex outlines such as the PDF
  specification.

  When pruning outlines, keep all outlines in the hierarchy that are above an outline for a page we
  care about. If one of the ancestor outlines points to a non-existent page, clear its dest. If an
  outline does not have any children that point to pages in the document, just omit it.

  Possible strategy:
  * resolve all named destinations to explicit destinations
  * concatenate top-level outlines
  * prune outlines whose dests don't point to a valid page
  * recompute all /Count fields

  Test files
  * page-labels-and-outlines.pdf: old file with both page labels and outlines. All destinations are
    explicit destinations. Each page has Potato and a number. All titles are feline names.
  * outlines-with-actions.pdf: mixture of explicit destinations, named destinations, goto actions
    with explicit destinations, and goto actions with named destinations; uses /Dests key in names
    dictionary. Each page has Salad and a number. All titles are silly words. One destination is an
    indirect object.
  * outlines-with-old-root-dests.pdf: like outlines-with-actions except it uses the PDF-1.1 /Dests
    dictionary for named destinations, and each page has Soup and a number. Also pages are numbered
    with upper-case Roman numerals starting with 0. All titles are silly words preceded by a bullet.

  If outline handling is significantly improved, see ../misc/bad-outlines/bad-outlines.pdf and
  email:
  https://mail.google.com/mail/u/0/#search/rfc822msgid%3A02aa01d3d013%249f766990%24de633cb0%24%40mono.hr)

* Form fields: should be similar to outlines.

Analytics
=========

Consider features that make it easier to detect certain patterns in PDF files. The information below
could be computed using an external program that reads the existing json, but if it's useful enough,
we could add it directly to the json output.

* Add to "pages" in the json:
  * "inheritsresources": bool; whether there are any inherited attributes from ancestor page tree
    nodes
  * "sharedresources": a list of indirect objects that are
    "/Resources" dictionaries or "XObject" resource dictionary subkeys of either the page itself or
    of any form XObject referenced by the page.

* Add to "objectinfo" in json: "directpagerefcount": the number of pages that directly reference
  this object (i.e., you can find an indirect reference to the object in the page dictionary without
  traversing over any indirect objects)

General
=======

NOTE: Some items in this list refer to files in my personal home directory or that are otherwise not
publicly accessible. This includes things sent to me by email that are specifically not public. Even
so, I find it useful to make reference to them in this list.

* Provide support in QPDFWriter for writing incremental updates. Provide support in qpdf for
  preserving incremental updates. The goal should be that QDF mode should be fully functional for
  files with incremental updates including fix_qdf. This will work best if original object IDs are
  preserved when a file is written. We will also have to preserve generations, which are, I believe,
  completely ignored by QPDFWriter. If an update adds an object with a higher generation, any
  reference to the object with a lower generation resolves to the null object. Increasing the
  generation represents reusing an object number, while keeping the generation the same is updating
  an object. I think qpdf must handle generations correctly, but make sure to test this carefully.

  Note that there's nothing that says an indirect object in one update can't refer to an object that
  doesn't appear until a later update. This means that QPDF has to hang onto indirect nulls,
  including when they appear as dictionary values. In this case, QPDF_Dictionary::getKeys() ignores
  all keys with null values, and hasKey() returns false for keys that have null values. QPDF_Dictionary
  already handles the special case of keys that are indirect nulls, which is used to reserve foreign
  objects, including foreign pages which may or may not be copied. We also have to make
  sure that the testing for this handles non-trivial cases of the targets of indirect nulls being
  replaced by real objects in an update. Such indirect nulls should appear in tests as dictionary
  values and as array values. In the distant past, qpdf used to replace indirect nulls with direct
  nulls, but I think there are no longer any remnants of that behavior.

  I'm not sure how this plays with linearization, if at all. For cases where incremental updates are
  not being preserved as incremental updates and where the data is being folded in (as is always the
  case with qpdf now), none of this should make any difference in the actual semantics of the files.

  One thought about how to implement this would be to have a QPDF object that is an incremental
  update to an underlying QPDF object. Objects would be resolved from the underlying QPDF if not
  found in the main one. When you write this type of QPDF, it can either flatten or it can write as
  incremental updates. Perhaps, in incremental mode, QPDF reads each increment as a separate QPDF
  with this kind of layering.

* Consider enabling code scanning on GitHub.

* Add an option --ignore-encryption to ignore encryption information and treat encrypted files as if
  they weren't encrypted. This should make it possible to solve #598 (--show-encryption without a
  password). We'll need to make sure we don't try to filter any streams in this mode. Ideally we
  should be able to combine this with --json so we can look at the raw encrypted strings and streams
  if we want to, though be sure to document that the resulting JSON won't be convertible back to a
  valid PDF. Since providing the password may reveal additional details, --show-encryption could
  potentially retry with this option if the first time doesn't work. Then, with the file open, we
  can read the encryption dictionary normally. If this is done, search for "raw, encrypted" in
  json.rst.

* In libtests, separate executables that need the object library from those that strictly use public
  API. Move as many of the test drivers from the qpdf directory into the latter category as long as
  doing so isn't too troublesome from a coverage standpoint.

* Consider generating a non-flat pages tree before creating output to better handle files with lots
  of pages. If there are more than 256 pages, add a second layer with the second layer nodes having
  no more than 256 nodes and being as evenly sizes as possible. Don't worry about the case of more
  than 65,536 pages. If the top node has more than 256 children, we'll live with it. This is only
  safe if all intermediate page nodes have only /Kids, /Parent, /Type, and /Count.

* Look at https://bestpractices.coreinfrastructure.org/en

* Rework tests so that nothing is written into the source directory. Ideally then the entire build
  could be done with a read-only source tree.

* Large file tests fail with linux32 before and after cmake. This was first noticed after 10.6.3. I
  don't think it's worth fixing.

* Consider updating the fuzzer with code that exercises copyAnnotations, file attachments, and name
  and number trees. Check fuzzer coverage.

* Add code for creation of a file attachment annotation. It should also be possible to create a
  widget annotation and a form field. Update the pdf-attach-file.cc example with new APIs when
  ready.

* Flattening of form XObjects seems like something that would be useful in the library. We are
  seeing more cases of completely valid PDF files with form XObjects that cause problems in other
  software. Flattening of form XObjects could be a useful way to work around those issues or to
  prepare files for additional processing, making it possible for users of the qpdf library to not
  be concerned about form XObjects. This could be done recursively; i.e., we could have a method to
  embed a form XObject into whatever contains it, whether that is a form XObject or a page. This
  would require more significant interpretation of the content stream. We would need a test file in
  which the placement of the form XObject has to be in the right place, e.g., the form XObject
  partially obscures earlier code and is partially obscured by later code. Keys in the resource
  dictionary may need to be changed -- create test cases with lots of duplicated/overlapping keys.

* Part of closed_file_input_source.cc is disabled on Windows because of odd failures. It might be
  worth investigating so we can fully exercise this in the test suite. That said,
  ClosedFileInputSource is exercised elsewhere in qpdf's test suite, so this is not that pressing.

* If possible, consider adding CCITT3, CCITT4, or any other easy filters. For some reference code
  that we probably can't use but may be handy anyway, see
  http://partners.adobe.com/public/developer/ps/sdk/index_archive.html

* If possible, support the following types of broken files:

  - Files that have no whitespace token after "endobj" such that endobj collides with the start of
    the next object

  - See ../misc/broken-files

  - See ../misc/bad-files-issue-476. This directory contains a snapshot of the google doc and linked
    PDF files from issue #476. Please see the issue for details.

* Additional form features
  * set value from CLI? Specify title, and provide way to disambiguate, probably by giving objgen of
    field

* Pl_TIFFPredictor is pretty slow.

* Support for handling file names with Unicode characters in Windows is incomplete. qpdf seems to
  support them okay from a functionality standpoint, and the right thing happens if you pass in
  UTF-8 encoded filenames to QPDF library routines in Windows (they are converted internally to
  wchar_t*), but file names are encoded in UTF-8 on output, which doesn't produce nice error
  messages or output on Windows in some cases.

* If we ever wanted to do anything more with character encoding, see ../misc/character-encoding/,
  which includes machine-readable dump of table D.2 in the ISO-32000 PDF spec. This shows the
  mapping between Unicode, StandardEncoding, WinAnsiEncoding, MacRomanEncoding, and PDFDocEncoding.

* Some test cases on bad files fail because qpdf is unable to find the root dictionary when it fails
  to read the trailer. Recovery could find the root dictionary and even the info dictionary in other
  ways. In particular, issue-202.pdf can be opened by evince, and there's no real reason that qpdf
  couldn't be made to be able to recover that file as well.

* Audit every place where qpdf allocates memory to see whether there are cases where malicious
  inputs could cause qpdf to attempt to grab very large amounts of memory. Certainly there are cases
  like this, such as if a very highly compressed, very large image stream is requested in a buffer.
  Hopefully normal input to output filtering doesn't ever try to do this. QPDFWriter should be
  checked carefully too. See also bugs/private/from-email-663916/

* Interactive form modification:
  https://github.com/qpdf/qpdf/issues/213 contains a good discussion of some ideas for adding
  methods to modify annotations and form fields if we want to make it easier to support
  modifications to interactive forms. Some of the ideas have been implemented, and some of the
  probably never will be implemented, but it's worth a read if there is an intention to work on
  this. In the issue, search for "Regarding write functionality", and read that comment and the
  responses to it.

* Look at ~/Q/pdf-collection/forms-from-appian/

* When decrypting files with /R=6, hash_V5 is called more than once with the same inputs. Caching
  the results or refactoring to reduce the number of identical calls could improve performance for
  workloads that involve processing large numbers of small files.

* Consider adding a method to balance the pages tree. It would call pushInheritedAttributesToPage,
  construct a pages tree from scratch, and replace the /Pages key of the root dictionary with the
  new tree.

* Study what's required to support savable forms that can be saved by Adobe Reader. Does this
  require actually signing the document with an Adobe private key? Search for "Digital signatures"
  in the PDF spec, and look at ~/Q/pdf-collection/form-with-full-save.pdf, which came from Adobe's
  example site. See also ../misc/digital-sign-from-trueroad/ and
  ../misc/digital-signatures/digitally-signed-pdf-xfa.pdf. If digital signatures are implemented,
  update the docs on crypto providers, which mention that this may happen in the future.

* Qpdf does not honor /EFF when adding new file attachments. When it encrypts, it never generates
  streams with explicit crypt filters. Prior to 10.2, there was an incorrect attempt to treat /EFF
  as a default value for decrypting file attachment streams, but it is not supposed to mean that.
  Instead, it is intended for conforming writers to obey this when adding new attachments. Qpdf is
  not a conforming writer in that respect.

* The whole xref handling code in the QPDF object allows the same object with more than one
  generation to coexist, but a lot of logic assumes this isn't the case. Anything that creates
  mappings only with the object number and not the generation is this way, including most of the
  interaction between QPDFWriter and QPDF. If we wanted to allow the same object with more than one
  generation to coexist, which I'm not sure is allowed, we could fix this by changing xref_table.
  Alternatively, we could detect and disallow that case. In fact, it appears that Adobe reader and
  other PDF viewing software silently ignores objects of this type, so this is probably not a big
  deal.

* From a suggestion in bug 3152169, consider having an option to re-encode inline images with an
  ASCII encoding.

* From github issue 2, provide more in-depth output for examining hint stream contents. Consider
  adding on option to provide a human-readable dump of linearization hint tables. This should
  include improving the 'overflow reading bit stream' message as reported in issue #2. There are
  multiple calls to stopOnError in the linearization checking code. Ideally, these should not
  terminate checking. It would require re-acquiring an understanding of all that code to make the
  checks more robust. In particular, it's hard to look at the code and quickly determine what is a
  true logic error and what could happen because of malformed user input. See also
  ../misc/linearization-errors.

* If I ever decide to make appearance stream-generation aware of fonts or font metrics, see email
  from Tobias with Message-ID
  <5C3C9C6C.8000102@thax.hardliners.org> dated 2019-01-14.

* Look at places in the code where object traversal is being done and, where possible, try to avoid
  it entirely or at least avoid ever traversing the same objects multiple times.

----------------------------------------------------------------------

### HISTORICAL NOTES

* [Performance](#performance)
* [QPDFPagesTree](#qpdfpagestree)
* [Rejected Ideas](#rejected-ideas)

Performance
===========

As described in https://github.com/qpdf/qpdf/issues/401, there was great performance degradation
between qpdf 7.1.1 and 9.1.1. Doing a bisect between dac65a21fb4fa5f871e31c314280b75adde89a6c and
release-qpdf-7.1.1, I found several commits that damaged performance. I fixed some of them to
improve performance by about 70% (as measured by saying that old times were 170% of new times). The
remaining commits that broke performance either can't be correct because they would re-introduce an
old bug or aren't worth correcting because of the high value they offer relative to a relatively low
penalty. For historical reference, here are the commits. The numbers are the time in seconds on the
machine I happened to be using of splitting the first 100 pages of PDF32000_2008.pdf 20 times and
taking an average duration.

Commits that broke performance:

* d0e99f195a987c483bbb6c5449cf39bee34e08a1 -- object description and context: 0.39 -> 0.45
* a01359189b32c60c2d55b039f7aefd6c3ce0ebde (minus 313ba08) -- fix dangling references: 0.55 -> 0.6
* e5f504b6c5dc34337cc0b316b4a7b1fca7e614b1 -- sparse array: 0.6 -> 0.62

Other intermediate steps that were previously fixed:

* 313ba081265f69ac9a0324f9fe87087c72918191 -- copy outlines into split: 0.55 -> 4.0
* a01359189b32c60c2d55b039f7aefd6c3ce0ebde -- fix dangling references:
  4.0 -> 9.0

This commit fixed the awful problem introduced in 313ba081:

* a5a016cdd26a8e5c99e5f019bc30d1bdf6c050a2 -- revert outline preservation: 9.0 -> 0.6

Note that the fix dangling references commit had a much worse impact prior to removing the outline
preservation, so I also measured its impact in isolation.

A few important lessons (in README-maintainer)

* Indirection through PointerHolder<Members> is expensive, and should not be used for things that
  are created and destroyed frequently such as QPDFObjectHandle and QPDFObject.
* Traversal of objects is expensive and should be avoided where possible.

Also, it turns out that PointerHolder is more performant than std::shared_ptr. (This was true at the
time but subsequent implementations of std::shared_ptr became much more efficient.)

QPDFPagesTree
=============

On a few occasions, I have considered implementing a QPDFPagesTree object that would allow the
document's original page tree structure to be preserved. See comments at the top QPDF_pages.cc for
why this was abandoned.

Partial work is in refs/attic/QPDFPagesTree. QPDFPageTree is mostly implemented and mostly tested.
There are not enough cases of different kinds of operations (pclm, linearize, json, etc.) with
non-flat pages trees. Insertion is not implemented. Insertion is potentially complex because of the
issue of inherited objects. We will have to call pushInheritedAttributesToPage before adding any
pages to the pages tree. The test suite is failing on that branch.

Some parts of page tree repair are silent (no warnings). All page tree repair should warn. The
reason is that page tree repair will change object numbers, and knowing that is important when
working with JSON output.

If we were to do this, we would still need keep a pages cache for efficient insertion. There's no
reason we can't keep a vector of page objects up to date and just do a traversal the first time we
do getAllPages just like we do now. The difference is that we would not flatten the pages tree. It
would be useful to go through QPDF_pages and reimplement everything without calling
flattenPagesTree. Then we can remove flattenPagesTree, which is private. That said, with the
addition of creating non-flat pages trees, there is really no reason not to flatten the pages tree
for internal use.

In its current state, QPDFPagesTree does not proactively fix /Type or correct page objects that are
used multiple times. You have to traverse the pages tree to trigger this operation. It would be nice
if we would do that somewhere but not do it more often than necessary so isPagesObject and
isPageObject are reliable and can be made more reliable. Maybe add a validate or repair function? It
should also make sure /Count and /Parent are correct.

Rejected Ideas
==============

* Investigate whether there is a way to automate the memory checker tests for Windows.

* The second xref stream for linearized files has to be padded only because we need file_size as
  computed in pass 1 to be accurate. If we were not allowing writing to a pipe, we could seek back
  to the beginning and fill in the value of /L in the linearization dictionary as an optimization to
  alleviate the need for this padding. Doing so would require us to pad the /L value individually
  and also to save the file descriptor and determine whether it's seekable. This is probably not
  worth bothering with.

* Based on an idea suggested by user "Atom Smasher", consider providing some mechanism to recover
  earlier versions of a file embedded prior to appended sections.

* Consider creating a sanitizer to make it easier for people to send broken files. Now that we have
  json mode, this is probably no longer worth doing. Here is the previous idea, possibly implemented
  by making it possible to run the lexer (tokenizer) over a whole file. Make it possible to replace
  all strings in a file lexically even on badly broken files. Ideally this should work files that
  are lacking xref, have broken links, duplicated dictionary keys, syntax errors, etc., and ideally
  it should work with encrypted files if possible. This should go through the streams and strings
  and replace them with fixed or random characters, preferably, but not necessarily, in a manner
  that works with fonts. One possibility would be to detect whether a string contains characters
  with normal encoding, and if so, use 0x41. If the string uses character maps, use 0x01. The output
  should otherwise be unrelated to the input. This could be built after the filtering and tokenizer
  rewrite and should be done in a manner that takes advantage of the other lexical features. This
  sanitizer should also clear metadata and replace images. If I ever do this, the file from issue
  #494 would be a great one to look at.

* Here are some notes about having stream data providers modify stream dictionaries. I had wanted to
  add this functionality to make it more efficient to create stream data providers that may
  dynamically decide what kind of filters to use and that may end up modifying the dictionary
  conditionally depending on the original stream data. Ultimately I decided not to implement this
  feature. This paragraph describes why.

  * When writing, the way objects are placed into the queue for writing strongly precludes creation
    of any new indirect objects, or even changing which indirect objects are referenced from which
    other objects, because we sometimes write as we are traversing and enqueuing objects. For
    non-linearized files, there is a risk that an indirect object that used to be referenced would
    no longer be referenced, and whether it was already written to the output file would be based on
    an accident of where it was encountered when traversing the object structure. For linearized
    files, the situation is considerably worse. We decide which section of the file to write an
    object to based on a mapping of which objects are used by which other objects. Changing this
    mapping could cause an object to appear in the wrong section, to be written even though it is
    unreferenced, or to be entirely omitted since, during linearization, we don't enqueue new
    objects as we traverse for writing.

  * There are several places in QPDFWriter that query a stream's dictionary in order to prepare for
    writing or to make decisions about certain aspects of the writing process. If the stream data
    provider has the chance to modify the dictionary, every piece of code that gets stream data
    would have to be aware of this. This would potentially include end user code. For example, any
    code that called getDict() on a stream before installing a stream data provider and expected
    that dictionary to be valid would potentially be broken. As implemented right now, you must
    perform any modifications on the dictionary in advance and provided /Filter and /DecodeParms at
    the time you installed the stream data provider. This means that some computations would have to
    be done more than once, but for linearized files, stream data providers are already called more
    than once. If the work done by a stream data provider is especially expensive, it can implement
    its own cache.

  The example examples/pdf-custom-filter.cc demonstrates the use of custom stream filters. This
  includes a custom pipeline, a custom stream filter, as well as modification of a stream's
  dictionary to include creation of a new stream that is referenced from /DecodeParms.

* Removal of raw QPDF* from the API. Discussions in #747 and #754. This is a summary of the
  arguments I put forth in #754. The idea was to make QPDF::QPDF() private and require all QPDF
  objects to be shared pointers created with QPDF::create(). This would enable us to have
  QPDFObjectHandle::getOwningQPDF() return a std::weak_ptr<QPDF>. Prior to #726 (
  QPDFObject/QPDFValue split, released in qpdf 11.0.0), getOwningQPDF() could return an invalid
  pointer if the owning QPDF disappeared, but this is no longer the case, which removes the main
  motivation. QPDF 11 added QPDF::create() anyway though.

  Removing raw QPDF* would look something like this. Note that you can't use std::make_shared<T>
  unless T has a public constructor.

  QPDF_POINTER_TRANSITION = 0 -- no warnings around calling the QPDF constructor
  QPDF_POINTER_TRANSITION = 1 -- calls to QPDF() are deprecated, but QPDF is still available so code
                                 can be backward compatible and use std::make_shared<QPDF>
  QPDF_POINTER_TRANSITION = 2 -- the QPDF constructor is private; all calls to
                                 std::make_shared<QPDF> have to be replaced with QPDF::create

  If we were to do this, we'd have to look at each use of QPDF* in the interface and decide whether
  to use a std::shared_ptr or a std::weak_ptr. The answer would almost always be to use a std::
  weak_ptr, which means we'd have to take the extra step of calling lock(), and it means there would
  be lots of code changes cause people would have to pass weak pointers instead of raw pointers
  around, and those have to be constructed and locked. Passing std::shared_ptr around leaves the
  possibility of creating circular references. It seems to be too much trouble in the library and
  too much toil for library users to be worth the small benefit of not having to call resetObjGen in
  QPDF's destructor.

* Fix Multiple Direct Object Parent Issue

  This idea was rejected because it would be complicated to implement and would likely have a high
  performance cost to fix what is not really that big of a problem in practice.

  It is possible for a QPDFObjectHandle for a direct object to be contained inside of multiple
  QPDFObjectHandle objects or even replicated across multiple QPDF objects. This creates a
  potentially confusing and unintentional aliasing of direct objects. There are known cases in the
  qpdf library where this happens including page splitting and merging (particularly with page
  labels, and possibly with other cases), and also with unsafeShallowCopy. Disallowing this would
  incur a significant performance penalty and is probably not worth doing. If we were to do it, here
  are some ideas.

  * Add std::weak_ptr<QPDFObject> parent to QPDFObject. When adding a direct object to an array or
    dictionary, set its parent. When removing it, clear the parent pointer. The parent pointer would
    always be null for indirect objects, so the parent pointer, which would reside in QPDFObject,
    would have to be managed by QPDFObjectHandle. This is because QPDFObject can't tell the
    difference between a resolved indirect object and a direct object.

  * Phase 1: When a direct object that already has a parent is added to a dictionary or array, issue
    a warning. There would need to be unsafe add methods used by unsafeShallowCopy. These would add
    but not modify the parent pointer.

  * Phase 2: In the next major release, make the multiple parent case an error. Require people to
    create a copy. The unsafe operations would still have to be permitted.

  This approach would allow an object to be moved from one object to another by removing it, which
  returns the now orphaned object, and then inserting it somewhere else. It also doesn't break the
  pattern of adding a direct object to something and subsequently mutating it. It just prevents the
  same object from being added to more than one thing.
