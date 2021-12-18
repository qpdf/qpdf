.. _using:

Running QPDF
============

This chapter describes how to run the qpdf program from the command
line.

.. _invocation:

Basic Invocation
----------------

When running qpdf, the basic invocation is as follows:

::

   qpdf [ options ] { infilename | --empty } outfilename

This converts PDF file :samp:`infilename` to PDF file
:samp:`outfilename`. The output file is functionally
identical to the input file but may have been structurally reorganized.
Also, orphaned objects will be removed from the file. Many
transformations are available as controlled by the options below. In
place of :samp:`infilename`, the parameter
:samp:`--empty` may be specified. This causes qpdf to
use a dummy input file that contains zero pages. The only normal use
case for using :samp:`--empty` would be if you were
going to add pages from another source, as discussed in :ref:`page-selection`.

If :samp:`@filename` appears as a word anywhere in the
command-line, it will be read line by line, and each line will be
treated as a command-line argument. Leading and trailing whitespace is
intentionally not removed from lines, which makes it possible to handle
arguments that start or end with spaces. The :samp:`@-`
option allows arguments to be read from standard input. This allows qpdf
to be invoked with an arbitrary number of arbitrarily long arguments. It
is also very useful for avoiding having to pass passwords on the command
line. Note that the :samp:`@filename` can't appear in
the middle of an argument, so constructs such as
:samp:`--arg=@option` will not work. You would have to
include the argument and its options together in the arguments file.

:samp:`outfilename` does not have to be seekable, even
when generating linearized files. Specifying ":samp:`-`"
as :samp:`outfilename` means to write to standard
output. If you want to overwrite the input file with the output, use the
option :samp:`--replace-input` and omit the output file
name. You can't specify the same file as both the input and the output.
If you do this, qpdf will tell you about the
:samp:`--replace-input` option.

Most options require an output file, but some testing or inspection
commands do not. These are specifically noted.

.. _exit-status:

Exit Status
~~~~~~~~~~~

The exit status of :command:`qpdf` may be interpreted as
follows:

- ``0``: no errors or warnings were found. The file may still have
  problems qpdf can't detect. If
  :samp:`--warning-exit-0` was specified, exit status 0
  is used even if there are warnings.

- ``2``: errors were found. qpdf was not able to fully process the
  file.

- ``3``: qpdf encountered problems that it was able to recover from. In
  some cases, the resulting file may still be damaged. Note that qpdf
  still exits with status ``3`` if it finds warnings even when
  :samp:`--no-warn` is specified. With
  :samp:`--warning-exit-0`, warnings without errors
  exit with status 0 instead of 3.

Note that :command:`qpdf` never exists with status ``1``.
If you get an exit status of ``1``, it was something else, like the
shell not being able to find or execute :command:`qpdf`.

.. _shell-completion:

Shell Completion
----------------

Starting in qpdf version 8.3.0, qpdf provides its own completion support
for zsh and bash. You can enable bash completion with :command:`eval
$(qpdf --completion-bash)` and zsh completion with
:command:`eval $(qpdf --completion-zsh)`. If
:command:`qpdf` is not in your path, you should invoke it
above with an absolute path. If you invoke it with a relative path, it
will warn you, and the completion won't work if you're in a different
directory.

qpdf will use ``argv[0]`` to figure out where its executable is. This
may produce unwanted results in some cases, especially if you are trying
to use completion with copy of qpdf that is built from source. You can
specify a full path to the qpdf you want to use for completion in the
``QPDF_EXECUTABLE`` environment variable.

.. _basic-options:

Basic Options
-------------

The following options are the most common ones and perform commonly
needed transformations.

:samp:`--help`
   Display command-line invocation help.

:samp:`--version`
   Display the current version of qpdf.

:samp:`--copyright`
   Show detailed copyright information.

:samp:`--show-crypto`
   Show a list of available crypto providers, each on a line by itself.
   The default provider is always listed first. See :ref:`crypto` for more information about crypto
   providers.

:samp:`--completion-bash`
   Output a completion command you can eval to enable shell completion
   from bash.

:samp:`--completion-zsh`
   Output a completion command you can eval to enable shell completion
   from zsh.

:samp:`--password={password}`
   Specifies a password for accessing encrypted files. To read the
   password from a file or standard input, you can use
   :samp:`--password-file`, added in qpdf 10.2. Note
   that you can also use :samp:`@filename` or
   :samp:`@-` as described above to put the password in
   a file or pass it via standard input, but you would do so by
   specifying the entire
   :samp:`--password={password}`
   option in the file. Syntax such as
   :samp:`--password=@filename` won't work since
   :samp:`@filename` is not recognized in the middle of
   an argument.

:samp:`--password-file={filename}`
   Reads the first line from the specified file and uses it as the
   password for accessing encrypted files.
   :samp:`{filename}`
   may be ``-`` to read the password from standard input. Note that, in
   this case, the password is echoed and there is no prompt, so use with
   caution.

:samp:`--is-encrypted`
   Silently exit with status 0 if the file is encrypted or status 2 if
   the file is not encrypted. This is useful for shell scripts. Other
   options are ignored if this is given. This option is mutually
   exclusive with :samp:`--requires-password`. Both this
   option and :samp:`--requires-password` exit with
   status 2 for non-encrypted files.

:samp:`--requires-password`
   Silently exit with status 0 if a password (other than as supplied) is
   required. Exit with status 2 if the file is not encrypted. Exit with
   status 3 if the file is encrypted but requires no password or the
   correct password has been supplied. This is useful for shell scripts.
   Note that any supplied password is used when opening the file. When
   used with a :samp:`--password` option, this option
   can be used to check the correctness of the password. In that case,
   an exit status of 3 means the file works with the supplied password.
   This option is mutually exclusive with
   :samp:`--is-encrypted`. Both this option and
   :samp:`--is-encrypted` exit with status 2 for
   non-encrypted files.

:samp:`--verbose`
   Increase verbosity of output. For now, this just prints some
   indication of any file that it creates.

:samp:`--progress`
   Indicate progress while writing files.

:samp:`--no-warn`
   Suppress writing of warnings to stderr. If warnings were detected and
   suppressed, :command:`qpdf` will still exit with exit
   code 3. See also :samp:`--warning-exit-0`.

:samp:`--warning-exit-0`
   If warnings are found but no errors, exit with exit code 0 instead 3.
   When combined with :samp:`--no-warn`, the effect is
   for :command:`qpdf` to completely ignore warnings.

:samp:`--linearize`
   Causes generation of a linearized (web-optimized) output file.

:samp:`--replace-input`
   If specified, the output file name should be omitted. This option
   tells qpdf to replace the input file with the output. It does this by
   writing to
   :file:`{infilename}.~qpdf-temp#`
   and, when done, overwriting the input file with the temporary file.
   If there were any warnings, the original input is saved as
   :file:`{infilename}.~qpdf-orig`.

:samp:`--copy-encryption=file`
   Encrypt the file using the same encryption parameters, including user
   and owner password, as the specified file. Use
   :samp:`--encryption-file-password` to specify a
   password if one is needed to open this file. Note that copying the
   encryption parameters from a file also copies the first half of
   ``/ID`` from the file since this is part of the encryption
   parameters.

:samp:`--encryption-file-password=password`
   If the file specified with :samp:`--copy-encryption`
   requires a password, specify the password using this option. Note
   that only one of the user or owner password is required. Both
   passwords will be preserved since QPDF does not distinguish between
   the two passwords. It is possible to preserve encryption parameters,
   including the owner password, from a file even if you don't know the
   file's owner password.

:samp:`--allow-weak-crypto`
   Starting with version 10.4, qpdf issues warnings when requested to
   create files using RC4 encryption. This option suppresses those
   warnings. In future versions of qpdf, qpdf will refuse to create
   files with weak cryptography when this flag is not given. See :ref:`weak-crypto` for additional details.

:samp:`--encrypt options --`
   Causes generation an encrypted output file. Please see :ref:`encryption-options` for details on how to specify
   encryption parameters.

:samp:`--decrypt`
   Removes any encryption on the file. A password must be supplied if
   the file is password protected.

:samp:`--password-is-hex-key`
   Overrides the usual computation/retrieval of the PDF file's
   encryption key from user/owner password with an explicit
   specification of the encryption key. When this option is specified,
   the argument to the :samp:`--password` option is
   interpreted as a hexadecimal-encoded key value. This only applies to
   the password used to open the main input file. It does not apply to
   other files opened by :samp:`--pages` or other
   options or to files being written.

   Most users will never have a need for this option, and no standard
   viewers support this mode of operation, but it can be useful for
   forensic or investigatory purposes. For example, if a PDF file is
   encrypted with an unknown password, a brute-force attack using the
   key directly is sometimes more efficient than one using the password.
   Also, if a file is heavily damaged, it may be possible to derive the
   encryption key and recover parts of the file using it directly. To
   expose the encryption key used by an encrypted file that you can open
   normally, use the :samp:`--show-encryption-key`
   option.

:samp:`--suppress-password-recovery`
   Ordinarily, qpdf attempts to automatically compensate for passwords
   specified in the wrong character encoding. This option suppresses
   that behavior. Under normal conditions, there are no reasons to use
   this option. See :ref:`unicode-passwords` for a
   discussion

:samp:`--password-mode={mode}`
   This option can be used to fine-tune how qpdf interprets Unicode
   (non-ASCII) password strings passed on the command line. With the
   exception of the :samp:`hex-bytes` mode, these only
   apply to passwords provided when encrypting files. The
   :samp:`hex-bytes` mode also applies to passwords
   specified for reading files. For additional discussion of the
   supported password modes and when you might want to use them, see
   :ref:`unicode-passwords`. The following modes
   are supported:

   - :samp:`auto`: Automatically determine whether the
     specified password is a properly encoded Unicode (UTF-8) string,
     and transcode it as required by the PDF spec based on the type
     encryption being applied. On Windows starting with version 8.4.0,
     and on almost all other modern platforms, incoming passwords will
     be properly encoded in UTF-8, so this is almost always what you
     want.

   - :samp:`unicode`: Tells qpdf that the incoming
     password is UTF-8, overriding whatever its automatic detection
     determines. The only difference between this mode and
     :samp:`auto` is that qpdf will fail with an error
     message if the password is not valid UTF-8 instead of falling back
     to :samp:`bytes` mode with a warning.

   - :samp:`bytes`: Interpret the password as a literal
     byte string. For non-Windows platforms, this is what versions of
     qpdf prior to 8.4.0 did. For Windows platforms, there is no way to
     specify strings of binary data on the command line directly, but
     you can use the :samp:`@filename` option to do it,
     in which case this option forces qpdf to respect the string of
     bytes as provided. This option will allow you to encrypt PDF files
     with passwords that will not be usable by other readers.

   - :samp:`hex-bytes`: Interpret the password as a
     hex-encoded string. This provides a way to pass binary data as a
     password on all platforms including Windows. As with
     :samp:`bytes`, this option may allow creation of
     files that can't be opened by other readers. This mode affects
     qpdf's interpretation of passwords specified for decrypting files
     as well as for encrypting them. It makes it possible to specify
     strings that are encoded in some manner other than the system's
     default encoding.

:samp:`--rotate=[+|-]angle[:page-range]`
   Apply rotation to specified pages. The
   :samp:`page-range` portion of the option value has
   the same format as page ranges in :ref:`page-selection`. If the page range is omitted, the
   rotation is applied to all pages. The :samp:`angle`
   portion of the parameter may be either 0, 90, 180, or 270. If
   preceded by :samp:`+` or :samp:`-`,
   the angle is added to or subtracted from the specified pages'
   original rotations. This is almost always what you want. Otherwise
   the pages' rotations are set to the exact value, which may cause the
   appearances of the pages to be inconsistent, especially for scans.
   For example, the command :command:`qpdf in.pdf out.pdf
   --rotate=+90:2,4,6 --rotate=180:7-8` would rotate pages
   2, 4, and 6 90 degrees clockwise from their original rotation and
   force the rotation of pages 7 through 8 to 180 degrees regardless of
   their original rotation, and the command :command:`qpdf in.pdf
   out.pdf --rotate=+180` would rotate all pages by 180
   degrees.

:samp:`--keep-files-open={[yn]}`
   This option controls whether qpdf keeps individual files open while
   merging. Prior to version 8.1.0, qpdf always kept all files open, but
   this meant that the number of files that could be merged was limited
   by the operating system's open file limit. Version 8.1.0 opened files
   as they were referenced and closed them after each read, but this
   caused a major performance impact. Version 8.2.0 optimized the
   performance but did so in a way that, for local file systems, there
   was a small but unavoidable performance hit, but for networked file
   systems, the performance impact could be very high. Starting with
   version 8.2.1, the default behavior is that files are kept open if no
   more than 200 files are specified, but this default behavior can be
   explicitly overridden with the
   :samp:`--keep-files-open` flag. If you are merging
   more than 200 files but less than the operating system's max open
   files limit, you may want to use
   :samp:`--keep-files-open=y`, especially if working
   over a networked file system. If you are using a local file system
   where the overhead is low and you might sometimes merge more than the
   OS limit's number of files from a script and are not worried about a
   few seconds additional processing time, you may want to specify
   :samp:`--keep-files-open=n`. The threshold for
   switching may be changed from the default 200 with the
   :samp:`--keep-files-open-threshold` option.

:samp:`--keep-files-open-threshold={count}`
   If specified, overrides the default value of 200 used as the
   threshold for qpdf deciding whether or not to keep files open. See
   :samp:`--keep-files-open` for details.

:samp:`--pages options --`
   Select specific pages from one or more input files. See :ref:`page-selection` for details on how to do
   page selection (splitting and merging).

:samp:`--collate={n}`
   When specified, collate rather than concatenate pages from files
   specified with :samp:`--pages`. With a numeric
   argument, collate in groups of :samp:`{n}`.
   The default is 1. See :ref:`page-selection` for additional details.

:samp:`--flatten-rotation`
   For each page that is rotated using the ``/Rotate`` key in the page's
   dictionary, remove the ``/Rotate`` key and implement the identical
   rotation semantics by modifying the page's contents. This option can
   be useful to prepare files for buggy PDF applications that don't
   properly handle rotated pages.

:samp:`--split-pages=[n]`
   Write each group of :samp:`n` pages to a separate
   output file. If :samp:`n` is not specified, create
   single pages. Output file names are generated as follows:

   - If the string ``%d`` appears in the output file name, it is
     replaced with a range of zero-padded page numbers starting from 1.

   - Otherwise, if the output file name ends in
     :file:`.pdf` (case insensitive), a zero-padded
     page range, preceded by a dash, is inserted before the file
     extension.

   - Otherwise, the file name is appended with a zero-padded page range
     preceded by a dash.

   Page ranges are a single number in the case of single-page groups or
   two numbers separated by a dash otherwise. For example, if
   :file:`infile.pdf` has 12 pages

   - :command:`qpdf --split-pages infile.pdf %d-out`
     would generate files :file:`01-out` through
     :file:`12-out`

   - :command:`qpdf --split-pages=2 infile.pdf
     outfile.pdf` would generate files
     :file:`outfile-01-02.pdf` through
     :file:`outfile-11-12.pdf`

   - :command:`qpdf --split-pages infile.pdf
     something.else` would generate files
     :file:`something.else-01` through
     :file:`something.else-12`

   Note that outlines, threads, and other global features of the
   original PDF file are not preserved. For each page of output, this
   option creates an empty PDF and copies a single page from the output
   into it. If you require the global data, you will have to run
   :command:`qpdf` with the
   :samp:`--pages` option once for each file. Using
   :samp:`--split-pages` is much faster if you don't
   require the global data.

:samp:`--overlay options --`
   Overlay pages from another file onto the output pages. See :ref:`overlay-underlay` for details on
   overlay/underlay.

:samp:`--underlay options --`
   Overlay pages from another file onto the output pages. See :ref:`overlay-underlay` for details on
   overlay/underlay.

Password-protected files may be opened by specifying a password. By
default, qpdf will preserve any encryption data associated with a file.
If :samp:`--decrypt` is specified, qpdf will attempt to
remove any encryption information. If :samp:`--encrypt`
is specified, qpdf will replace the document's encryption parameters
with whatever is specified.

Note that qpdf does not obey encryption restrictions already imposed on
the file. Doing so would be meaningless since qpdf can be used to remove
encryption from the file entirely. This functionality is not intended to
be used for bypassing copyright restrictions or other restrictions
placed on files by their producers.

Prior to 8.4.0, in the case of passwords that contain characters that
fall outside of 7-bit US-ASCII, qpdf left the burden of supplying
properly encoded encryption and decryption passwords to the user.
Starting in qpdf 8.4.0, qpdf does this automatically in most cases. For
an in-depth discussion, please see :ref:`unicode-passwords`. Previous versions of this manual
described workarounds using the :command:`iconv` command.
Such workarounds are no longer required or recommended with qpdf 8.4.0.
However, for backward compatibility, qpdf attempts to detect those
workarounds and do the right thing in most cases.

.. _encryption-options:

Encryption Options
------------------

To change the encryption parameters of a file, use the --encrypt flag.
The syntax is

::

   --encrypt user-password owner-password key-length [ restrictions ] --

Note that ":samp:`--`" terminates parsing of encryption
flags and must be present even if no restrictions are present.

Either or both of the user password and the owner password may be empty
strings. Starting in qpdf 10.2, qpdf defaults to not allowing creation
of PDF files with a non-empty user password, an empty owner password,
and a 256-bit key since such files can be opened with no password. If
you want to create such files, specify the encryption option
:samp:`--allow-insecure`, as described below.

The value for
:samp:`{key-length}` may
be 40, 128, or 256. The restriction flags are dependent upon key length.
When no additional restrictions are given, the default is to be fully
permissive.

If :samp:`{key-length}`
is 40, the following restriction options are available:

:samp:`--print=[yn]`
   Determines whether or not to allow printing.

:samp:`--modify=[yn]`
   Determines whether or not to allow document modification.

:samp:`--extract=[yn]`
   Determines whether or not to allow text/image extraction.

:samp:`--annotate=[yn]`
   Determines whether or not to allow comments and form fill-in and
   signing.

If :samp:`{key-length}`
is 128, the following restriction options are available:

:samp:`--accessibility=[yn]`
   Determines whether or not to allow accessibility to visually
   impaired. The qpdf library disregards this field when AES is used or
   when 256-bit encryption is used. You should really never disable
   accessibility, but qpdf lets you do it in case you need to configure
   a file this way for testing purposes. The PDF spec says that
   conforming readers should disregard this permission and always allow
   accessibility.

:samp:`--extract=[yn]`
   Determines whether or not to allow text/graphic extraction.

:samp:`--assemble=[yn]`
   Determines whether document assembly (rotation and reordering of
   pages) is allowed.

:samp:`--annotate=[yn]`
   Determines whether modifying annotations is allowed. This includes
   adding comments and filling in form fields. Also allows editing of
   form fields if :samp:`--modify-other=y` is given.

:samp:`--form=[yn]`
   Determines whether filling form fields is allowed.

:samp:`--modify-other=[yn]`
   Allow all document editing except those controlled separately by the
   :samp:`--assemble`,
   :samp:`--annotate`, and
   :samp:`--form` options.

:samp:`--print={print-opt}`
   Controls printing access.
   :samp:`{print-opt}`
   may be one of the following:

   - :samp:`full`: allow full printing

   - :samp:`low`: allow low-resolution printing only

   - :samp:`none`: disallow printing

:samp:`--modify={modify-opt}`
   Controls modify access. This way of controlling modify access has
   less granularity than new options added in qpdf 8.4.
   :samp:`{modify-opt}`
   may be one of the following:

   - :samp:`all`: allow full document modification

   - :samp:`annotate`: allow comment authoring, form
     operations, and document assembly

   - :samp:`form`: allow form field fill-in and signing
     and document assembly

   - :samp:`assembly`: allow document assembly only

   - :samp:`none`: allow no modifications

   Using the :samp:`--modify` option does not allow you
   to create certain combinations of permissions such as allowing form
   filling but not allowing document assembly. Starting with qpdf 8.4,
   you can either just use the other options to control fields
   individually, or you can use something like :samp:`--modify=form
   --assembly=n` to fine tune.

:samp:`--cleartext-metadata`
   If specified, any metadata stream in the document will be left
   unencrypted even if the rest of the document is encrypted. This also
   forces the PDF version to be at least 1.5.

:samp:`--use-aes=[yn]`
   If :samp:`--use-aes=y` is specified, AES encryption
   will be used instead of RC4 encryption. This forces the PDF version
   to be at least 1.6.

:samp:`--allow-insecure`
   From qpdf 10.2, qpdf defaults to not allowing creation of PDF files
   where the user password is non-empty, the owner password is empty,
   and a 256-bit key is in use. Files created in this way are insecure
   since they can be opened without a password. Users would ordinarily
   never want to create such files. If you are using qpdf to
   intentionally created strange files for testing (a definite valid use
   of qpdf!), this option allows you to create such insecure files.

:samp:`--force-V4`
   Use of this option forces the ``/V`` and ``/R`` parameters in the
   document's encryption dictionary to be set to the value ``4``. As
   qpdf will automatically do this when required, there is no reason to
   ever use this option. It exists primarily for use in testing qpdf
   itself. This option also forces the PDF version to be at least 1.5.

If :samp:`{key-length}`
is 256, the minimum PDF version is 1.7 with extension level 8, and the
AES-based encryption format used is the PDF 2.0 encryption method
supported by Acrobat X. the same options are available as with 128 bits
with the following exceptions:

:samp:`--use-aes`
   This option is not available with 256-bit keys. AES is always used
   with 256-bit encryption keys.

:samp:`--force-V4`
   This option is not available with 256 keys.

:samp:`--force-R5`
   If specified, qpdf sets the minimum version to 1.7 at extension level
   3 and writes the deprecated encryption format used by Acrobat version
   IX. This option should not be used in practice to generate PDF files
   that will be in general use, but it can be useful to generate files
   if you are trying to test proper support in another application for
   PDF files encrypted in this way.

The default for each permission option is to be fully permissive.

.. _page-selection:

Page Selection Options
----------------------

Starting with qpdf 3.0, it is possible to split and merge PDF files by
selecting pages from one or more input files. Whatever file is given as
the primary input file is used as the starting point, but its pages are
replaced with pages as specified.

::

   --pages input-file [ --password=password ] [ page-range ] [ ... ] --

Multiple input files may be specified. Each one is given as the name of
the input file, an optional password (if required to open the file), and
the range of pages. Note that ":samp:`--`" terminates
parsing of page selection flags.

Starting with qpf 8.4, the special input file name
":file:`.`" can be used as a shortcut for the
primary input filename.

For each file that pages should be taken from, specify the file, a
password needed to open the file (if any), and a page range. The
password needs to be given only once per file. If any of the input files
are the same as the primary input file or the file used to copy
encryption parameters (if specified), you do not need to repeat the
password here. The same file can be repeated multiple times. If a file
that is repeated has a password, the password only has to be given the
first time. All non-page data (info, outlines, page numbers, etc.) are
taken from the primary input file. To discard these, use
:samp:`--empty` as the primary input.

Starting with qpdf 5.0.0, it is possible to omit the page range. If qpdf
sees a value in the place where it expects a page range and that value
is not a valid range but is a valid file name, qpdf will implicitly use
the range ``1-z``, meaning that it will include all pages in the file.
This makes it possible to easily combine all pages in a set of files
with a command like :command:`qpdf --empty out.pdf --pages \*.pdf
--`.

The page range is a set of numbers separated by commas, ranges of
numbers separated dashes, or combinations of those. The character "z"
represents the last page. A number preceded by an "r" indicates to count
from the end, so ``r3-r1`` would be the last three pages of the
document. Pages can appear in any order. Ranges can appear with a high
number followed by a low number, which causes the pages to appear in
reverse. Numbers may be repeated in a page range. A page range may be
optionally appended with ``:even`` or ``:odd`` to indicate only the even
or odd pages in the given range. Note that even and odd refer to the
positions within the specified, range, not whether the original number
is even or odd.

Example page ranges:

- ``1,3,5-9,15-12``: pages 1, 3, 5, 6, 7, 8, 9, 15, 14, 13, and 12 in
  that order.

- ``z-1``: all pages in the document in reverse

- ``r3-r1``: the last three pages of the document

- ``r1-r3``: the last three pages of the document in reverse order

- ``1-20:even``: even pages from 2 to 20

- ``5,7-9,12:odd``: pages 5, 8, and, 12, which are the pages in odd
  positions from among the original range, which represents pages 5, 7,
  8, 9, and 12.

Starting in qpdf version 8.3, you can specify the
:samp:`--collate` option. Note that this option is
specified outside of :samp:`--pages ... --`. When
:samp:`--collate` is specified, it changes the meaning
of :samp:`--pages` so that the specified files, as
modified by page ranges, are collated rather than concatenated. For
example, if you add the files :file:`odd.pdf` and
:file:`even.pdf` containing odd and even pages of a
document respectively, you could run :command:`qpdf --collate odd.pdf
--pages odd.pdf even.pdf -- all.pdf` to collate the pages.
This would pick page 1 from odd, page 1 from even, page 2 from odd, page
2 from even, etc. until all pages have been included. Any number of
files and page ranges can be specified. If any file has fewer pages,
that file is just skipped when its pages have all been included. For
example, if you ran :command:`qpdf --collate --empty --pages a.pdf
1-5 b.pdf 6-4 c.pdf r1 -- out.pdf`, you would get the
following pages in this order:

- a.pdf page 1

- b.pdf page 6

- c.pdf last page

- a.pdf page 2

- b.pdf page 5

- a.pdf page 3

- b.pdf page 4

- a.pdf page 4

- a.pdf page 5

Starting in qpdf version 10.2, you may specify a numeric argument to
:samp:`--collate`. With
:samp:`--collate={n}`,
pull groups of :samp:`{n}` pages from each file,
again, stopping when there are no more pages. For example, if you ran
:command:`qpdf --collate=2 --empty --pages a.pdf 1-5 b.pdf 6-4 c.pdf
r1 -- out.pdf`, you would get the following pages in this
order:

- a.pdf page 1

- a.pdf page 2

- b.pdf page 6

- b.pdf page 5

- c.pdf last page

- a.pdf page 3

- a.pdf page 4

- b.pdf page 4

- a.pdf page 5

Starting in qpdf version 8.3, when you split and merge files, any page
labels (page numbers) are preserved in the final file. It is expected
that more document features will be preserved by splitting and merging.
In the mean time, semantics of splitting and merging vary across
features. For example, the document's outlines (bookmarks) point to
actual page objects, so if you select some pages and not others,
bookmarks that point to pages that are in the output file will work, and
remaining bookmarks will not work. A future version of
:command:`qpdf` may do a better job at handling these
issues. (Note that the qpdf library already contains all of the APIs
required in order to implement this in your own application if you need
it.) In the mean time, you can always use
:samp:`--empty` as the primary input file to avoid
copying all of that from the first file. For example, to take pages 1
through 5 from a :file:`infile.pdf` while preserving
all metadata associated with that file, you could use

::

   qpdf infile.pdf --pages . 1-5 -- outfile.pdf

If you wanted pages 1 through 5 from
:file:`infile.pdf` but you wanted the rest of the
metadata to be dropped, you could instead run

::

   qpdf --empty --pages infile.pdf 1-5 -- outfile.pdf

If you wanted to take pages 1 through 5 from
:file:`file1.pdf` and pages 11 through 15 from
:file:`file2.pdf` in reverse, taking document-level
metadata from :file:`file2.pdf`, you would run

::

   qpdf file2.pdf --pages file1.pdf 1-5 . 15-11 -- outfile.pdf

If, for some reason, you wanted to take the first page of an encrypted
file called :file:`encrypted.pdf` with password
``pass`` and repeat it twice in an output file, and if you wanted to
drop document-level metadata but preserve encryption, you would use

::

   qpdf --empty --copy-encryption=encrypted.pdf --encryption-file-password=pass
   --pages encrypted.pdf --password=pass 1 ./encrypted.pdf --password=pass 1 --
   outfile.pdf

Note that we had to specify the password all three times because giving
a password as :samp:`--encryption-file-password` doesn't
count for page selection, and as far as qpdf is concerned,
:file:`encrypted.pdf` and
:file:`./encrypted.pdf` are separated files. These
are all corner cases that most users should hopefully never have to be
bothered with.

Prior to version 8.4, it was not possible to specify the same page from
the same file directly more than once, and the workaround of specifying
the same file in more than one way was required. Version 8.4 removes
this limitation, but there is still a valid use case. When you specify
the same page from the same file more than once, qpdf will share objects
between the pages. If you are going to do further manipulation on the
file and need the two instances of the same original page to be deep
copies, then you can specify the file in two different ways. For example
:command:`qpdf in.pdf --pages . 1 ./in.pdf 1 -- out.pdf`
would create a file with two copies of the first page of the input, and
the two copies would share any objects in common. This includes fonts,
images, and anything else the page references.

.. _overlay-underlay:

Overlay and Underlay Options
----------------------------

Starting with qpdf 8.4, it is possible to overlay or underlay pages from
other files onto the output generated by qpdf. Specify overlay or
underlay as follows:

::

   { --overlay | --underlay } file [ options ] --

Overlay and underlay options are processed late, so they can be combined
with other like merging and will apply to the final output. The
:samp:`--overlay` and :samp:`--underlay`
options work the same way, except underlay pages are drawn underneath
the page to which they are applied, possibly obscured by the original
page, and overlay files are drawn on top of the page to which they are
applied, possibly obscuring the page. You can combine overlay and
underlay.

The default behavior of overlay and underlay is that pages are taken
from the overlay/underlay file in sequence and applied to corresponding
pages in the output until there are no more output pages. If the overlay
or underlay file runs out of pages, remaining output pages are left
alone. This behavior can be modified by options, which are provided
between the :samp:`--overlay` or
:samp:`--underlay` flag and the
:samp:`--` option. The following options are supported:

- :samp:`--password=password`: supply a password if the
  overlay/underlay file is encrypted.

- :samp:`--to=page-range`: a range of pages in the same
  form at described in :ref:`page-selection`
  indicates which pages in the output should have the overlay/underlay
  applied. If not specified, overlay/underlay are applied to all pages.

- :samp:`--from=[page-range]`: a range of pages that
  specifies which pages in the overlay/underlay file will be used for
  overlay or underlay. If not specified, all pages will be used. This
  can be explicitly specified to be empty if
  :samp:`--repeat` is used.

- :samp:`--repeat=page-range`: an optional range of
  pages that specifies which pages in the overlay/underlay file will be
  repeated after the "from" pages are used up. If you want to repeat a
  range of pages starting at the beginning, you can explicitly use
  :samp:`--from=`.

Here are some examples.

- :command:`--overlay o.pdf --to=1-5 --from=1-3 --repeat=4
  --`: overlay the first three pages from file
  :file:`o.pdf` onto the first three pages of the
  output, then overlay page 4 from :file:`o.pdf`
  onto pages 4 and 5 of the output. Leave remaining output pages
  untouched.

- :command:`--underlay footer.pdf --from= --repeat=1,2
  --`: Underlay page 1 of
  :file:`footer.pdf` on all odd output pages, and
  underlay page 2 of :file:`footer.pdf` on all even
  output pages.

.. _attachments:

Embedded Files/Attachments Options
----------------------------------

Starting with qpdf 10.2, you can work with file attachments in PDF files
from the command line. The following options are available:

:samp:`--list-attachments`
   Show the "key" and stream number for embedded files. With
   :samp:`--verbose`, additional information, including
   preferred file name, description, dates, and more are also displayed.
   The key is usually but not always equal to the file name, and is
   needed by some of the other options.

:samp:`--show-attachment={key}`
   Write the contents of the specified attachment to standard output as
   binary data. The key should match one of the keys shown by
   :samp:`--list-attachments`. If specified multiple
   times, only the last attachment will be shown.

:samp:`--add-attachment {file} {options} --`
   Add or replace an attachment with the contents of
   :samp:`{file}`. This may be specified more
   than once. The following additional options may appear before the
   ``--`` that ends this option:

   :samp:`--key={key}`
      The key to use to register the attachment in the embedded files
      table. Defaults to the last path element of
      :samp:`{file}`.

   :samp:`--filename={name}`
      The file name to be used for the attachment. This is what is
      usually displayed to the user and is the name most graphical PDF
      viewers will use when saving a file. It defaults to the last path
      element of :samp:`{file}`.

   :samp:`--creationdate={date}`
      The attachment's creation date in PDF format; defaults to the
      current time. The date format is explained below.

   :samp:`--moddate={date}`
      The attachment's modification date in PDF format; defaults to the
      current time. The date format is explained below.

   :samp:`--mimetype={type/subtype}`
      The mime type for the attachment, e.g. ``text/plain`` or
      ``application/pdf``. Note that the mimetype appears in a field
      called ``/Subtype`` in the PDF but actually includes the full type
      and subtype of the mime type.

   :samp:`--description={"text"}`
      Descriptive text for the attachment, displayed by some PDF
      viewers.

   :samp:`--replace`
      Indicates that any existing attachment with the same key should be
      replaced by the new attachment. Otherwise,
      :command:`qpdf` gives an error if an attachment
      with that key is already present.

:samp:`--remove-attachment={key}`
   Remove the specified attachment. This doesn't only remove the
   attachment from the embedded files table but also clears out the file
   specification. That means that any potential internal links to the
   attachment will be broken. This option may be specified multiple
   times. Run with :samp:`--verbose` to see status of
   the removal.

:samp:`--copy-attachments-from {file} {options} --`
   Copy attachments from another file. This may be specified more than
   once. The following additional options may appear before the ``--``
   that ends this option:

   :samp:`--password={password}`
      If required, the password needed to open
      :samp:`{file}`

   :samp:`--prefix={prefix}`
      Only required if the file from which attachments are being copied
      has attachments with keys that conflict with attachments already
      in the file. In this case, the specified prefix will be prepended
      to each key. This affects only the key in the embedded files
      table, not the file name. The PDF specification doesn't preclude
      multiple attachments having the same file name.

When a date is required, the date should conform to the PDF date format
specification, which is
``D:``\ :samp:`{yyyymmddhhmmss<z>}`, where
:samp:`{<z>}` is either ``Z`` for UTC or a
timezone offset in the form :samp:`{-hh'mm'}` or
:samp:`{+hh'mm'}`. Examples:
``D:20210207161528-05'00'``, ``D:20210207211528Z``.

.. _advanced-parsing:

Advanced Parsing Options
------------------------

These options control aspects of how qpdf reads PDF files. Mostly these
are of use to people who are working with damaged files. There is little
reason to use these options unless you are trying to solve specific
problems. The following options are available:

:samp:`--suppress-recovery`
   Prevents qpdf from attempting to recover damaged files.

:samp:`--ignore-xref-streams`
   Tells qpdf to ignore any cross-reference streams.

Ordinarily, qpdf will attempt to recover from certain types of errors in
PDF files. These include errors in the cross-reference table, certain
types of object numbering errors, and certain types of stream length
errors. Sometimes, qpdf may think it has recovered but may not have
actually recovered, so care should be taken when using this option as
some data loss is possible. The
:samp:`--suppress-recovery` option will prevent qpdf
from attempting recovery. In this case, it will fail on the first error
that it encounters.

Ordinarily, qpdf reads cross-reference streams when they are present in
a PDF file. If :samp:`--ignore-xref-streams` is
specified, qpdf will ignore any cross-reference streams for hybrid PDF
files. The purpose of hybrid files is to make some content available to
viewers that are not aware of cross-reference streams. It is almost
never desirable to ignore them. The only time when you might want to use
this feature is if you are testing creation of hybrid PDF files and wish
to see how a PDF consumer that doesn't understand object and
cross-reference streams would interpret such a file.

.. _advanced-transformation:

Advanced Transformation Options
-------------------------------

These transformation options control fine points of how qpdf creates the
output file. Mostly these are of use only to people who are very
familiar with the PDF file format or who are PDF developers. The
following options are available:

:samp:`--compress-streams={[yn]}`
   By default, or with :samp:`--compress-streams=y`,
   qpdf will compress any stream with no other filters applied to it
   with the ``/FlateDecode`` filter when it writes it. To suppress this
   behavior and preserve uncompressed streams as uncompressed, use
   :samp:`--compress-streams=n`.

:samp:`--decode-level={option}`
   Controls which streams qpdf tries to decode. The default is
   :samp:`generalized`. The following options are
   available:

   - :samp:`none`: do not attempt to decode any streams

   - :samp:`generalized`: decode streams filtered with
     supported generalized filters: ``/LZWDecode``, ``/FlateDecode``,
     ``/ASCII85Decode``, and ``/ASCIIHexDecode``. We define generalized
     filters as those to be used for general-purpose compression or
     encoding, as opposed to filters specifically designed for image
     data. Note that, by default, streams already compressed with
     ``/FlateDecode`` are not uncompressed and recompressed unless you
     also specify :samp:`--recompress-flate`.

   - :samp:`specialized`: in addition to generalized,
     decode streams with supported non-lossy specialized filters;
     currently this is just ``/RunLengthDecode``

   - :samp:`all`: in addition to generalized and
     specialized, decode streams with supported lossy filters;
     currently this is just ``/DCTDecode`` (JPEG)

:samp:`--stream-data={option}`
   Controls transformation of stream data. This option predates the
   :samp:`--compress-streams` and
   :samp:`--decode-level` options. Those options can be
   used to achieve the same affect with more control. The value of
   :samp:`{option}` may
   be one of the following:

   - :samp:`compress`: recompress stream data when
     possible (default); equivalent to
     :samp:`--compress-streams=y`
     :samp:`--decode-level=generalized`. Does not
     recompress streams already compressed with ``/FlateDecode`` unless
     :samp:`--recompress-flate` is also specified.

   - :samp:`preserve`: leave all stream data as is;
     equivalent to :samp:`--compress-streams=n`
     :samp:`--decode-level=none`

   - :samp:`uncompress`: uncompress stream data
     compressed with generalized filters when possible; equivalent to
     :samp:`--compress-streams=n`
     :samp:`--decode-level=generalized`

:samp:`--recompress-flate`
   By default, streams already compressed with ``/FlateDecode`` are left
   alone rather than being uncompressed and recompressed. This option
   causes qpdf to uncompress and recompress the streams. There is a
   significant performance cost to using this option, but you probably
   want to use it if you specify
   :samp:`--compression-level`.

:samp:`--compression-level={level}`
   When writing new streams that are compressed with ``/FlateDecode``,
   use the specified compression level. The value of
   :samp:`level` should be a number from 1 to 9 and is
   passed directly to zlib, which implements deflate compression. Note
   that qpdf doesn't uncompress and recompress streams by default. To
   have this option apply to already compressed streams, you should also
   specify :samp:`--recompress-flate`. If your goal is
   to shrink the size of PDF files, you should also use
   :samp:`--object-streams=generate`.

:samp:`--normalize-content=[yn]`
   Enables or disables normalization of content streams. Content
   normalization is enabled by default in QDF mode. Please see :ref:`qdf` for additional discussion of QDF mode.

:samp:`--object-streams={mode}`
   Controls handling of object streams. The value of
   :samp:`{mode}` may be
   one of the following:

   - :samp:`preserve`: preserve original object streams
     (default)

   - :samp:`disable`: don't write any object streams

   - :samp:`generate`: use object streams wherever
     possible

:samp:`--preserve-unreferenced`
   Tells qpdf to preserve objects that are not referenced when writing
   the file. Ordinarily any object that is not referenced in a traversal
   of the document from the trailer dictionary will be discarded. This
   may be useful in working with some damaged files or inspecting files
   with known unreferenced objects.

   This flag is ignored for linearized files and has the effect of
   causing objects in the new file to be written in order by object ID
   from the original file. This does not mean that object numbers will
   be the same since qpdf may create stream lengths as direct or
   indirect differently from the original file, and the original file
   may have gaps in its numbering.

   See also :samp:`--preserve-unreferenced-resources`,
   which does something completely different.

:samp:`--remove-unreferenced-resources={option}`
   The :samp:`{option}` may be ``auto``,
   ``yes``, or ``no``. The default is ``auto``.

   Starting with qpdf 8.1, when splitting pages, qpdf is able to attempt
   to remove images and fonts that are not used by a page even if they
   are referenced in the page's resources dictionary. When shared
   resources are in use, this behavior can greatly reduce the file sizes
   of split pages, but the analysis is very slow. In versions from 8.1
   through 9.1.1, qpdf did this analysis by default. Starting in qpdf
   10.0.0, if ``auto`` is used, qpdf does a quick analysis of the file
   to determine whether the file is likely to have unreferenced objects
   on pages, a pattern that frequently occurs when resource dictionaries
   are shared across multiple pages and rarely occurs otherwise. If it
   discovers this pattern, then it will attempt to remove unreferenced
   resources. Usually this means you get the slower splitting speed only
   when it's actually going to create smaller files. You can suppress
   removal of unreferenced resources altogether by specifying ``no`` or
   force it to do the full algorithm by specifying ``yes``.

   Other than cases in which you don't care about file size and care a
   lot about runtime, there are few reasons to use this option,
   especially now that ``auto`` mode is supported. One reason to use
   this is if you suspect that qpdf is removing resources it shouldn't
   be removing. If you encounter that case, please report it as bug at
   https://github.com/qpdf/qpdf/issues/.

:samp:`--preserve-unreferenced-resources`
   This is a synonym for
   :samp:`--remove-unreferenced-resources=no`.

   See also :samp:`--preserve-unreferenced`, which does
   something completely different.

:samp:`--newline-before-endstream`
   Tells qpdf to insert a newline before the ``endstream`` keyword, not
   counted in the length, after any stream content even if the last
   character of the stream was a newline. This may result in two
   newlines in some cases. This is a requirement of PDF/A. While qpdf
   doesn't specifically know how to generate PDF/A-compliant PDFs, this
   at least prevents it from removing compliance on already compliant
   files.

:samp:`--linearize-pass1={file}`
   Write the first pass of linearization to the named file. The
   resulting file is not a valid PDF file. This option is useful only
   for debugging ``QPDFWriter``'s linearization code. When qpdf
   linearizes files, it writes the file in two passes, using the first
   pass to calculate sizes and offsets that are required for hint tables
   and the linearization dictionary. Ordinarily, the first pass is
   discarded. This option enables it to be captured.

:samp:`--coalesce-contents`
   When a page's contents are split across multiple streams, this option
   causes qpdf to combine them into a single stream. Use of this option
   is never necessary for ordinary usage, but it can help when working
   with some files in some cases. For example, this can also be combined
   with QDF mode or content normalization to make it easier to look at
   all of a page's contents at once.

:samp:`--flatten-annotations={option}`
   This option collapses annotations into the pages' contents with
   special handling for form fields. Ordinarily, an annotation is
   rendered separately and on top of the page. Combining annotations
   into the page's contents effectively freezes the placement of the
   annotations, making them look right after various page
   transformations. The library functionality backing this option was
   added for the benefit of programs that want to create *n-up* page
   layouts and other similar things that don't work well with
   annotations. The :samp:`{option}` parameter
   may be any of the following:

   - :samp:`all`: include all annotations that are not
     marked invisible or hidden

   - :samp:`print`: only include annotations that
     indicate that they should appear when the page is printed

   - :samp:`screen`: omit annotations that indicate
     they should not appear on the screen

   Note that form fields are special because the annotations that are
   used to render filled-in form fields may become out of date from the
   fields' values if the form is filled in by a program that doesn't
   know how to update the appearances. If qpdf detects this case, its
   default behavior is not to flatten those annotations because doing so
   would cause the value of the form field to be lost. This gives you a
   chance to go back and resave the form with a program that knows how
   to generate appearances. QPDF itself can generate appearances with
   some limitations. See the
   :samp:`--generate-appearances` option below.

:samp:`--generate-appearances`
   If a file contains interactive form fields and indicates that the
   appearances are out of date with the values of the form, this flag
   will regenerate appearances, subject to a few limitations. Note that
   there is not usually a reason to do this, but it can be necessary
   before using the :samp:`--flatten-annotations`
   option. Most of these are not a problem with well-behaved PDF files.
   The limitations are as follows:

   - Radio button and checkbox appearances use the pre-set values in
     the PDF file. QPDF just makes sure that the correct appearance is
     displayed based on the value of the field. This is fine for PDF
     files that create their forms properly. Some PDF writers save
     appearances for fields when they change, which could cause some
     controls to have inconsistent appearances.

   - For text fields and list boxes, any characters that fall outside
     of US-ASCII or, if detected, "Windows ANSI" or "Mac Roman"
     encoding, will be replaced by the ``?`` character.

   - Quadding is ignored. Quadding is used to specify whether the
     contents of a field should be left, center, or right aligned with
     the field.

   - Rich text, multi-line, and other more elaborate formatting
     directives are ignored.

   - There is no support for multi-select fields or signature fields.

   If qpdf doesn't do a good enough job with your form, use an external
   application to save your filled-in form before processing it with
   qpdf.

:samp:`--optimize-images`
   This flag causes qpdf to recompress all images that are not
   compressed with DCT (JPEG) using DCT compression as long as doing so
   decreases the size in bytes of the image data and the image does not
   fall below minimum specified dimensions. Useful information is
   provided when used in combination with
   :samp:`--verbose`. See also the
   :samp:`--oi-min-width`,
   :samp:`--oi-min-height`, and
   :samp:`--oi-min-area` options. By default, starting
   in qpdf 8.4, inline images are converted to regular images and
   optimized as well. Use :samp:`--keep-inline-images`
   to prevent inline images from being included.

:samp:`--oi-min-width={width}`
   Avoid optimizing images whose width is below the specified amount. If
   omitted, the default is 128 pixels. Use 0 for no minimum.

:samp:`--oi-min-height={height}`
   Avoid optimizing images whose height is below the specified amount.
   If omitted, the default is 128 pixels. Use 0 for no minimum.

:samp:`--oi-min-area={area-in-pixels}`
   Avoid optimizing images whose pixel count (width × height) is below
   the specified amount. If omitted, the default is 16,384 pixels. Use 0
   for no minimum.

:samp:`--externalize-inline-images`
   Convert inline images to regular images. By default, images whose
   data is at least 1,024 bytes are converted when this option is
   selected. Use :samp:`--ii-min-bytes` to change the
   size threshold. This option is implicitly selected when
   :samp:`--optimize-images` is selected. Use
   :samp:`--keep-inline-images` to exclude inline images
   from image optimization.

:samp:`--ii-min-bytes={bytes}`
   Avoid converting inline images whose size is below the specified
   minimum size to regular images. If omitted, the default is 1,024
   bytes. Use 0 for no minimum.

:samp:`--keep-inline-images`
   Prevent inline images from being included in image optimization. This
   option has no affect when :samp:`--optimize-images`
   is not specified.

:samp:`--remove-page-labels`
   Remove page labels from the output file.

:samp:`--qdf`
   Turns on QDF mode. For additional information on QDF, please see :ref:`qdf`. Note that :samp:`--linearize`
   disables QDF mode.

:samp:`--min-version={version}`
   Forces the PDF version of the output file to be at least
   :samp:`{version}`. In other words, if the
   input file has a lower version than the specified version, the
   specified version will be used. If the input file has a higher
   version, the input file's original version will be used. It is seldom
   necessary to use this option since qpdf will automatically increase
   the version as needed when adding features that require newer PDF
   readers.

   The version number may be expressed in the form
   :samp:`{major.minor.extension-level}`, in
   which case the version is interpreted as
   :samp:`{major.minor}` at extension level
   :samp:`{extension-level}`. For example,
   version ``1.7.8`` represents version 1.7 at extension level 8. Note
   that minimal syntax checking is done on the command line.

:samp:`--force-version={version}`
   This option forces the PDF version to be the exact version specified
   *even when the file may have content that is not supported in that
   version*. The version number is interpreted in the same way as with
   :samp:`--min-version` so that extension levels can be
   set. In some cases, forcing the output file's PDF version to be lower
   than that of the input file will cause qpdf to disable certain
   features of the document. Specifically, 256-bit keys are disabled if
   the version is less than 1.7 with extension level 8 (except R5 is
   disabled if less than 1.7 with extension level 3), AES encryption is
   disabled if the version is less than 1.6, cleartext metadata and
   object streams are disabled if less than 1.5, 128-bit encryption keys
   are disabled if less than 1.4, and all encryption is disabled if less
   than 1.3. Even with these precautions, qpdf won't be able to do
   things like eliminate use of newer image compression schemes,
   transparency groups, or other features that may have been added in
   more recent versions of PDF.

   As a general rule, with the exception of big structural things like
   the use of object streams or AES encryption, PDF viewers are supposed
   to ignore features in files that they don't support from newer
   versions. This means that forcing the version to a lower version may
   make it possible to open your PDF file with an older version, though
   bear in mind that some of the original document's functionality may
   be lost.

By default, when a stream is encoded using non-lossy filters that qpdf
understands and is not already compressed using a good compression
scheme, qpdf will uncompress and recompress streams. Assuming proper
filter implements, this is safe and generally results in smaller files.
This behavior may also be explicitly requested with
:samp:`--stream-data=compress`.

When :samp:`--normalize-content=y` is specified, qpdf
will attempt to normalize whitespace and newlines in page content
streams. This is generally safe but could, in some cases, cause damage
to the content streams. This option is intended for people who wish to
study PDF content streams or to debug PDF content. You should not use
this for "production" PDF files.

When normalizing content, if qpdf runs into any lexical errors, it will
print a warning indicating that content may be damaged. The only
situation in which qpdf is known to cause damage during content
normalization is when a page's contents are split across multiple
streams and streams are split in the middle of a lexical token such as a
string, name, or inline image. Note that files that do this are invalid
since the PDF specification states that content streams are not to be
split in the middle of a token. If you want to inspect the original
content streams in an uncompressed format, you can always run with
:samp:`--qdf --normalize-content=n` for a QDF file
without content normalization, or alternatively
:samp:`--stream-data=uncompress` for a regular non-QDF
mode file with uncompressed streams. These will both uncompress all the
streams but will not attempt to normalize content. Please note that if
you are using content normalization or QDF mode for the purpose of
manually inspecting files, you don't have to care about this.

Object streams, also known as compressed objects, were introduced into
the PDF specification at version 1.5, corresponding to Acrobat 6. Some
older PDF viewers may not support files with object streams. qpdf can be
used to transform files with object streams to files without object
streams or vice versa. As mentioned above, there are three object stream
modes: :samp:`preserve`,
:samp:`disable`, and :samp:`generate`.

In :samp:`preserve` mode, the relationship to objects
and the streams that contain them is preserved from the original file.
In :samp:`disable` mode, all objects are written as
regular, uncompressed objects. The resulting file should be readable by
older PDF viewers. (Of course, the content of the files may include
features not supported by older viewers, but at least the structure will
be supported.) In :samp:`generate` mode, qpdf will
create its own object streams. This will usually result in more compact
PDF files, though they may not be readable by older viewers. In this
mode, qpdf will also make sure the PDF version number in the header is
at least 1.5.

The :samp:`--qdf` flag turns on QDF mode, which changes
some of the defaults described above. Specifically, in QDF mode, by
default, stream data is uncompressed, content streams are normalized,
and encryption is removed. These defaults can still be overridden by
specifying the appropriate options as described above. Additionally, in
QDF mode, stream lengths are stored as indirect objects, objects are
laid out in a less efficient but more readable fashion, and the
documents are interspersed with comments that make it easier for the
user to find things and also make it possible for
:command:`fix-qdf` to work properly. QDF mode is intended
for people, mostly developers, who wish to inspect or modify PDF files
in a text editor. For details, please see :ref:`qdf`.

.. _testing-options:

Testing, Inspection, and Debugging Options
------------------------------------------

These options can be useful for digging into PDF files or for use in
automated test suites for software that uses the qpdf library. When any
of the options in this section are specified, no output file should be
given. The following options are available:

:samp:`--deterministic-id`
   Causes generation of a deterministic value for /ID. This prevents use
   of timestamp and output file name information in the /ID generation.
   Instead, at some slight additional runtime cost, the /ID field is
   generated to include a digest of the significant parts of the content
   of the output PDF file. This means that a given qpdf operation should
   generate the same /ID each time it is run, which can be useful when
   caching results or for generation of some test data. Use of this flag
   is not compatible with creation of encrypted files.

:samp:`--static-id`
   Causes generation of a fixed value for /ID. This is intended for
   testing only. Never use it for production files. If you are trying to
   get the same /ID each time for a given file and you are not
   generating encrypted files, consider using the
   :samp:`--deterministic-id` option.

:samp:`--static-aes-iv`
   Causes use of a static initialization vector for AES-CBC. This is
   intended for testing only so that output files can be reproducible.
   Never use it for production files. This option in particular is not
   secure since it significantly weakens the encryption.

:samp:`--no-original-object-ids`
   Suppresses inclusion of original object ID comments in QDF files.
   This can be useful when generating QDF files for test purposes,
   particularly when comparing them to determine whether two PDF files
   have identical content.

:samp:`--show-encryption`
   Shows document encryption parameters. Also shows the document's user
   password if the owner password is given.

:samp:`--show-encryption-key`
   When encryption information is being displayed, as when
   :samp:`--check` or
   :samp:`--show-encryption` is given, display the
   computed or retrieved encryption key as a hexadecimal string. This
   value is not ordinarily useful to users, but it can be used as the
   argument to :samp:`--password` if the
   :samp:`--password-is-hex-key` is specified. Note
   that, when PDF files are encrypted, passwords and other metadata are
   used only to compute an encryption key, and the encryption key is
   what is actually used for encryption. This enables retrieval of that
   key.

:samp:`--check-linearization`
   Checks file integrity and linearization status.

:samp:`--show-linearization`
   Checks and displays all data in the linearization hint tables.

:samp:`--show-xref`
   Shows the contents of the cross-reference table in a human-readable
   form. This is especially useful for files with cross-reference
   streams which are stored in a binary format.

:samp:`--show-object=trailer|obj[,gen]`
   Show the contents of the given object. This is especially useful for
   inspecting objects that are inside of object streams (also known as
   "compressed objects").

:samp:`--raw-stream-data`
   When used along with the :samp:`--show-object`
   option, if the object is a stream, shows the raw stream data instead
   of object's contents.

:samp:`--filtered-stream-data`
   When used along with the :samp:`--show-object`
   option, if the object is a stream, shows the filtered stream data
   instead of object's contents. If the stream is filtered using filters
   that qpdf does not support, an error will be issued.

:samp:`--show-npages`
   Prints the number of pages in the input file on a line by itself.
   Since the number of pages appears by itself on a line, this option
   can be useful for scripting if you need to know the number of pages
   in a file.

:samp:`--show-pages`
   Shows the object and generation number for each page dictionary
   object and for each content stream associated with the page. Having
   this information makes it more convenient to inspect objects from a
   particular page.

:samp:`--with-images`
   When used along with :samp:`--show-pages`, also shows
   the object and generation numbers for the image objects on each page.
   (At present, information about images in shared resource dictionaries
   are not output by this command. This is discussed in a comment in the
   source code.)

:samp:`--json`
   Generate a JSON representation of the file. This is described in
   depth in :ref:`json`

:samp:`--json-help`
   Describe the format of the JSON output.

:samp:`--json-key=key`
   This option is repeatable. If specified, only top-level keys
   specified will be included in the JSON output. If not specified, all
   keys will be shown.

:samp:`--json-object=trailer|obj[,gen]`
   This option is repeatable. If specified, only specified objects will
   be shown in the "``objects``" key of the JSON output. If absent, all
   objects will be shown.

:samp:`--check`
   Checks file structure and well as encryption, linearization, and
   encoding of stream data. A file for which
   :samp:`--check` reports no errors may still have
   errors in stream data content but should otherwise be structurally
   sound. If :samp:`--check` any errors, qpdf will exit
   with a status of 2. There are some recoverable conditions that
   :samp:`--check` detects. These are issued as warnings
   instead of errors. If qpdf finds no errors but finds warnings, it
   will exit with a status of 3 (as of version 2.0.4). When
   :samp:`--check` is combined with other options,
   checks are always performed before any other options are processed.
   For erroneous files, :samp:`--check` will cause qpdf
   to attempt to recover, after which other options are effectively
   operating on the recovered file. Combining
   :samp:`--check` with other options in this way can be
   useful for manually recovering severely damaged files. Note that
   :samp:`--check` produces no output to standard output
   when everything is valid, so if you are using this to
   programmatically validate files in bulk, it is safe to run without
   output redirected to :file:`/dev/null` and just
   check for a 0 exit code.

The :samp:`--raw-stream-data` and
:samp:`--filtered-stream-data` options are ignored
unless :samp:`--show-object` is given. Either of these
options will cause the stream data to be written to standard output. In
order to avoid commingling of stream data with other output, it is
recommend that these objects not be combined with other test/inspection
options.

If :samp:`--filtered-stream-data` is given and
:samp:`--normalize-content=y` is also given, qpdf will
attempt to normalize the stream data as if it is a page content stream.
This attempt will be made even if it is not a page content stream, in
which case it will produce unusable results.

.. _unicode-passwords:

Unicode Passwords
-----------------

At the library API level, all methods that perform encryption and
decryption interpret passwords as strings of bytes. It is up to the
caller to ensure that they are appropriately encoded. Starting with qpdf
version 8.4.0, qpdf will attempt to make this easier for you when
interact with qpdf via its command line interface. The PDF specification
requires passwords used to encrypt files with 40-bit or 128-bit
encryption to be encoded with PDF Doc encoding. This encoding is a
single-byte encoding that supports ISO-Latin-1 and a handful of other
commonly used characters. It has a large overlap with Windows ANSI but
is not exactly the same. There is generally not a way to provide PDF Doc
encoded strings on the command line. As such, qpdf versions prior to
8.4.0 would often create PDF files that couldn't be opened with other
software when given a password with non-ASCII characters to encrypt a
file with 40-bit or 128-bit encryption. Starting with qpdf 8.4.0, qpdf
recognizes the encoding of the parameter and transcodes it as needed.
The rest of this section provides the details about exactly how qpdf
behaves. Most users will not need to know this information, but it might
be useful if you have been working around qpdf's old behavior or if you
are using qpdf to generate encrypted files for testing other PDF
software.

A note about Windows: when qpdf builds, it attempts to determine what it
has to do to use ``wmain`` instead of ``main`` on Windows. The ``wmain``
function is an alternative entry point that receives all arguments as
UTF-16-encoded strings. When qpdf starts up this way, it converts all
the strings to UTF-8 encoding and then invokes the regular main. This
means that, as far as qpdf is concerned, it receives its command-line
arguments with UTF-8 encoding, just as it would in any modern Linux or
UNIX environment.

If a file is being encrypted with 40-bit or 128-bit encryption and the
supplied password is not a valid UTF-8 string, qpdf will fall back to
the behavior of interpreting the password as a string of bytes. If you
have old scripts that encrypt files by passing the output of
:command:`iconv` to qpdf, you no longer need to do that,
but if you do, qpdf should still work. The only exception would be for
the extremely unlikely case of a password that is encoded with a
single-byte encoding but also happens to be valid UTF-8. Such a password
would contain strings of even numbers of characters that alternate
between accented letters and symbols. In the extremely unlikely event
that you are intentionally using such passwords and qpdf is thwarting
you by interpreting them as UTF-8, you can use
:samp:`--password-mode=bytes` to suppress qpdf's
automatic behavior.

The :samp:`--password-mode` option, as described earlier
in this chapter, can be used to change qpdf's interpretation of supplied
passwords. There are very few reasons to use this option. One would be
the unlikely case described in the previous paragraph in which the
supplied password happens to be valid UTF-8 but isn't supposed to be
UTF-8. Your best bet would be just to provide the password as a valid
UTF-8 string, but you could also use
:samp:`--password-mode=bytes`. Another reason to use
:samp:`--password-mode=bytes` would be to intentionally
generate PDF files encrypted with passwords that are not properly
encoded. The qpdf test suite does this to generate invalid files for the
purpose of testing its password recovery capability. If you were trying
to create intentionally incorrect files for a similar purposes, the
:samp:`bytes` password mode can enable you to do this.

When qpdf attempts to decrypt a file with a password that contains
non-ASCII characters, it will generate a list of alternative passwords
by attempting to interpret the password as each of a handful of
different coding systems and then transcode them to the required format.
This helps to compensate for the supplied password being given in the
wrong coding system, such as would happen if you used the
:command:`iconv` workaround that was previously needed.
It also generates passwords by doing the reverse operation: translating
from correct in incorrect encoding of the password. This would enable
qpdf to decrypt files using passwords that were improperly encoded by
whatever software encrypted the files, including older versions of qpdf
invoked without properly encoded passwords. The combination of these two
recovery methods should make qpdf transparently open most encrypted
files with the password supplied correctly but in the wrong coding
system. There are no real downsides to this behavior, but if you don't
want qpdf to do this, you can use the
:samp:`--suppress-password-recovery` option. One reason
to do that is to ensure that you know the exact password that was used
to encrypt the file.

With these changes, qpdf now generates compliant passwords in most
cases. There are still some exceptions. In particular, the PDF
specification directs compliant writers to normalize Unicode passwords
and to perform certain transformations on passwords with bidirectional
text. Implementing this functionality requires using a real Unicode
library like ICU. If a client application that uses qpdf wants to do
this, the qpdf library will accept the resulting passwords, but qpdf
will not perform these transformations itself. It is possible that this
will be addressed in a future version of qpdf. The ``QPDFWriter``
methods that enable encryption on the output file accept passwords as
strings of bytes.

Please note that the :samp:`--password-is-hex-key`
option is unrelated to all this. This flag bypasses the normal process
of going from password to encryption string entirely, allowing the raw
encryption key to be specified directly. This is useful for forensic
purposes or for brute-force recovery of files with unknown passwords.
