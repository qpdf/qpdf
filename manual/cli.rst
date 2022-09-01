.. NOTES

   See "HOW TO ADD A COMMAND-LINE ARGUMENT" in README-maintainer.

   This file contains text that is used for help file generation.
   Lines that start with the magic comment ".. help topic x: y"
   introduce a help topic called "x" with short text "y". The contents
   of the comment are the long text.

   The ".. qpdf:option:: option" directive introduces a command-line
   option. The next ".. help: short_text" comment's contents are the
   long text of the help. Search for ".. help-topic" and "qpdf:option"
   for additional help. Command line arguments can be referenced using
   :qpdf:ref:`--option`. They also appear in an index.

   Note: 2022-01-22: because short help text is used in the "schema"
   JSON object for QPDFJob JSON, we can't end short text with a ``)``
   character since doing so would cause ``)"`` to appear in the string
   literal. We use the R"(...)" syntax for these literals, and that
   looks like an end delimiter. While the C++ spec allows
   R"anything(...)anything" specifically for this purpose, the MSVC in
   CI at the time of this writing did not support that construct.

   STYLE NOTES

   In this text, :samp:`...` and ``...`` are used somewhat
   interchangeably. :samp: should be used when there is replaceable
   text enclosed in curly braces. Otherwise, either is fine. Ideally
   there should be a stricter editorial convention, but they render
   the same, so I have not gone to the trouble of making it
   consistent.

   USE :qpdf:ref:`--option` to refer to an option whenever it is
   specified without parameters in any place other than its own help.
   This creates a link.

   When referring to command-line options, use the following
   terminology:

   argument: any command-line argument whether option or positional
   option: any argument starting with -- including its parameter, if any
   flag: the --flag part of the option; only use to disambiguate
   parameter: the parameter part of the option

   Example: qpdf in.pdf --object-stream=generalized out.pdf

   Each word is an argument.
   The "--object-stream=generalized" option consists of
   the "--object-stream" flag with the "generalized" parameter. It
   would be okay to talk about "the --object-stream option" in the
   text if there's no ambiguity.

.. _using:

Running qpdf
============

This chapter describes how to run the qpdf program from the command
line.

.. _invocation:

Basic Invocation
----------------

.. help-topic usage: basic invocation

   Read a PDF file, apply transformations or modifications, and write
   a new PDF file.

   Usage: qpdf [infile] [options] [outfile]
      OR  qpdf --help[={topic|--option}]

   - infile, options, and outfile may be in any order as long as infile
     precedes outfile.
   - Use --empty in place of an input file for a zero-page, empty input
   - Use --replace-input in place of an output file to overwrite the
     input file with the output
   - outfile may be - to write to stdout; reading from stdin is not supported
   - @filename is an argument file; each line is treated as a separate
     command-line argument
   - @- may be used to read arguments from stdin
   - Later options may override earlier options if contradictory

::

   Usage: qpdf [infile] [options] [outfile]

The :command:`qpdf` command reads the PDF file :samp:`{infile}`,
applies various transformations or modifications to the file in
memory, and writes the result to :samp:`{outfile}`. When run with no
options, the output file is functionally identical to the input file
but may be structurally reorganized, and orphaned objects are removed
from the file. Many options are available for applying transformations
or modifications to the file.

:samp:`{infile}` can be a regular file, or it can be
:qpdf:ref:`--empty` to start with an empty PDF file. There is no way
to use standard input since the input file has to be seekable.

:samp:`{outfile}` can be a regular file, ``-`` to represent standard
output, or :qpdf:ref:`--replace-input` to indicate that the input file
should be overwritten. The output file does not have to be seekable,
even when generating linearized files. You can also use
:qpdf:ref:`--split-pages` to create separate output files for each
page (or group of pages) instead of a single output file.

Password-protected files may be opened by specifying a password with
:qpdf:ref:`--password`.

All options other than help options (see :ref:`help-options`) require
an input file. If inspection or JSON options (see
:ref:`inspection-options` and :ref:`json-options`) or help options are
given, an output file must not be given. Otherwise, an output file is
required.

If :samp:`@filename` appears as a word anywhere in the command-line,
it will be read line by line, and each line will be treated as a
command-line argument. Leading and trailing whitespace is
intentionally not removed from lines, which makes it possible to
handle arguments that start or end with spaces. The :samp:`@-` option
allows arguments to be read from standard input. This allows qpdf to
be invoked with an arbitrary number of arbitrarily long arguments. It
is also very useful for avoiding having to pass passwords on the
command line, though see also :qpdf:ref:`--password-file`. Note that
the :samp:`@filename` can't appear in the middle of an argument, so
constructs such as :samp:`--arg=@filename` will not work. Instead, you
would have to include the option and its parameter (e.g.,
:samp:`--option=parameter`) as a line in the :file:`filename` file and
just pass :samp:`@filename` on the command line.

Related Options
~~~~~~~~~~~~~~~

.. qpdf:option:: --empty

   .. help: use empty file as input

      Use in place of infile for an empty input. Especially useful
      with --pages.

   This option may be given in place of :samp:`{infile}`. This causes
   qpdf to use a dummy input file that contains zero pages. This
   option is useful in conjunction with :qpdf:ref:`--pages`. See
   :ref:`page-selection` for details.

.. qpdf:option:: --replace-input

   .. help: overwrite input with output

      Use in place of outfile to overwrite the input file with the output.

   This option may be given in place of :samp:`{outfile}`. This causes
   qpdf to replace the input file with the output. It does this by
   writing to :file:`{infilename}.~qpdf-temp#` and, when done,
   overwriting the input file with the temporary file. If there were
   any warnings, the original input is saved as
   :file:`{infilename}.~qpdf-orig`. If there are errors, the input
   file is left untouched.

.. qpdf:option:: --job-json-file=file

   .. help: job JSON file

      Specify the name of a file whose contents are expected to
      contain a QPDFJob JSON file. Run qpdf --job-json-help for a
      description of the JSON input file format.

   Specify the name of a file whose contents are expected to contain a
   QPDFJob JSON file. This file is read and treated as if the
   equivalent command-line arguments were supplied. It can be repeated
   and mixed freely with other options. Run ``qpdf`` with
   :qpdf:ref:`--job-json-help` for a description of the job JSON input
   file format. For more information, see :ref:`qpdf-job`. Note that
   this is unrelated to :qpdf:ref:`--json` but may be combined with
   it.

.. _exit-status:

Exit Status
-----------

.. help-topic exit-status: meanings of qpdf's exit codes

   Meaning of exit codes:

   0: no errors or warnings
   1: not used by qpdf but may be used by the shell if unable to invoke qpdf
   2: errors detected
   3: warnings detected, unless --warning-exit-0 is given

The exit status of :command:`qpdf` may be interpreted as follows:

.. list-table:: Exit Codes
   :widths: 5 80
   :header-rows: 0

   - - 0
     - no errors or warnings were found

   - - 1
     - not used

   - - 2
     - errors were found; the file was not processed

   - - 3
     - warnings were found without errors

Notes:

- A PDF file may have problems that qpdf can't detect.

- With the :qpdf:ref:`--warning-exit-0` option, exit status ``0`` is
  used even if there are warnings.

- :command:`qpdf` does not exit with status ``1`` since the shell uses
  this exit code if it is unable to invoke the command.

- If both errors and warnings were found, exit status ``2`` is used.

- The :qpdf:ref:`--is-encrypted` and :qpdf:ref:`--requires-password`
  options use different exit codes. See their help for details.

Related Options
~~~~~~~~~~~~~~~

.. qpdf:option:: --warning-exit-0

   .. help: exit 0 even with warnings

      Use exit status 0 instead of 3 when warnings are present. When
      combined with --no-warn, warnings are completely ignored.

   If there were warnings only and no errors, exit with exit code
   ``0`` instead of ``3``. When combined with :qpdf:ref:`--no-warn`,
   the effect is for :command:`qpdf` to completely ignore warnings.

.. _shell-completion:

Shell Completion
----------------

.. help-topic completion: shell completion

   Shell completion is supported with bash and zsh. Use
   eval $(qpdf --completion-bash) or eval $(qpdf --completion-zsh)
   to enable. The QPDF_EXECUTABLE environment variable overrides the
   path to qpdf that these commands output.

:command:`qpdf` provides its own completion support for zsh and bash.
You can enable bash completion with :command:`eval $(qpdf
--completion-bash)` and zsh completion with :command:`eval $(qpdf
--completion-zsh)`. If :command:`qpdf` is not in your path, you should
use an absolute path to qpdf in the above invocation. If you invoke it
with a relative path, it will warn you, and the completion won't work
if you're in a different directory.

:command:`qpdf` will use ``argv[0]`` to figure out where its
executable is. This may produce unwanted results in some cases,
especially if you are trying to use completion with a copy of qpdf that
is run directly out of the source tree or that is invoked with a
wrapper script. You can specify a full path to the qpdf you want to
use for completion in the ``QPDF_EXECUTABLE`` environment variable.

Related Options
~~~~~~~~~~~~~~~

.. qpdf:option:: --completion-bash

   .. help: enable bash completion

      Output a command that enables bash completion

   Output a completion command you can eval to enable shell completion
   from bash.

.. qpdf:option:: --completion-zsh

   .. help: enable zsh completion

      Output a command that enables zsh completion

   Output a completion command you can eval to enable shell completion
   from zsh.

.. _help-options:

Help/Information
----------------

.. help-topic help: information about qpdf

   Help options provide some information about qpdf itself. Help
   options are only valid as the first and only command-line argument.

Help options provide some information about qpdf itself. Help options
are only valid as the first and only command-line argument.

Related Options
~~~~~~~~~~~~~~~

.. qpdf:option:: --help[=--option|topic]

   .. help: provide help

      --help: provide general information and a list of topics
      --help=--option: provide help on a specific option
      --help=topic: provide help on a topic

   Display command-line invocation help. Use :samp:`--help={--option}`
   for help on a specific option and :samp:`--help={topic}` for help
   on a help topic and also provides a list of available help topics.

.. qpdf:option:: --version

   .. help: show qpdf version

      Display the version of qpdf.

   Display the version of qpdf. The version number displayed is the
   one that is compiled into the qpdf library. If you don't see the
   version number you expect, you may have more than one version of
   :command:`qpdf` installed and may not have your library path set up
   correctly.

.. qpdf:option:: --copyright

   .. help: show copyright information

      Display copyright and license information.

   Display copyright and license information.

.. qpdf:option:: --show-crypto

   .. help: show available crypto providers

      Show a list of available crypto providers, one per line. The
      default provider is shown first.

   Show a list of available crypto providers, each on a line by
   itself. The default provider is always listed first. See
   :ref:`crypto` for more information about crypto providers.

.. qpdf:option:: --job-json-help

   .. help: show format of job JSON

      Describe the format of the QPDFJob JSON input used by
      --job-json-file.

   Describe the format of the QPDFJob JSON input used by
   :qpdf:ref:`--job-json-file`. For more information about QPDFJob,
   see :ref:`qpdf-job`.

.. _general-options:

General Options
---------------

.. help-topic general: general options

   General options control qpdf's behavior in ways that are not
   directly related to the operation it is performing.

This section describes general options that control :command:`qpdf`'s
behavior. They are not necessarily related to the specific operation
that is being performed and may be used whether or not an output file
is being created.

Related Options
~~~~~~~~~~~~~~~

.. qpdf:option:: --password=password

   .. help: password for encrypted file

      Specify a password for an encrypted, password-protected file.
      Not needed for encrypted files without a password.

   Specifies a password for accessing encrypted, password-protected
   files. To read the password from a file or standard input, you can
   use :qpdf:ref:`--password-file`.

   Prior to 8.4.0, in the case of passwords that contain characters
   that fall outside of 7-bit US-ASCII, qpdf left the burden of
   supplying properly encoded encryption and decryption passwords to
   the user. Starting in qpdf 8.4.0, qpdf does this automatically in
   most cases. For an in-depth discussion, please see
   :ref:`unicode-passwords`. Previous versions of this manual
   described workarounds using the :command:`iconv` command. Such
   workarounds are no longer required or recommended starting with
   qpdf 8.4.0. However, for backward compatibility, qpdf attempts to
   detect those workarounds and do the right thing in most cases.

.. qpdf:option:: --password-file=filename

   .. help: read password from a file

      The first line of the specified file is used as the password.
      This is used in place of the --password option.

   Reads the first line from the specified file and uses it as the
   password for accessing encrypted files. :samp:`{filename}` may be
   ``-`` to read the password from standard input, but if you do that
   the password is echoed and there is no prompt, so use ``-`` with
   caution. Note that leading and trailing spaces are not stripped
   from the password.

.. qpdf:option:: --verbose

   .. help: print additional information

      Output additional information about various things qpdf is
      doing, including information about files created and operations
      performed.

   Increase verbosity of output. This includes information about files
   created, image optimization, and several other operations. In some
   cases, it also displays additional information when inspection
   options (see :ref:`inspection-options`) are used.

.. qpdf:option:: --progress

   .. help: show progress when writing

      Indicate progress when writing files.

   Indicate progress while writing output files. Progress indication
   does not start until writing starts, so there may be a delay before
   progress indicators are seen if complicated transformations are
   being applied before the write process begins.

.. qpdf:option:: --no-warn

   .. help: suppress printing of warning messages

      Suppress printing of warning messages. If warnings were
      encountered, qpdf still exits with exit status 3.
      Use --warning-exit-0 with --no-warn to completely ignore
      warnings.

   Suppress writing of warnings to stderr. If warnings were detected
   and suppressed, :command:`qpdf` will still exit with exit code 3.
   To completely ignore warnings, also specify
   :qpdf:ref:`--warning-exit-0`. Use with caution as qpdf is not
   always successful in recovering from situations that cause warnings
   to be issued.

.. qpdf:option:: --deterministic-id

   .. help: generate ID deterministically

      Generate a secure, random document ID only using static
      information, such as the page contents. Does not use the file's
      name or attributes or the current time.

   Generate a secure, random document ID using deterministic values.
   This prevents use of timestamp and output file name information in
   the ID generation. Instead, at some slight additional runtime cost,
   the ID field is generated to include a digest of the significant
   parts of the content of the output PDF file. This means that a
   given qpdf operation should generate the same ID each time it is
   run, which can be useful when caching results or for generation of
   some test data. Use of this flag is not compatible with creation of
   encrypted files. This option can be useful for testing. See also
   :qpdf:ref:`--static-id`.

   While qpdf will generate the same deterministic ID given the same
   output PDF, there is no guarantee that different versions of qpdf
   will generate exactly the same PDF output for the same input and
   options. While care is taken to avoid gratuitous changes to qpdf's
   PDF generation, new versions of qpdf may include changes or bug
   fixes that cause slightly different PDF code to be generated. Such
   changes are noted in the release notes.

.. qpdf:option:: --allow-weak-crypto

   .. help: allow insecure cryptographic algorithms

      Allow creation of files with weak cryptographic algorithms. This
      option is necessary to create 40-bit files or 128-bit files that
      use RC4 encryption.

   Encrypted PDF files using 40-bit keys or 128-bit keys without AES
   use the insecure *RC4* encryption algorithm. Starting with version
   11.0, qpdf's default behavior is to refuse to write files using RC4
   encryption. Use this option to allow creation of such files. In
   versions 10.4 through 10.6, attempting to create weak encrypted
   files was a warning, rather than an error, without this flag. See
   :ref:`weak-crypto` for additional details.

   No check is performed for weak crypto when preserving encryption
   parameters from or copying encryption parameters from other files.
   The rationale for this is discussed in :ref:`weak-crypto`.

.. qpdf:option:: --keep-files-open=[y|n]

   .. help: manage keeping multiple files open

      When qpdf needs to work with many files, as when merging large
      numbers of files, explicitly indicate whether files should be
      kept open. The default behavior is to determine this based on
      the number of files.

   This option controls whether qpdf keeps individual files open while
   merging. By default, qpdf keeps files open when merging unless more
   than 200 files are specified, in which case files are opened as
   needed and closed when finished. Repeatedly opening
   and closing files may impose a large performance penalty with some
   file systems, especially networked file systems. If you know that
   you have a large enough open file limit and are suffering from
   performance problems, or if you have an open file limit smaller
   than 200, you can use this option to override the default behavior
   by specifying :samp:`--keep-files-open=y` to force :command:`qpdf`
   to keep files open or :samp:`--keep-files-open=n` to force it to
   only open files as needed. See also
   :qpdf:ref:`--keep-files-open-threshold`.

   Historical note: prior to version 8.1.0, qpdf always kept all files
   open, but this meant that the number of files that could be merged
   was limited by the operating system's open file limit. Version
   8.1.0 opened files as they were referenced and closed them after
   each read, but this caused a major performance impact. Version
   8.2.0 optimized the performance but did so in a way that, for local
   file systems, there was a small but unavoidable performance hit,
   but for networked file systems the performance impact could be
   very high. The current behavior was introduced in qpdf version
   8.2.1.

.. qpdf:option:: --keep-files-open-threshold=count

   .. help: set threshold for --keep-files-open

      Set the threshold used by --keep-files-open, overriding the
      default value of 200.

   If specified, overrides the default value of 200 used as the
   threshold for qpdf deciding whether or not to keep files open. See
   :qpdf:ref:`--keep-files-open` for details.

.. _advanced-control-options:

Advanced Control Options
------------------------

.. help-topic advanced-control: tweak qpdf's behavior

   Advanced control options control qpdf's behavior in ways that would
   normally never be needed by a user but that may be useful to
   developers or people investigating problems with specific files.

Advanced control options control qpdf's behavior in ways that would
normally never be needed by a user but that may be useful to
developers or people investigating problems with specific files.

Related Options
~~~~~~~~~~~~~~~

.. qpdf:option:: --password-is-hex-key

   .. help: provide hex-encoded encryption key

      Provide the underlying file encryption key as a hex-encoded
      string rather than supplying a password. This is an expert
      option.

   Overrides the usual computation/retrieval of the PDF file's
   encryption key from user/owner password with an explicit
   specification of the encryption key. When this option is specified,
   the parameter to the :qpdf:ref:`--password` option is interpreted as
   a hexadecimal-encoded key value. This only applies to the password
   used to open the main input file. It does not apply to other files
   opened by :qpdf:ref:`--pages` or other options or to files being
   written.

   Most users will never have a need for this option, and no standard
   viewers support this mode of operation, but it can be useful for
   forensic or investigatory purposes. For example, if a PDF file is
   encrypted with an unknown password, a brute-force attack using the
   key directly is sometimes more efficient than one using the
   password. Also, if a file is heavily damaged, it may be possible to
   derive the encryption key and recover parts of the file using it
   directly. To expose the encryption key used by an encrypted file
   that you can open normally, use the
   :qpdf:ref:`--show-encryption-key` option.

.. qpdf:option:: --suppress-password-recovery

   .. help: don't try different password encodings

      Suppress qpdf's usual behavior of attempting different encodings
      of a password that contains non-ASCII Unicode characters if the
      first attempt doesn't succeed.

   Ordinarily, qpdf attempts to automatically compensate for passwords
   encoded with the wrong character encoding. This option suppresses
   that behavior. Under normal conditions, there are no reasons to use
   this option. See :ref:`unicode-passwords` for a discussion.

.. qpdf:option:: --password-mode=mode

   .. help: tweak how qpdf encodes passwords

      Fine-tune how qpdf controls encoding of Unicode passwords. Valid
      options are auto, bytes, hex-bytes, and unicode.

   This option can be used to fine-tune how qpdf interprets Unicode
   (non-ASCII) password strings passed on the command line. With the
   exception of the :samp:`hex-bytes` mode, these only apply to
   passwords provided when encrypting files. The :samp:`hex-bytes`
   mode also applies to passwords specified for reading files. For
   additional discussion of the supported password modes and when you
   might want to use them, see :ref:`unicode-passwords`. The following
   modes are supported:

   - :samp:`auto`: Automatically determine whether the specified
     password is a properly encoded Unicode (UTF-8) string, and
     transcode it as required by the PDF spec based on the type of
     encryption being applied. On Windows starting with version 8.4.0,
     and on almost all other modern platforms, incoming passwords will
     be properly encoded in UTF-8, so this is almost always what you
     want.

   - :samp:`unicode`: Tells qpdf that the incoming password is UTF-8,
     overriding whatever its automatic detection determines. The only
     difference between this mode and :samp:`auto` is that qpdf will
     fail with an error message if the password is not valid UTF-8
     instead of falling back to :samp:`bytes` mode with a warning.

   - :samp:`bytes`: Interpret the password as a literal byte string.
     For non-Windows platforms, this is what versions of qpdf prior to
     8.4.0 did. For Windows platforms, there is no way to specify
     strings of binary data on the command line directly, but you can
     use a :samp:`@filename` option or :qpdf:ref:`--password-file` to
     do it, in which case this option forces qpdf to respect the
     string of bytes as provided. Note that this option may cause you
     to encrypt PDF files with passwords that will not be usable by
     other readers.

   - :samp:`hex-bytes`: Interpret the password as a hex-encoded
     string. This provides a way to pass binary data as a password on
     all platforms including Windows. As with :samp:`bytes`, this
     option may allow creation of files that can't be opened by other
     readers. This mode affects qpdf's interpretation of passwords
     specified for decrypting files as well as for encrypting them. It
     makes it possible to specify strings that are encoded in some
     manner other than the system's default encoding.

.. qpdf:option:: --suppress-recovery

   .. help: suppress error recovery

      Avoid attempting to recover when errors are found in a file's
      cross reference table or stream lengths.

   Prevents qpdf from attempting to reconstruct a file's cross
   reference table when there are errors reading objects from the
   file. Recovery is triggered by a variety of situations. While
   usually successful, it uses heuristics that don't work on all
   files. If this option is given, :command:`qpdf` fails on the first
   error it encounters.

.. qpdf:option:: --ignore-xref-streams

   .. help: use xref tables rather than streams

      Ignore any cross-reference streams in the file, falling back to
      cross-reference tables or triggering document recovery.

   Tells qpdf to ignore any cross-reference streams, falling back to
   any embedded cross-reference tables or triggering document
   recovery. Ordinarily, qpdf reads cross-reference streams when they
   are present in a PDF file. If this option is specified,
   qpdf will ignore any cross-reference streams for hybrid PDF files.
   The purpose of hybrid files is to make some content available to
   viewers that are not aware of cross-reference streams. It is almost
   never desirable to ignore them. The only time when you might want
   to use this feature is if you are testing creation of hybrid PDF
   files and wish to see how a PDF consumer that doesn't understand
   object and cross-reference streams would interpret such a file.

.. _transformation-options:

PDF Transformation
------------------

.. help-topic transformation: make structural PDF changes

   The options below tell qpdf to apply transformations that change
   the structure without changing the content.

The options discussed in this section tell qpdf to apply
transformations that change the structure of a PDF file without
changing its content. Examples include creating linearized
(web-optimized) files, adding or removing encryption, restructuring
files for older viewers, and rewriting files for human inspection. See
also :ref:`modification-options`.

Related Options
~~~~~~~~~~~~~~~

.. qpdf:option:: --linearize

   .. help: linearize (web-optimize) output

      Create linearized (web-optimized) output files.

   Create linearized (web-optimized) output files. Linearized files
   are formatted in a way that allows compliant readers to begin
   displaying a PDF file before it is fully downloaded. Ordinarily,
   the entire file must be present before it can be rendered because
   important cross-reference information typically appears at the end
   of the file.

.. qpdf:option:: --encrypt user-password owner-password key-length [options] --

   .. help: start encryption options

      Run qpdf --help=encryption for details.

   This flag starts encryption options, used to create encrypted
   files. Please see :ref:`encryption-options` for details.

.. qpdf:option:: --decrypt

   .. help: remove encryption from input file

      Create an unencrypted output file even if the input file was
      encrypted. Normally qpdf preserves whatever encryption was
      present on the input file. This option overrides that behavior.

   Create an output file with no encryption even if the input file is
   encrypted. This option overrides the default behavior of preserving
   whatever encryption was present on the input file. This
   functionality is not intended to be used for bypassing copyright
   restrictions or other restrictions placed on files by their
   producers. See also :qpdf:ref:`--copy-encryption`.

.. qpdf:option:: --copy-encryption=file

   .. help: copy another file's encryption details

      Copy encryption details from the specified file instead of
      preserving the input file's encryption. Use --encryption-file-password
      to specify the encryption file's password.

   Copy all encryption parameters, including the user password, the
   owner password, and all security restrictions, from the specified
   file instead of preserving the encryption details from the input
   file. This works even if only one of the user password or owner
   password is known. If the encryption file requires a password, use
   the :qpdf:ref:`--encryption-file-password` option to set it. Note
   that copying the encryption parameters from a file also copies the
   first half of ``/ID`` from the file since this is part of the
   encryption parameters. This option can be useful if you need to
   decrypt a file to make manual changes to it or to change it outside
   of qpdf, and then want to restore the original encryption on the
   file without having to manual specify all the individual settings.
   See also :qpdf:ref:`--decrypt`.

   Checks for weak cryptographic algorithms are intentionally not made
   by this operation. See :ref:`weak-crypto` for the rationale.

.. qpdf:option:: --encryption-file-password=password

   .. help: supply password for --copy-encryption

      If the file named in --copy-encryption requires a password, use
      this option to supply the password.

   If the file specified with :qpdf:ref:`--copy-encryption`
   requires a password, supply the password using this option. This
   option is necessary because the :qpdf:ref:`--password` option
   applies to the input file, not the file from which encryption is
   being copied.

.. qpdf:option:: --qdf

   .. help: enable viewing PDF code in a text editor

      Create a PDF file suitable for viewing in a text editor and even
      editing. This is for editing the PDF code, not the page contents.
      All streams that can be uncompressed are uncompressed, and
      content streams are normalized, among other changes. The
      companion tool "fix-qdf" can be used to repair hand-edited QDF
      files. QDF is a feature specific to the qpdf tool. Please see
      the "QDF Mode" chapter in the manual.

   Create a PDF file suitable for viewing and editing in a text
   editor. This is to edit the PDF code, not the page contents. To
   edit a QDF file, your text editor must preserve binary data. In a
   QDF file, all streams that can be uncompressed are uncompressed,
   and content streams are normalized, among other changes. The
   companion tool :command:`fix-qdf` can be used to repair hand-edited
   QDF files. QDF is a feature specific to the qpdf tool. For
   additional information, see :ref:`qdf`. Note that
   :qpdf:ref:`--linearize` disables QDF mode.

   QDF mode has full support for object streams, but sometimes it's
   easier to locate a specific object if object streams are disabled.
   When trying to understand some PDF construct by inspecting an
   existing file, it can be useful to combine :samp:`--qdf` with
   :samp:`--object-streams=disable`.

   This flag changes some of the defaults of other options: stream
   data is uncompressed, content streams are normalized, and
   encryption is removed. These defaults can still be overridden by
   specifying the appropriate options with :samp:`--qdf`.
   Additionally, in QDF mode, stream lengths are stored as indirect
   objects, objects are formatted in a less efficient but more
   readable fashion, and the documents are interspersed with comments
   that make it easier for the user to find things and also make it
   possible for :command:`fix-qdf` to work properly. When editing QDF
   files, it is not necessary to maintain the object formatting.

   When normalizing content, if qpdf runs into any lexical errors, it
   will print a warning indicating that content may be damaged. If you
   want to create QDF files without content normalization, specify
   :samp:`--qdf --normalize-content=n`. You can also create a non-QDF
   file with uncompressed streams using
   :samp:`--stream-data=uncompress`. Either option will uncompress all
   the streams but will not attempt to normalize content. Please note
   that if you are using content normalization or QDF mode for the
   purpose of manually inspecting files, you don't have to care about
   this.

   See also :qpdf:ref:`--no-original-object-ids`.

.. qpdf:option:: --no-original-object-ids

   .. help: omit original object IDs in qdf

      Omit comments in a QDF file indicating the object ID an object
      had in the original file.

   Suppresses inclusion of original object ID comments in QDF files.
   This can be useful when generating QDF files for test purposes,
   particularly when comparing them to determine whether two PDF files
   have identical content. The original object ID comment is there by
   default because it makes it easier to trace objects back to the
   original file.

.. qpdf:option:: --compress-streams=[y|n]

   .. help: compress uncompressed streams

      Setting --compress-streams=n prevents qpdf from compressing
      uncompressed streams. This can be useful if you are leaving some
      streams uncompressed intentionally.

   By default, or with :samp:`--compress-streams=y`, qpdf will
   compress streams using the flate compression algorithm (used by zip
   and gzip) unless those streams are compressed in some other way.
   This analysis is made after qpdf attempts to uncompress streams and
   is therefore closely related to :qpdf:ref:`--decode-level`. To
   suppress this behavior and leave streams streams uncompressed, use
   :samp:`--compress-streams=n`. In QDF mode (see :ref:`qdf` and
   :qpdf:ref:`--qdf`), the default is to leave streams uncompressed.

.. qpdf:option:: --decode-level=parameter

   .. help: control which streams to uncompress

      When uncompressing streams, control which types of compression
      schemes should be uncompressed:
      - none: don't uncompress anything. This is the default with
        --json-output.
      - generalized: uncompress streams compressed with a
        general-purpose compression algorithm. This is the default
        except when --json-output is given.
      - specialized: in addition to generalized, also uncompress
        streams compressed with a special-purpose but non-lossy
        compression scheme
      - all: in addition to specialized, uncompress streams compressed
        with lossy compression schemes like JPEG (DCT)
      qpdf does not know how to uncompress all compression schemes.

   Controls which streams qpdf tries to decode. The default is
   :samp:`generalized`.

   The following values for :samp:`{parameter}` are available:

   - :samp:`none`: do not attempt to decode any streams. This is the
     default with :qpdf:ref:`--json-output`.

   - :samp:`generalized`: decode streams filtered with supported
     generalized filters: ``/LZWDecode``, ``/FlateDecode``,
     ``/ASCII85Decode``, and ``/ASCIIHexDecode``. We define
     generalized filters as those to be used for general-purpose
     compression or encoding, as opposed to filters specifically
     designed for image data. This is the default except when
     :qpdf:ref:`--json-output` is given.

   - :samp:`specialized`: in addition to generalized, decode streams
     with supported non-lossy specialized filters; currently this is
     just ``/RunLengthDecode``

   - :samp:`all`: in addition to generalized and specialized, decode
     streams with supported lossy filters; currently this is just
     ``/DCTDecode`` (JPEG)

   There are several filters that :command:`qpdf` does not support.
   These are left untouched regardless of the option. Future versions
   of qpdf may support additional filters.

   Because the default value is ``generalized``, qpdf's default
   behavior is to uncompress any stream that is encoded using
   non-lossy filters that qpdf understands. If
   ``--compress-streams=y`` is also in effect, which is the default
   (see :qpdf:ref:`--compress-streams`), the overall effect is that
   qpdf will recompress streams with generalized filters using flate
   compression, effectively eliminating LZW and ASCII-based filters.
   This is usually desirable behavior but can be disabled with
   ``--decode-level=none``. Note that ``--decode-level=none`` is the
   default when :qpdf:ref:`--json-output` is specified, but it can be
   overridden in that case as well.

   As a special case, streams already compressed with ``/FlateDecode``
   are not uncompressed and recompressed. You can change this behavior
   with :qpdf:ref:`--recompress-flate`.

.. qpdf:option:: --stream-data=parameter

   .. help: control stream compression

      This option controls how streams are compressed in the output.
      It is less granular than the newer options, --compress-streams
      and --decode-level.

      Parameters:
      - compress: same as --compress-streams=y --decode-level=generalized
      - preserve: same as --compress-streams=n --decode-level=none
      - uncompress: same as --compress-streams=n --decode-level=generalized

   Controls transformation of stream data. This option predates the
   :qpdf:ref:`--compress-streams` and :qpdf:ref:`--decode-level`
   options. Those options can be used to achieve the same effect with
   more control. The value of :samp:`{parameter}` may be one of the
   following:

   - :samp:`compress`: recompress stream data when possible (default);
     equivalent to :samp:`--compress-streams=y`
     :samp:`--decode-level=generalized`. Does not recompress streams
     already compressed with ``/FlateDecode`` unless
     :qpdf:ref:`--recompress-flate` is also specified.

   - :samp:`preserve`: leave all stream data as is; equivalent to
     :samp:`--compress-streams=n` :samp:`--decode-level=none`

   - :samp:`uncompress`: uncompress stream data compressed with
     generalized filters when possible; equivalent to
     :samp:`--compress-streams=n` :samp:`--decode-level=generalized`

.. qpdf:option:: --recompress-flate

   .. help: uncompress and recompress flate

      The default generalized compression scheme used by PDF is flate,
      which is the same as used by zip and gzip. Usually qpdf just
      leaves these alone. This option tells qpdf to uncompress and
      recompress streams compressed with flate. This can be useful
      when combined with --compression-level.

   The default generalized compression scheme used by PDF is flate
   (``/FlateDecode``), which is the same as used by :command:`zip` and
   :command:`gzip`. Usually qpdf just leaves these alone. This option
   tells :command:`qpdf` to uncompress and recompress streams
   compressed with flate. This can be useful when combined with
   :qpdf:ref:`--compression-level`. Using this option may make
   :command:`qpdf` much slower when writing output files.

.. qpdf:option:: --compression-level=level

   .. help: set compression level for flate

      Set a compression level from 1 (least, fastest) to 9 (most,
      slowest) when compressing files with flate (used in zip and
      gzip), which is the default compression for most PDF files.
      You need --recompress-flate with this option if you want to
      change already compressed streams.

   When writing new streams that are compressed with ``/FlateDecode``,
   use the specified compression level. The value of :samp:`level`
   should be a number from 1 to 9 and is passed directly to zlib,
   which implements deflate compression. Lower numbers compress less
   and are faster; higher numbers compress more and are slower. Note
   that :command:`qpdf` doesn't uncompress and recompress streams
   compressed with flate by default. To have this option apply to
   already compressed streams, you should also specify
   :qpdf:ref:`--recompress-flate`. If your goal is to shrink the size
   of PDF files, you should also use
   :samp:`--object-streams=generate`. If you omit this option, qpdf
   defers to the compression library's default behavior.

.. qpdf:option:: --normalize-content=[y|n]

   .. help: fix newlines in content streams

      Normalize newlines to UNIX-style newlines in PDF content
      streams, which is useful for viewing them in a programmer's text
      editor across multiple platforms. This is also turned on by
      --qdf.

   Enables or disables normalization of newlines in PDF content
   streams to UNIX-style newlines, which is useful for viewing files
   in a programmer-friendly text edit across multiple platforms.
   Content normalization is off by default, but is automatically
   enabled by :qpdf:ref:`--qdf` (see also :ref:`qdf`). It is not
   recommended to use this option for production use. If qpdf runs
   into any lexical errors while normalizing content, it will print a
   warning indicating that content may be damaged.

.. qpdf:option:: --object-streams=mode

   .. help: control use of object streams

      Control what qpdf does regarding object streams. Options:
      - preserve: preserve original object streams, if any (the default)
      - disable: create output files with no object streams
      - generate: create object streams, and compress objects when possible

   Controls handling of object streams. The value of :samp:`{mode}`
   may be one of the following:

   .. list-table:: Object Stream Modes
      :widths: 10 80
      :header-rows: 0

      - - :samp:`preserve`
        - preserve original object streams, if any (the default)

      - - :samp:`disable`
        - create output files with no object streams

      - - :samp:`generate`
        - create object streams, and compress objects when possible

   Object streams are PDF streams that contain other objects. Putting
   objects into object streams allows the PDF objects themselves to be
   compressed, which can result in much smaller PDF files. Combining
   this option with :qpdf:ref:`--compression-level` and
   :qpdf:ref:`--recompress-flate` can often result in the creation of
   smaller PDF files.

   Object streams, also known as compressed objects, were introduced
   into the PDF specification at version 1.5 around 2003. Some ancient
   PDF viewers may not support files with object streams. qpdf can be
   used to transform files with object streams into files without object
   streams or vice versa.

   In :samp:`preserve` mode, the relationship between objects and the
   streams that contain them is preserved from the original file. If
   the file has no object streams, qpdf will not add any. In
   :samp:`disable` mode, all objects are written as regular,
   uncompressed objects. The resulting file should be structurally
   readable by older PDF viewers, though there is still a chance that
   the file may contain other content that some older readers can't
   support. In :samp:`generate` mode, qpdf will create its own object
   streams. This will usually result in more compact PDF files. In
   this mode, qpdf will also make sure the PDF version number in the
   header is at least 1.5.

.. qpdf:option:: --preserve-unreferenced

   .. help: preserve unreferenced objects

      Preserve all objects from the input even if not referenced.

   Tells qpdf to preserve objects that are not referenced when writing
   the file. Ordinarily any object that is not referenced in a
   traversal of the document from the trailer dictionary will be
   discarded. Disabling this default behavior may be useful in working
   with some damaged files or inspecting files with known unreferenced
   objects.

   This flag is ignored for linearized files and has the effect of
   causing objects in the new file to be written ordered by object ID
   from the original file. This does not mean that object numbers will
   be the same since qpdf may create stream lengths as direct or
   indirect differently from the original file, and the original file
   may have gaps in its numbering.

   See also :qpdf:ref:`--preserve-unreferenced-resources`, which does
   something completely different.

.. qpdf:option:: --remove-unreferenced-resources=parameter

   .. help: remove unreferenced page resources

      Remove from a page's resource dictionary any resources that are
      not referenced in the page's contents. Parameters: "auto"
      (default), "yes", "no".

   Parameters: ``auto`` (the default), ``yes``, or ``no``.

   Starting with qpdf 8.1, when splitting pages, qpdf is able to
   attempt to remove images and fonts that are not used by a page even
   if they are referenced in the page's resources dictionary. When
   shared resources are in use, this behavior can greatly reduce the
   file sizes of split pages, but the analysis is very slow. In
   versions from 8.1 through 9.1.1, qpdf did this analysis by default.
   Starting in qpdf 10.0.0, if ``auto`` is used, qpdf does a quick
   analysis of the file to determine whether the file is likely to
   have unreferenced objects on pages, a pattern that frequently
   occurs when resource dictionaries are shared across multiple pages
   and rarely occurs otherwise. If it discovers this pattern, then it
   will attempt to remove unreferenced resources. Usually this means
   you get the slower splitting speed only when it's actually going to
   create smaller files. You can suppress removal of unreferenced
   resources altogether by specifying ``no`` or force qpdf to do the
   full algorithm by specifying ``yes``.

   Other than cases in which you don't care about file size and care a
   lot about runtime, there are few reasons to use this option,
   especially now that ``auto`` mode is supported. One reason to use
   this is if you suspect that qpdf is removing resources it shouldn't
   be removing. If you encounter such a case, please report it as a bug at
   https://github.com/qpdf/qpdf/issues/.

.. qpdf:option:: --preserve-unreferenced-resources

   .. help: use --remove-unreferenced-resources=no

      Synonym for --remove-unreferenced-resources=no. Use that instead.

   This is a synonym for :samp:`--remove-unreferenced-resources=no`.
   See :qpdf:ref:`--remove-unreferenced-resources`.

   See also :qpdf:ref:`--preserve-unreferenced`, which does something
   completely different. To reduce confusion, you should use
   :samp:`--remove-unreferenced-resources=no` instead.

.. qpdf:option:: --newline-before-endstream

   .. help: force a newline before endstream

      For an extra newline before endstream. Using this option enables
      qpdf to preserve PDF/A when rewriting such files.

   Tell qpdf to insert a newline before the ``endstream`` keyword,
   not counted in the length, after any stream content even if the
   last character of the stream was a newline. This may result in two
   newlines in some cases. This is a requirement of PDF/A. While qpdf
   doesn't specifically know how to generate PDF/A-compliant PDFs,
   this at least prevents it from removing compliance on already
   compliant files.

.. qpdf:option:: --coalesce-contents

   .. help: combine content streams

      If a page has an array of content streams, concatenate them into
      a single content stream.

   When a page's contents are split across multiple streams, this
   option causes qpdf to combine them into a single stream. Use of
   this option is never necessary for ordinary usage, but it can help
   when working with some files in some cases. For example, this can
   be combined with QDF mode or content normalization to make it
   easier to look at all of a page's contents at once. It is common
   for PDF writers to create multiple content streams for a variety of
   reasons such as making it easier to modify page contents and
   splitting very large content streams so PDF viewers may be able to
   use less memory.

.. qpdf:option:: --externalize-inline-images

   .. help: convert inline to regular images

      Convert inline images to regular images.

   Convert inline images to regular images. By default, images whose
   data is at least 1,024 bytes are converted when this option is
   selected. Use :qpdf:ref:`--ii-min-bytes` to change the size
   threshold. This option is implicitly selected when
   :qpdf:ref:`--optimize-images` is selected unless
   :qpdf:ref:`--keep-inline-images` is also specified.

.. qpdf:option:: --ii-min-bytes=size-in-bytes

   .. help: set minimum size for --externalize-inline-images

      Don't externalize inline images smaller than this size. The
      default is 1,024. Use 0 for no minimum.

   Avoid converting inline images whose size is below the specified
   minimum size to regular images. The default is 1,024 bytes. Use 0
   for no minimum.

.. qpdf:option:: --min-version=version

   .. help: set minimum PDF version

      Force the PDF version of the output to be at least the specified
      version. The version number format is
      "major.minor[.extension-level]", which sets the version header
      to "major.minor" and the extension level, if specified, to
      "extension-level".

   Force the PDF version of the output file to be at least
   :samp:`{version}`. In other words, if the input file has a lower
   version than the specified version, the specified version will be
   used. If the input file has a higher version, the input file's
   original version will be used. It is seldom necessary to use this
   option since qpdf will automatically increase the version as needed
   when adding features that require newer PDF readers.

   The version number may be expressed in the form
   :samp:`{major}.{minor}[.{extension-level}]`. If
   :samp:`.{extension-level}`, is given, version is interpreted as
   :samp:`{major.minor}` at extension level :samp:`{extension-level}`.
   For example, version ``1.7.8`` represents version 1.7 at extension
   level 8. Note that minimal syntax checking is done on the command
   line. :command:`qpdf` does not check whether the specified version
   is actually required.

.. qpdf:option:: --force-version=version

   .. help: set output PDF version

      Force the output PDF file's PDF version header to be the specified
      value, even if the file uses features that may not be available
      in that version.

   This option forces the PDF version to be the exact version
   specified *even when the file may have content that is not
   supported in that version*. The version number is interpreted in
   the same way as with :qpdf:ref:`--min-version` so that extension
   levels can be set. In some cases, forcing the output file's PDF
   version to be lower than that of the input file will cause qpdf to
   disable certain features of the document. Specifically, 256-bit
   keys are disabled if the version is less than 1.7 with extension
   level 8 (except the deprecated, unsupported "R5" format is allowed
   with extension levels 3 through 7), AES encryption is disabled if
   the version is less than 1.6, cleartext metadata and object streams
   are disabled if less than 1.5, 128-bit encryption keys are disabled
   if less than 1.4, and all encryption is disabled if less than 1.3.
   Even with these precautions, qpdf won't be able to do things like
   eliminate use of newer image compression schemes, transparency
   groups, or other features that may have been added in more recent
   versions of PDF.

   As a general rule, with the exception of big structural things like
   the use of object streams or AES encryption, PDF viewers are
   supposed to ignore features they don't support. This means that
   forcing the version to a lower version may make it possible to open
   your PDF file with an older version, though bear in mind that some
   of the original document's functionality may be lost.

.. _page-ranges:

Page Ranges
-----------

.. help-topic page-ranges: page range syntax

   A full description of the page range syntax, with examples, can be
   found in the manual. Summary:

   - a,b,c    pages a, b, and c
   - a-b      pages a through b inclusive; if a > b, this counts down
   - r<n>     where <n> represents a number is the <n>th page from the end
   - z        the last page, same as r1

   You can append :even or :odd to select every other page from the
   resulting set of pages, where :odd starts with the first page and
   :even starts with the second page. These are odd and even pages
   from the resulting set, not based on the original page numbers.

Several :command:`qpdf` command-line options use page ranges. This
section describes the syntax of a page range.

- A plain number indicates a page numbered from ``1``, so ``1``
  represents the first page.

- A number preceded by ``r`` counts from the end, so ``r1`` is the
  last page, ``r2`` is the second-to-last page, etc.

- The letter ``z`` represents the last page and is the same as ``r1``.

- Page numbers may appear in any order separated by commas.

- Two page numbers separated by dashes represents the inclusive range
  of pages from the first to the second. If the first number is higher
  than the second number, it is the range of pages in reverse.

- The range may be appended with ``:odd`` or ``:even`` to select only
  pages from the resulting range in odd or even positions. In this
  case, odd and even refer to positions in the final range, not
  whether the original page number is odd or even.

.. list-table:: Example Page Ranges
   :widths: 20 80
   :header-rows: 0

   - - ``1,6,4``
     - pages 1, 6, and 4 in that order

   - - ``3-7``
     - pages 3 through 7 inclusive in increasing order

   - - ``7-3``
     - pages 7, 6, 5, 4, and 3 in that order

   - - ``1-z``
     - all pages in order

   - - ``z-1``
     - all pages in reverse order

   - - ``1,3,5-9,15-12``
     - pages 1, 3, 5, 6, 7, 8, 9, 15, 14, 13, and 12 in that order

   - - ``r3-r1``
     - the last three pages of the document

   - - ``r1-r3``
     - the last three pages of the document in reverse order

   - - ``1-20:even``
     - even pages from 2 to 20

   - - ``5,7-9,12``
     - pages 5, 7, 8, 9, and 12

   - - ``5,7-9,12:odd``
     - pages 5, 8, and 12, which are the pages in odd positions from
       the original set of 5, 7, 8, 9, 12

   - - ``5,7-9,12:even``
     - pages 7 and 9, which are the pages in even positions from the
       original set of 5, 7, 8, 9, 12

.. _modification-options:

PDF Modification
----------------

.. help-topic modification: change parts of the PDF

   Modification options make systematic changes to certain parts of
   the PDF, causing the PDF to render differently from the original.

Modification options make systematic changes to certain parts of the
PDF, causing the PDF to render differently from the original. See also
:ref:`transformation-options`.

Related Options
~~~~~~~~~~~~~~~

.. qpdf:option:: --pages file [--password=password] [page-range] [...] --

   .. help: begin page selection

      Run qpdf --help=page-selection for details.

   This flag starts page selection options, which are used to select
   pages from one or more input files to perform operations such as
   splitting, merging, and collating files.

   Please see :ref:`page-selection` for details about selecting pages.

   See also :qpdf:ref:`--split-pages`, :qpdf:ref:`--collate`,
   :ref:`page-ranges`.

.. qpdf:option:: --collate[=n]

   .. help: collate with --pages

      Collate rather than concatenate pages specified with --pages.
      With a numeric parameter, collate in groups of n. The default
      is 1. Run qpdf --help=page-selection for additional details.

   This option causes :command:`qpdf` to collate rather than
   concatenate pages specified with :qpdf:ref:`--pages`. With a
   numeric parameter, collate in groups of :samp:`{n}`. The default
   is 1.

   Please see :ref:`page-selection` for additional details.

.. qpdf:option:: --split-pages[=n]

   .. help: write pages to separate files

      This option causes qpdf to create separate output files for each
      page or group of pages rather than a single output file.

      File names are generated from the specified output file as follows:

      - If the string %d appears in the output file name, it is replaced with a
        zero-padded page range starting from 1
      - Otherwise, if the output file name ends in .pdf (case insensitive), a
        zero-padded page range, preceded by a dash, is inserted before the file
        extension
      - Otherwise, the file name is appended with a zero-padded page range
        preceded by a dash.

      Page ranges are single page numbers for single-page groups or first-last
      for multi-page groups.

   Write each group of :samp:`{n}` pages to a separate output file. If
   :samp:`{n}` is not specified, create single pages. Output file
   names are generated as follows:

   - If the string ``%d`` appears in the output file name, it is
     replaced with a range of zero-padded page numbers starting
     from 1.

   - Otherwise, if the output file name ends in :file:`.pdf` (case
     insensitive), a zero-padded page range, preceded by a dash, is
     inserted before the file extension.

   - Otherwise, the file name is appended with a zero-padded page
     range preceded by a dash.

   Zero padding is added to all page numbers in file names so that all
   the numbers are the same length, which causes the output filenames
   to sort lexically in numerical order.

   Page ranges are a single number in the case of single-page groups or
   two numbers separated by a dash otherwise.

   Here are some examples. In these examples, :file:`infile.pdf` has
   12 pages.

   - ``qpdf --split-pages infile.pdf %d-out``: output files are
     :file:`01-out` through :file:`12-out` with no extension.

   - ``qpdf --split-pages=2 infile.pdf outfile.pdf``: output files are
     :file:`outfile-01-02.pdf` through :file:`outfile-11-12.pdf`

   - ``qpdf --split-pages infile.pdf something.else`` would generate
     files :file:`something.else-01` through
     :file:`something.else-12`. The extension ``.else`` is not treated
     in any special way regarding the placement of the number.

   Note that outlines, threads, and other document-level features of
   the original PDF file are not preserved. For each page of output,
   this option creates an empty PDF and copies a single page from the
   output into it. If you require the document-level data, you will
   have to run :command:`qpdf` with the :qpdf:ref:`--pages` option
   once for each page. Using :qpdf:ref:`--split-pages` is much faster
   if you don't require the document-level data. A future version of
   qpdf may support preservation of some document-level information.

.. qpdf:option:: --overlay file [options] --

   .. help: begin overlay options

      Overlay pages from another file on the output.
      Run qpdf --help=overlay-underlay for details.

   Overlay pages from another file on the output.

   See :ref:`overlay-underlay` for details.

.. qpdf:option:: --underlay file [options] --

   .. help: begin underlay options

      Underlay pages from another file on the output.
      Run qpdf --help=overlay-underlay for details.

   Underlay pages from another file on the output.

   See :ref:`overlay-underlay` for details.

.. qpdf:option:: --flatten-rotation

   .. help: remove rotation from page dictionary

      For each page that is rotated using the /Rotate key in the
      page's dictionary, remove the /Rotate key and implement the
      identical rotation semantics by modifying the page's contents.
      This can be useful if a broken PDF viewer fails to properly
      consider page rotation metadata.

   For each page that is rotated using the ``/Rotate`` key in the
   page's dictionary, remove the ``/Rotate`` key and implement the
   identical rotation semantics by modifying the page's contents. This
   option can be useful to prepare files for buggy PDF applications
   that don't properly handle rotated pages. There is usually no
   reason to use this option unless you are working around a specific
   problem.

.. qpdf:option:: --flatten-annotations=parameter

   .. help: push annotations into content

      Push page annotations into the content streams. This may be
      necessary in some case when printing or splitting files.
      Parameters: "all", "print", "screen".

   This option collapses annotations into the pages' contents with
   special handling for form fields. Ordinarily, an annotation is
   rendered separately and on top of the page. Combining annotations
   into the page's contents effectively freezes the placement of the
   annotations, making them look right after various page
   transformations. The library functionality backing this option was
   added for the benefit of programs that want to create *n-up* page
   layouts and other similar things that don't work well with
   annotations. The value of :samp:`{parameter}` may be any of the
   following:

   .. list-table:: Flatten Annotation Parameters
      :widths: 10 80
      :header-rows: 0

      - - :samp:`all`
        - include all annotations that are not marked invisible or
          hidden

      - - :samp:`print`
        - only include annotations that should appear when the page is
          printed

      - - :samp:`screen`
        - omit annotations that should not appear on the screen

   In a PDF file, interactive form fields have a value and,
   independently, a set of instructions, called an appearance, to
   render the filled-in field. If a form is filled in by a program
   that doesn't know how to update the appearances, they may become
   inconsistent with the fields' values. If qpdf detects this case,
   its default behavior is not to flatten those annotations because
   doing so would cause the value of the form field to be lost. This
   gives you a chance to go back and resave the form with a program
   that knows how to generate appearances. qpdf itself can generate
   appearances with some limitations. See the
   :qpdf:ref:`--generate-appearances` option for details.

.. qpdf:option:: --rotate=[+|-]angle[:page-range]

   .. help: rotate pages

      Rotate specified pages by multiples of 90 degrees specifying
      either absolute or relative angles. "angle" may be 0, 90, 180,
      or 270. You almost always want to use +angle or -angle rather
      than just angle, as discussed in the manual. Run
      qpdf --help=page-ranges for help with page ranges.

   Rotate the specified range of pages by the specified angle, which
   must be a multiple of 90 degrees.

   The value of :samp:`{angle}` may be ``0``, ``90``, ``180``, or ``270``.

   For a description of the syntax of :samp:`{page-range}`, see
   :ref:`page-ranges`. If the page range is omitted, the rotation is
   applied to all pages.

   If ``+`` is prepended to :samp:`{angle}`, the angle is added, so an
   angle of ``+90`` indicates a 90-degree clockwise rotation. If ``-``
   is prepended, the angle is subtracted, so ``-90`` is a 90-degree
   counterclockwise rotation and is exactly the same as ``+270``.

   If neither ``+`` or ``-`` is prepended, the rotation angle is set
   exactly. You almost always want ``+`` or ``-`` since, without
   inspecting the actual PDF code, it is impossible to know whether a
   page that appears to be rotate is rotated "naturally" or has been
   rotated by specifying rotation. For example, if a page appears to
   contain a portrait-mode image rotated by 90 degrees so that the top
   of the image is on the right edge of the page, there is no way to
   tell by visual inspection whether the literal top of the image is
   the top of the page or whether the literal top of the image is the
   right edge and the page is already rotated in the PDF. Specifying a
   rotation angle of ``-90`` will produce an image that appears
   upright in either case. Use of absolute rotation angles should be
   reserved for cases in which you have specific knowledge about the
   way the PDF file is constructed.

   Examples:

   - ``qpdf in.pdf out.pdf --rotate=+90:2,4,6 --rotate=+180:7-8``:
     rotate pages 2, 4, and 6 by 90 degrees clockwise from their
     original rotation

   - ``qpdf in.pdf out.pdf --rotate=+180``: rotate all pages by 180
     degrees

   - ``qpdf in.pdf out.pdf --rotate=0``: force each page to be displayed
     in its natural orientation, which would undo the effect of any
     rotations previously applied in page metadata.

   See also :qpdf:ref:`--flatten-rotation`.

.. qpdf:option:: --generate-appearances

   .. help: generate appearances for form fields

      PDF form fields consist of values and appearances, which may be
      inconsistent with each other if a form field value has been
      modified without updating its appearance. This option tells qpdf
      to generate new appearance streams. There are some limitations,
      which are discussed in the manual.

   If a file contains interactive form fields and indicates that the
   appearances are out of date with the values of the form, this flag
   will regenerate appearances, subject to a few limitations. Note
   that there is usually no reason to do this, but it can be
   necessary before using the :qpdf:ref:`--flatten-annotations`
   option. Here is a summary of the limitations.

   - Radio button and checkbox appearances use the pre-set values in
     the PDF file. :command:`qpdf` just makes sure that the correct
     appearance is displayed based on the value of the field. This is
     fine for PDF files that create their forms properly. Some PDF
     writers save appearances for fields when they change, which could
     cause some controls to have inconsistent appearances.

   - For text fields and list boxes, any characters that fall outside
     of US-ASCII or, if detected, "Windows ANSI" or "Mac Roman"
     encoding, will be replaced by the ``?`` character.
     :command:`qpdf` does not know enough about fonts and encodings to
     correctly represent characters that fall outside of this range.

   - For variable text fields where the default appearance stream
     specifies that the font should be auto-sized, a fixed font size
     is used rather than calculating the font size.

   - Quadding is ignored. Quadding is used to specify whether the
     contents of a field should be left, center, or right aligned with
     the field.

   - Rich text, multi-line, and other more elaborate formatting
     directives are ignored.

   - There is no support for multi-select fields or signature fields.

   Appearances generated by :command:`qpdf` should be good enough for
   simple forms consisting of ASCII characters where the original file
   followed the PDF specification and provided template information
   for text field appearances. If :command:`qpdf` doesn't do a good
   enough job with your form, use an external application to save your
   filled-in form before processing it with :command:`qpdf`. Most PDF
   viewers that support filling in of forms will generate appearance
   streams. Some of them will even do it for forms filled in with
   characters outside the original font's character range by embedding
   additional fonts as needed.

.. qpdf:option:: --optimize-images

   .. help: use efficient compression for images

      Attempt to use DCT (JPEG) compression for images that fall
      within certain constraints as long as doing so decreases the
      size in bytes of the image. See also help for the following
      options:
        --oi-min-width
        --oi-min-height
        --oi-min-area
        --keep-inline-images

   This flag causes qpdf to recompress all images that are not
   compressed with DCT (JPEG) using DCT compression as long as doing
   so decreases the size in bytes of the image data and the image does
   not fall below minimum specified dimensions. Useful information is
   provided when used in combination with :qpdf:ref:`--verbose`. See
   also the :qpdf:ref:`--oi-min-width`, :qpdf:ref:`--oi-min-height`,
   and :qpdf:ref:`--oi-min-area` options. By default, inline images
   are converted to regular images and optimized as well. Use
   :qpdf:ref:`--keep-inline-images` to prevent inline images from
   being included.

.. qpdf:option:: --oi-min-width=width

   .. help: minimum width for --optimize-images

      Don't optimize images whose width is below the specified value.

   Avoid optimizing images whose width is below the specified amount. If
   omitted, the default is 128 pixels. Use 0 for no minimum.

.. qpdf:option:: --oi-min-height=height

   .. help: minimum height for --optimize-images

      Don't optimize images whose height is below the specified value.

   Avoid optimizing images whose height is below the specified amount.
   If omitted, the default is 128 pixels. Use 0 for no minimum.

.. qpdf:option:: --oi-min-area=area-in-pixels

   .. help: minimum area for --optimize-images

      Don't optimize images whose area in pixels is below the specified value.

   Avoid optimizing images whose pixel count
   (:samp:`{width}` × :samp:`{height}`) is below the specified amount.
   If omitted, the default is 16,384 pixels. Use 0 for no minimum.

.. qpdf:option:: --keep-inline-images

   .. help: exclude inline images from optimization

      Prevent inline images from being considered by --optimize-images.

   Prevent inline images from being included in image optimization
   done by :qpdf:ref:`--optimize-images`.

.. qpdf:option:: --remove-page-labels

   .. help: remove explicit page numbers

      Exclude page labels (explicit page numbers) from the output file.

   Exclude page labels (explicit page numbers) from the output file.

.. _encryption-options:

Encryption
----------

.. help-topic encryption: create encrypted files

   Create encrypted files. Usage:

   --encrypt user-password owner-password key-length [options] --

   Either or both of user-password and owner-password may be empty
   strings. key-length may be 40, 128, or 256. Encryption options are
   terminated by "--" by itself.

   40-bit encryption is insecure, as is 128-bit encryption without
   AES. Use 256-bit encryption unless you have a specific reason to
   use an insecure format, such as testing or compatibility with very
   old viewers. You must use the --allow-weak-crypto to create
   encrypted files that use insecure cryptographic algorithms. The
   --allow-weak-crypto flag appears outside of --encrypt ... --
   (before --encrypt or after --).

   Available options vary by key length. Not all readers respect all
   restrictions. Different PDF readers respond differently to various
   combinations of options. Sometimes a PDF viewer may show you
   restrictions that differ from what you selected. This is probably
   not a bug in qpdf.

   Options for 40-bit only:
     --annotate=[y|n]         restrict comments, filling forms, and signing
     --extract=[y|n]          restrict text/graphic extraction
     --modify=[y|n]           restrict document modification
     --print=[y|n]            restrict printing

   Options for 128-bit or 256-bit:
     --accessibility=[y|n]    restrict accessibility (usually ignored)
     --annotate=[y|n]         restrict commenting/filling form fields
     --assemble=[y|n]         restrict document assembly
     --extract=[y|n]          restrict text/graphic extraction
     --form=[y|n]             restrict filling form fields
     --modify-other=[y|n]     restrict other modifications
     --modify=modify-opt      control modify access by level
     --print=print-opt        control printing access
     --cleartext-metadata     prevent encryption of metadata

   For 128-bit only:
     --use-aes=[y|n]          indicates whether to use AES encryption
     --force-V4               forces use of V=4 encryption handler

   For 256-bit only:
     --force-R5               forces use of deprecated R=5 encryption
     --allow-insecure         allow user password with empty owner password

   Values for print-opt:
     none                     disallow printing
     low                      allow only low-resolution printing
     full                     allow full printing

   Values for modify-opt:
     none                     allow no modifications
     assembly                 allow document assembly only
     form                     assembly + filling in form fields and signing
     annotate                 form + commenting and modifying forms
     all                      allow full document modification

This section describes the options used to create encrypted files. For
other options related to encryption, see also :qpdf:ref:`--decrypt`
and :qpdf:ref:`--copy-encryption`. For a more in-depth technical
discussion of how PDF encryption works internally, see
:ref:`pdf-encryption`.

To create an encrypted file, use

::

   --encrypt user-password owner-password key-length [options] --

Either or both of :samp:`{user-password}` and :samp:`{owner-password}`
may be empty strings. :samp:`{key-length}` may be ``40``, ``128``, or
``256``. Encryption options are terminated by ``--`` by itself.

40-bit encryption is insecure, as is 128-bit encryption without AES.
Use 256-bit encryption unless you have a specific reason to use an
insecure format, such as testing or compatibility with very old
viewers. You must use the :qpdf:ref:`--allow-weak-crypto` flag to
create encrypted files that use insecure cryptographic algorithms. The
:qpdf:ref:`--allow-weak-crypto` flag appears outside of ``--encrypt
... --`` (before ``--encrypt`` or after ``--``).

If :samp:`{key-length}` is 256, the minimum PDF version is 1.7 with
extension level 8, and the AES-based encryption format used is the one
described in the PDF 2.0 specification. Using 128-bit encryption
forces the PDF version to be at least 1.4, or if AES is used, 1.6.
Using 40-bit encryption forces the PDF version to be at least 1.3.

When 256-bit encryption is used, PDF files with empty owner
passwords are insecure. To create such files, you must specify the
:qpdf:ref:`--allow-insecure` option.

Available options vary by key length. Not all readers respect all
restrictions. The default for each permission option is to be fully
permissive. These restrictions may or may not be enforced by any
particular reader. :command:`qpdf` allows very granular setting of
restrictions. Some readers may not recognize the combination of
options you specify. If you specify certain combinations of
restrictions and find a reader that doesn't seem to honor them as you
expect, it is most likely not a bug in :command:`qpdf`. qpdf itself
does not obey encryption restrictions already imposed on the file.
Doing so would be meaningless since qpdf can be used to remove
encryption from the file entirely.

Here is a summary of encryption options. Details are provided in the
help for each option.

.. list-table:: Options for 40-bit Encryption Only
   :widths: 30 70
   :header-rows: 0

   - - ``--annotate=[y|n]``
     - restrict comments, filling forms, and signing

   - - ``--extract=[y|n]``
     - restrict text/graphic extraction

   - - ``--modify=[y|n]``
     - restrict document modification

   - - ``--print=[y|n]``
     - restrict printing

.. list-table:: Options for 128-bit or 256-bit Encryption
   :widths: 30 70
   :header-rows: 0

   - - ``--accessibility=[y|n]``
     - restrict accessibility (usually ignored)

   - - ``--annotate=[y|n]``
     - restrict commenting/filling form fields

   - - ``--assemble=[y|n]``
     - restrict document assembly

   - - ``--extract=[y|n]``
     - restrict text/graphic extraction

   - - ``--form=[y|n]``
     - restrict filling form fields

   - - ``--modify-other=[y|n]``
     - restrict other modifications

   - - ``--modify=modify-opt``
     - control modify access by level

   - - ``--print=print-opt``
     - control printing access

   - - ``--cleartext-metadata``
     - prevent encryption of metadata

.. list-table:: Options for 128-bit Encryption Only
   :widths: 35 65
   :header-rows: 0

   - - ``--use-aes=[y|n]``
     - indicates whether to use AES encryption

   - - ``--force-V4``
     - forces use of V=4 encryption handler

.. list-table:: Options for 256-bit Encryption Only
   :widths: 30 70
   :header-rows: 0

   - - ``--force-R5``
     - forces use of deprecated ``R=5`` encryption algorithm

   - - ``--allow-insecure``
     - allow user password with empty owner password

.. list-table:: Values for :samp:`{print-opt}`
   :widths: 20 80
   :header-rows: 0

   - - ``none``
     - disallow printing

   - - ``low``
     - allow only low-resolution printing

   - - ``full``
     - allow full printing

.. list-table:: Values for :samp:`{modify-opt}`
   :widths: 20 80
   :header-rows: 0

   - - ``none``
     - allow no modifications

   - - ``assembly``
     - allow document assembly only

   - - ``form``
     - ``assembly`` permissions plus filling in form fields and
       signing

   - - ``annotate``
     - ``form`` permissions plus commenting and modifying forms

   - - ``all``
     - allow full document modification

Related Options
~~~~~~~~~~~~~~~

.. qpdf:option:: --accessibility=[y|n]

   .. help: restrict document accessibility

      This option is ignored except with very old encryption formats.
      The current PDF specification does not allow restriction of
      document accessibility. This option is not available with 40-bit
      encryption.

   Enable/disable extraction of text for accessibility to visually
   impaired users. The default is to be fully permissive. The qpdf
   library disregards this field when AES is used with 128-bit
   encryption or when 256-bit encryption is used. You should never
   disable accessibility unless you are explicitly doing so for
   creating test files. The PDF spec says that conforming readers
   should disregard this permission and always allow accessibility.

   This option is not available with 40-bit encryption.

.. qpdf:option:: --annotate=[y|n]

   .. help: restrict document annotation

      Enable/disable modifying annotations including making comments
      and filling in form fields. For 128-bit and 256-bit encryption,
      this also enables editing, creating, and deleting form fields
      unless --modify-other=n or --modify=none is also specified.

   Enable/disable modifying annotations including making comments and
   filling in form fields. The default is to be fully permissive. For
   128-bit and 256-bit encryption, this also enables editing,
   creating, and deleting form fields unless :samp:`--modify-other=n`
   or :samp:`--modify=none` is also specified.

.. qpdf:option:: --assemble=[y|n]

   .. help: restrict document assembly

      Enable/disable document assembly (rotation and reordering of
      pages). This option is not available with 40-bit encryption.

   Enable/disable document assembly (rotation and reordering of
   pages). The default is to be fully permissive.

   This option is not available with 40-bit encryption.

.. qpdf:option:: --extract=[y|n]

   .. help: restrict text/graphic extraction

      Enable/disable text/graphic extraction for purposes other than
      accessibility.

   Enable/disable text/graphic extraction for purposes other than
   accessibility. The default is to be fully permissive.

.. qpdf:option:: --form=[y|n]

   .. help: restrict form filling

      Enable/disable whether filling form fields is allowed even if
      modification of annotations is disabled. This option is not
      available with 40-bit encryption.

   Enable/disable whether filling form fields is allowed even if
   modification of annotations is disabled. The default is to be fully
   permissive.

   This option is not available with 40-bit encryption.

.. qpdf:option:: --modify-other=[y|n]

   .. help: restrict other modifications

      Enable/disable modifications not controlled by --assemble,
      --annotate, or --form. --modify-other=n is implied by any of the
      other --modify options. This option is not available with 40-bit
      encryption.

   Enable/disable modifications not controlled by
   :qpdf:ref:`--assemble`, :qpdf:ref:`--annotate`, or
   :qpdf:ref:`--form`. ``--modify-other=n`` is implied by any of the
   other :qpdf:ref:`--modify` options except for ``--modify=all``. The
   default is to be fully permissive.

   This option is not available with 40-bit encryption.

.. qpdf:option:: --modify=modify-opt

   .. help: restrict document modification

      For 40-bit files, modify-opt may only be y or n and controls all
      aspects of document modification.

      For 128-bit and 256-bit encryption, modify-opt values allow
      enabling and disabling levels of restriction in a manner similar
      to how some PDF creation tools do it. modify-opt values map to
      other combinations of options as follows:

      all: allow full modification (the default)
      annotate: --modify-other=n
      form: --modify-other=n --annotate=n
      assembly: --modify-other=n --annotate=n --form=n
      none: --modify-other=n --annotate=n --form=n --assemble=n

   For 40-bit files, :samp:`{modify-opt}` may only be ``y`` or ``n``
   and controls all aspects of document modification. The default is
   to be fully permissive.

   For 128-bit and 256-bit encryption, :samp:`{modify-opt}` values
   allow enabling and disabling levels of restriction in a manner
   similar to how some PDF creation tools do it:

   .. list-table:: :samp:`{modify-opt}` for 128-bit and 256-bit Encryption
      :widths: 10 80
      :header-rows: 0

      - - ``none``
        - allow no modifications

      - - ``assembly``
        - allow document assembly only

      - - ``form``
        - ``assembly`` permissions plus filling in form fields and
          signing

      - - ``annotate``
        - ``form`` permissions plus commenting and modifying forms

      - - ``all``
        - allow full document modification (the default)

   Modify options correspond to the more granular options as follows:

   .. list-table:: Mapping :samp:`{modify-opt}` to Other Options
      :widths: 10 80
      :header-rows: 0

      - - ``none``
        - ``--modify-other=n --annotate=n --form=n --assemble=n``

      - - ``assembly``
        - ``--modify-other=n --annotate=n --form=n``

      - - ``form``
        - ``--modify-other=n --annotate=n``

      - - ``annotate``
        - ``--modify-other=n``

      - - ``all``
        - no other modify options (the default)

   You can combine this option with the options listed above. If you
   do, later options override earlier options.

.. qpdf:option:: --print=print-opt

   .. help: restrict printing

      Control what kind of printing is allowed. For 40-bit encryption,
      print-opt may only be y or n and enables or disables all
      printing. For 128-bit and 256-bit encryption, print-opt may have
      the following values:

      none: disallow printing
      low: allow low-resolution printing only
      full: allow full printing (the default)

   Control what kind of printing is allowed. The default is to be
   fully permissive. For 40-bit encryption, :samp:`{print-opt}` may
   only be ``y`` or ``n`` and enables or disables all printing. For
   128-bit and 256-bit encryption, :samp:`{print-opt}` may have the
   following values:

   .. list-table:: :samp:`{print-opt}` Values
      :widths: 10 80
      :header-rows: 0

      - - :samp:`none`
        - disallow printing

      - - :samp:`low`
        - allow low-resolution printing only

      - - :samp:`full`
        - allow full printing (the default)

.. qpdf:option:: --cleartext-metadata

   .. help: don't encrypt metadata

      If specified, don't encrypt document metadata even when
      encrypting the rest of the document. This option is not
      available with 40-bit encryption.

   If specified, any metadata stream in the document will be left
   unencrypted even if the rest of the document is encrypted. This also
   forces the PDF version to be at least 1.5.

   This option is not available with 40-bit encryption.

.. qpdf:option:: --use-aes=[y|n]

   .. help: use AES with 128-bit encryption

      Enables/disables use of the more secure AES encryption with
      128-bit encryption. Specifying --use-aes=y forces the PDF
      version to be at least 1.6. This option is only available with
      128-bit encryption. The default is "n" for compatibility
      reasons. Use 256-bit encryption instead.

   Enables/disables use of the more secure AES encryption with 128-bit
   encryption. Specifying ``--use-aes=y`` forces the PDF version to be
   at least 1.6. This option is only available with 128-bit
   encryption. The default is ``n`` for compatibility reasons. Use
   256-bit encryption instead.

.. qpdf:option:: --allow-insecure

   .. help: allow empty owner passwords

      Allow creation of PDF files with empty owner passwords and
      non-empty user passwords when using 256-bit encryption.

   Allow creation of PDF files with 256-bit keys where the user
   password is non-empty and the owner password is empty. Files
   created in this way are insecure since they can be opened without a
   password, and restrictions will not be enforced. Users would
   ordinarily never want to create such files. If you are using qpdf
   to intentionally created strange files for testing (a valid use of
   qpdf!), this option allows you to create such insecure files. This
   option is only available with 256-bit encryption.

   See :ref:`pdf-passwords` for a more technical discussion of this
   issue.

.. qpdf:option:: --force-V4

   .. help: force V=4 in encryption dictionary

      This option is for testing and is never needed in practice since
      qpdf does this automatically when needed.

   Use of this option forces the ``V`` and ``R`` parameters in the
   document's encryption dictionary to be set to the value ``4``. As
   qpdf will automatically do this when required, there is no reason
   to ever use this option. It exists primarily for use in testing
   qpdf itself. This option also forces the PDF version to be at least
   1.5.

.. qpdf:option:: --force-R5

   .. help: use unsupported R=5 encryption

      Use an undocumented, unsupported, deprecated encryption
      algorithm that existed only in Acrobat version IX. This option
      should not be used except for compatibility testing.

   Use an undocumented, unsupported, deprecated encryption algorithm
   that existed only in Acrobat version IX. This option should not be
   used except for compatibility testing. If specified, qpdf sets the
   minimum version to 1.7 at extension level 3.

.. _page-selection:

Page Selection
--------------

.. help-topic page-selection: select pages from one or more files

   Use the --pages option to select pages from multiple files. Usage:

   qpdf in.pdf --pages input-file [--password=password] [page-range] \
       [...] -- out.pdf

   Between --pages and the -- that terminates pages option, repeat
   the following:

   filename [--password=password] [page-range]

   Document-level information, such as outlines, tags, etc., is taken
   from in.pdf and is preserved in out.pdf. You can use --empty in place
   of an input file to start from an empty file and just copy pages
   equally from all files. You can use "." as a shorthand for the
   primary input file (if not --empty). In the above example, "."
   would refer to in.pdf.

   Use --password=password to specify the password for a
   password-protected input file. If the same input file is used more
   than once, you only need to supply the password the first time. If
   the page range is omitted, all pages are selected.

   Run qpdf --help=page-ranges for help with page ranges.

   Use --collate=n to cause pages to be collated in groups of n pages
   (default 1) instead of concatenating the input.

   Examples:

   - Start with in.pdf and append all pages from a.pdf and the even
     pages from b.pdf, and write the output to out.pdf. Document-level
     information from in.pdf is retained. Note the use of "." to refer
     to in.pdf.

     qpdf in.pdf --pages . a.pdf b.pdf:even -- out.pdf

   - Take all the pages from a.pdf, all the pages from b.pdf in
     reverse, and only pages 3 and 6 from c.pdf and write the result
     to out.pdf. Use password "x" to open b.pdf:

     qpdf --empty --pages a.pdf b.pdf --password=x z-1 c.pdf 3,6

   More examples are in the manual.

:command:`qpdf` allows you to use the :qpdf:ref:`--pages` option to
split and merge PDF files by selecting pages from one or more input
files.

Usage: :samp:`qpdf {in.pdf} --pages input-file [--password={password}] [{page-range}] [...] -- {out.pdf}`

Between ``--pages`` and the ``--`` that terminates pages option,
repeat the following:

:samp:`{filename} [--password={password}] [{page-range}]`

Notes:
  - The password option is needed only for password-protected files.
    If you specify the same file more than once, you only need to supply
    the password the first time.

  - The page range may be omitted. If omitted, all pages are included.

  - Document-level information, such as outlines, tags, etc., is taken
    from the primary input file (in the above example, :file:`in.pdf`)
    and is preserved in :file:`out.pdf`. You can use
    :qpdf:ref:`--empty` in place of an input file to start from an
    empty file and just copy pages equally from all files.

  - You can use ``.`` as a shorthand for the primary input file, if not
    empty.

See :ref:`page-ranges` for help on specifying a page range.

Use :samp:`--collate={n}` to cause pages to be collated in groups of
:samp:`{n}` pages (default 1) instead of concatenating the input. Note
that the :qpdf:ref:`--collate` appears outside of ``--pages ... --``
(before ``--pages`` or after ``--``). Pages are pulled from each
document in turn. When a document is out of pages, it is skipped. See
examples below.

Examples
~~~~~~~~

- Start with :file:`in.pdf` and append all pages from :file:`a.pdf`
  and the even pages from :file:`b.pdf`, and write the output to
  :file:`out.pdf`. Document-level information from :file:`in.pdf` is
  retained. Note the use of ``.`` to refer to :file:`in.pdf`.

  ::

     qpdf in.pdf --pages . a.pdf b.pdf:even -- out.pdf


- Take all the pages from :file:`a.pdf`, all the pages from
  :file:`b.pdf` in reverse, and only pages 3 and 6 from :file:`c.pdf`
  and write the result to :file:`out.pdf`. Document-level metadata is
  discarded from all input files. The password ``x`` is used to open
  :file:`b.pdf`.

  ::

     qpdf --empty --pages a.pdf b.pdf --password=x z-1 c.pdf 3,6

- Scan a document with double-sided printing by scanning the fronts
  into :file:`odd.pdf` and the backs into :file:`even.pdf`. Collate
  the results into :file:`all.pdf`. This takes the first page of
  :file:`odd.pdf`, the first page of :file:`even.pdf`, the second page
  of :file:`odd.pdf`, the second page of :file:`even.pdf`, etc.

  ::

     qpdf --collate odd.pdf --pages . even.pdf -- all.pdf
       OR
     qpdf --collate --empty --pages odd.pdf even.pdf -- all.pdf

- When collating, any number of files and page ranges can be
  specified. If any file has fewer pages, that file is just skipped
  when its pages have all been included. For example, if you ran

  ::

     qpdf --collate --empty --pages a.pdf 1-5 b.pdf 6-4 c.pdf r1 -- out.pdf

  you would get the following pages in this order:

  - a.pdf page 1

  - b.pdf page 6

  - c.pdf last page

  - a.pdf page 2

  - b.pdf page 5

  - a.pdf page 3

  - b.pdf page 4

  - a.pdf page 4

  - a.pdf page 5

- You can specify a numeric parameter to :qpdf:ref:`--collate`. With
  :samp:`--collate={n}`, pull groups of :samp:`{n}` pages from each
  file, as always, stopping when there are no more pages. For example,
  if you ran

  ::

     qpdf --collate=2 --empty --pages a.pdf 1-5 b.pdf 6-4 c.pdf r1 -- out.pdf

  you would get the following pages in this order:

  - a.pdf page 1

  - a.pdf page 2

  - b.pdf page 6

  - b.pdf page 5

  - c.pdf last page

  - a.pdf page 3

  - a.pdf page 4

  - b.pdf page 4

  - a.pdf page 5

- Take pages 1 through 5 from :file:`file1.pdf` and pages 11 through
  15 in reverse from :file:`file2.pdf`, taking document-level metadata
  from :file:`file2.pdf`.

  ::

     qpdf file2.pdf --pages file1.pdf 1-5 . 15-11 -- outfile.pdf

- Here's a more contrived example. If, for some reason, you wanted to
  take the first page of an encrypted file called
  :file:`encrypted.pdf` with password ``pass`` and repeat it twice in
  an output file without any shared data between the two copies of
  page 1, and if you wanted to drop document-level metadata but
  preserve encryption, you could run

  ::

     qpdf --empty --copy-encryption=encrypted.pdf \
          --encryption-file-password=pass \
          --pages encrypted.pdf --password=pass 1 \
                ./encrypted.pdf --password=pass 1 -- \
          outfile.pdf

  Note that we had to specify the password all three times because
  giving a password as :qpdf:ref:`--encryption-file-password` doesn't
  count for page selection, and as far as qpdf is concerned,
  :file:`encrypted.pdf` and :file:`./encrypted.pdf` are separate
  files. (This is by design. See :ref:`page-limitations` for a
  discussion.) These are all corner cases that most users should
  hopefully never have to be bothered with.

.. _page-limitations:

Limitations
~~~~~~~~~~~

With the exception of page labels (page numbers), :command:`qpdf`
doesn't yet have full support for handling document-level data as it
relates to pages. Certain document-level features such as form fields,
outlines (bookmarks), and article tags among others, are copied in
their entirety from the primary input file. Starting with qpdf version
8.3, page labels are preserved from all files unless
:qpdf:ref:`--remove-page-labels` is specified.

.. If updating this after limitations are removed or reduced,
   recheck --split-pages as well.

It is expected that a future version of :command:`qpdf` will have more
complete and configurable behavior regarding document-level metadata.
In the meantime, semantics of splitting and merging vary across
features. For example, the document's outlines (bookmarks) point to
actual page objects, so if you select some pages and not others,
bookmarks that point to pages that are in the output file will work,
and remaining bookmarks will not work. If you don't want to preserve
the primary file's metadata, use :qpdf:ref:`--empty` as the primary
input file.

Visit `qpdf issues labeled with "pages"
<https://github.com/qpdf/qpdf/issues?q=is%3Aopen+is%3Aissue+label%3Apages>`__
or look at the :file:`TODO` file in the qpdf source distribution for
some of the ideas.

.. NOTE:

   The workaround described in the following paragraph is mentioned in
   the documentation in more than one place. Searching for ./ should
   help find them. It is also in the test suite. I believe there are
   several valid uses cases for doing this, and so it is my intention
   to leave the behavior of treating different paths to the same file
   as separate even if the above limitations are removed. See also
   https://github.com/qpdf/qpdf/issues/399

Prior to :command:`qpdf` version 8.4, it was not possible to specify
the same page from the same file directly more than once, and a
workaround of specifying the same file in more than one way was
required. Version 8.4 removes this limitation, but when the same page
is copied more than once, all its data is shared between the pages.
Sometimes this is fine, but sometimes it may not work correctly,
particularly if there are form fields or you intend to perform other
modifications on one of the pages. A future version of qpdf should
address this more completely. You can work around this by specifying
the same file in two different ways. For example :command:`qpdf
in.pdf --pages . 1 ./in.pdf 1 -- out.pdf` would create a file with two
copies of the first page of the input, and the two copies would not
share any objects in common. This includes fonts, images, and anything
else the page references.

.. _overlay-underlay:

Overlay and Underlay
--------------------

.. help-topic overlay-underlay: overlay/underlay pages from other files

   These options allow pages from another file to be overlaid or
   underlaid on the primary output. Overlaid pages are drawn on top of
   the destination page and may obscure the page. Underlaid pages are
   drawn below the destination page. Usage:

   {--overlay|--underlay} file
         [--password=password]
         [--to=page-range]
         [--from=[page-range]]
         [--repeat=page-range]
         --

   Note the use of "--" by itself to terminate overlay/underlay options.

   For overlay and underlay, a file and optional password are specified, along
   with a series of optional page ranges. The default behavior is that each
   page of the overlay or underlay file is imposed on the corresponding page
   of the primary output until it runs out of pages, and any extra pages are
   ignored. You can also give a page range with --repeat to cause
   those pages to be repeated after the original pages are exhausted.

   Run qpdf --help=page-ranges for help with page ranges.

You can use :command:`qpdf` to overlay or underlay pages from other
files onto the output generated by qpdf. Specify overlay or underlay
as follows:

::

   {--overlay|--underlay} file [options] --

Overlay and underlay options are processed late, so they can be
combined with other options like merging and will apply to the final
output. The ``--overlay`` and ``--underlay`` options work the same
way, except underlay pages are drawn underneath the page to which they
are applied, possibly obscured by the original page, and overlay files
are drawn on top of the page to which they are applied, possibly
obscuring the page. You can combine overlay and underlay, but you can
only specify each option at most one time.

The default behavior of overlay and underlay is that pages are taken
from the overlay/underlay file in sequence and applied to
corresponding pages in the output until there are no more output
pages. If the overlay or underlay file runs out of pages, remaining
output pages are left alone. This behavior can be modified by options,
which are provided between the ``--overlay`` or ``--underlay`` flag
and the ``--`` option. The following options are supported:

.. qpdf:option:: --to=page-range

   .. help: destination pages for underlay/overlay

      Specify the range of pages in the primary output to apply
      overlay/underlay to. See qpdf --help=page-ranges for help with
      the page range syntax.

  Specify a page range (see :ref:`page-ranges`) that indicates which
  pages in the output should have the overlay/underlay applied. If not
  specified, overlay/underlay are applied to all pages.

.. qpdf:option:: --from=[page-range]

   .. help: source pages for underlay/overlay

      Specify pages from the overlay/underlay file that are applied to
      the destination pages. See qpdf --help=page-ranges for help
      with the page range syntax. The page range may be omitted
      if --repeat is used.

Specify a page range that indicates which pages in the
overlay/underlay file will be used for overlay or underlay. If not
specified, all pages will be used. The "from" pages are used until
they are exhausted, after which any pages specified with
:qpdf:ref:`--repeat` are used. If you are using the
:qpdf:ref:`--repeat` option, you can use ``--from=`` to provide an
empty set of "from" pages.

This Can be left empty by omitting
:samp:`{page-range}` 

.. qpdf:option:: --repeat=page-range

   .. help: overlay/underlay pages to repeat

      Specify pages from the overlay/underlay that are repeated after
      "from" pages have been exhausted. See qpdf --help=page-ranges
      for help with the page range syntax.

Specify an optional page range that indicates which pages in the
overlay/underlay file will be repeated after the "from" pages are used
up. If you want to apply a repeat a range of pages starting with the
first page of output, you can explicitly use ``--from=``.

Examples
~~~~~~~~

- Overlay the first three pages from file :file:`o.pdf` onto the first
  three pages of the output, then overlay page 4 from :file:`o.pdf`
  onto pages 4 and 5 of the output. Leave remaining output pages
  untouched.

  ::

     qpdf in.pdf --overlay o.pdf --to=1-5 --from=1-3 --repeat=4 -- out.pdf


- Underlay page 1 of :file:`footer.pdf` on all odd output pages, and
  underlay page 2 of :file:`footer.pdf` on all even output pages.

  ::

     qpdf in.pdf --underlay footer.pdf --from= --repeat=1,2 -- out.pdf

- Combine two files and overlay the single page from watermark.pdf on
  the result.

  ::

     qpdf --empty --pages a.pdf b.pdf -- \
          --overlay watermark.pdf --from= --repeat=1 -- out.pdf

.. _attachments:

Embedded Files/Attachments
--------------------------

.. help-topic attachments: work with embedded files

   It is possible to list, add, or delete embedded files (also known
   as attachments) and to copy attachments from other files. See help
   on individual options for details. Run qpdf --help=add-attachment
   for additional details about adding attachments. See also
   --help=--list-attachments and --help=--show-attachment.

It is possible to list, add, or delete embedded files (also known as
attachments) and to copy attachments from other files. See also
:qpdf:ref:`--list-attachments` and :qpdf:ref:`--show-attachment`.

Related Options
~~~~~~~~~~~~~~~

.. qpdf:option:: --add-attachment file [options] --

   .. help: start add attachment options

      The --add-attachment flag and its options may be repeated to add
      multiple attachments. Run qpdf --help=add-attachment for details.

   This flag starts add attachment options, which are used to add
   attachments to a file.

   The ``--add-attachment`` flag and its options may be repeated to
   add multiple attachments. Please see :ref:`add-attachment` for
   additional details.

.. qpdf:option:: --copy-attachments-from file [options] --

   .. help: start copy attachment options

      The --copy-attachments-from flag and its options may be repeated
      to copy attachments from multiple files. Run
      qpdf --help=copy-attachments for details.

   This flag starts copy attachment options, which are used to copy
   attachments from other files.

   The ``--copy-attachments-from`` flag and its options may be
   repeated to copy attachments from multiple files. Please see
   :ref:`copy-attachments` for additional details.

.. qpdf:option:: --remove-attachment=key

   .. help: remove an embedded file

      Remove an embedded file using its key. Get the key with
      --list-attachments.

   Remove the specified attachment. This doesn't only remove the
   attachment from the embedded files table but also clears out the
   file specification to ensure that the attachment is actually not
   present in the output file. That means that any potential internal
   links to the attachment will be broken. Run with :qpdf:ref:`--verbose` to
   see status of the removal. Use :qpdf:ref:`--list-attachments` to find
   the attachment key. This option may be repeated to remove multiple
   attachments.

.. _pdf-dates:

PDF Date Format
~~~~~~~~~~~~~~~

.. help-topic pdf-dates: PDF date format

   When a date is required, the date should conform to the PDF date
   format specification, which is "D:yyyymmddhhmmssz" where "z" is
   either literally upper case "Z" for UTC or a timezone offset in
   the form "-hh'mm'" or "+hh'mm'". Negative timezone offsets indicate
   time before UTC. Positive offsets indicate how far after. For
   example, US Eastern Standard Time (America/New_York) is "-05'00'",
   and Indian Standard Time (Asia/Calcutta) is "+05'30'".

   Examples:
   - D:20210207161528-05'00'   February 7, 2021 at 4:15:28 p.m.
   - D:20210207211528Z         February 7, 2021 at 21:15:28 UTC

When a date is required, the date should conform to the PDF date
format specification, which is :samp:`D:{yyyymmddhhmmssz}` where
:samp:`{z}` is either literally upper case ``Z`` for UTC or a
timezone offset in the form :samp:`{-hh'mm'}` or :samp:`{+hh'mm'}`.
Negative timezone offsets indicate time before UTC. Positive offsets
indicate how far after. For example, US Eastern Standard Time
(America/New_York) is ``-05'00'``, and Indian Standard Time
(Asia/Calcutta) is ``+05'30'``.

.. list-table:: PDF Date Examples
   :widths: 30 70
   :header-rows: 0

   - - ``D:20210207161528-05'00'``
     - February 7, 2021 at 4:15:28 p.m.

   - - ``D:20210207211528Z``
     - February 7, 2021 at 21:15:28 UTC

.. _add-attachment:

Options for Adding Attachments
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. help-topic add-attachment: attach (embed) files

   The options listed below appear between --add-attachment and its
   terminating "--".

These options are valid between :qpdf:ref:`--add-attachment` and ``--``.

.. qpdf:option:: --key=key

   .. help: specify attachment key

      Specify the key to use for the attachment in the embedded files
      table. It defaults to the last element (basename) of the
      attached file's filename.

   Specify the key to use for the attachment in the embedded files
   table. It defaults to the last element of the attached file's
   filename. For example, if you say ``--add-attachment
   /home/user/image.png``, the default key will be just ``image.png``.

.. qpdf:option:: --filename=name

   .. help: set attachment's displayed filename

      Specify the filename to be used for the attachment. This is what
      is usually displayed to the user and is the name most graphical
      PDF viewers will use when saving a file. It defaults to the last
      element (basename) of the attached file's filename.

   Specify the filename to be used for the attachment. This is what is
   usually displayed to the user and is the name most graphical PDF
   viewers will use when saving a file. It defaults to the last
   element of the attached file's filename. For example, if you say
   ``--add-attachment /home/user/image.png``, the default key will be
   just ``image.png``.

.. qpdf:option:: --creationdate=date

   .. help: set attachment's creation date

      Specify the attachment's creation date in PDF format; defaults
      to the current time. Run qpdf --help=pdf-dates for information
      about the date format.

   Specify the attachment's creation date in PDF format; defaults to
   the current time. See :ref:`pdf-dates` for information about the
   date format.

.. qpdf:option:: --moddate=date

   .. help: set attachment's modification date

      Specify the attachment's modification date in PDF format;
      defaults to the current time. Run qpdf --help=pdf-dates for
      information about the date format.

   Specify the attachment's modification date in PDF format; defaults
   to the current time. See :ref:`pdf-dates` for information about the
   date format.

.. qpdf:option:: --mimetype=type/subtype

   .. help: attachment mime type, e.g. application/pdf

      Specify the mime type for the attachment, such as text/plain,
      application/pdf, image/png, etc.

   Specify the mime type for the attachment, such as ``text/plain``,
   ``application/pdf``, ``image/png``, etc. The qpdf library does not
   automatically determine the mime type. In a UNIX-like environment,
   the :command:`file` command can often provide this information. In
   MacOS, you can use :samp:`file -I {filename}`. In Linux, it's
   :samp:`file -i {filename}`.

   Implementation note: the mime type appears in a field called
   ``/Subtype`` in the PDF file, but that field actually includes the
   full type and subtype of the mime type. This is because ``/Type``
   already means something else in PDF.

.. qpdf:option:: --description="text"

   .. help: set attachment's description

      Supply descriptive text for the attachment, displayed by some
      PDF viewers.

   Supply descriptive text for the attachment, displayed by some PDF
   viewers.

.. qpdf:option:: --replace

   .. help: replace attachment with same key

      Indicate that any existing attachment with the same key should
      be replaced by the new attachment. Otherwise, qpdf gives an
      error if an attachment with that key is already present.

   Indicate that any existing attachment with the same key should be
   replaced by the new attachment. Otherwise, :command:`qpdf` gives an
   error if an attachment with that key is already present.

.. _copy-attachments:

Options for Copying Attachments
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. help-topic copy-attachments: copy attachments from another file

   The options listed below appear between --copy-attachments-from and
   its terminating "--".

   To copy attachments from a password-protected file, use
   the --password option after the file name.

Options in this section are valid between
:qpdf:ref:`--copy-attachments-from` and ``--``.

.. qpdf:option:: --prefix=prefix

   .. help: key prefix for copying attachments

      Prepend a prefix to each key; may be needed if there are
      duplicate attachment keys. This affects the key only, not the
      file name.

   Only required if the file from which attachments are being copied
   has attachments with keys that conflict with attachments already
   in the file. In this case, the specified prefix will be prepended
   to each key. This affects only the key in the embedded files
   table, not the file name. The PDF specification doesn't preclude
   multiple attachments having the same file name.

.. _inspection-options:

PDF Inspection
--------------

.. help-topic inspection: inspect PDF files

   These options provide tools for inspecting PDF files. When any of
   the options in this section are specified, no output file may be
   given.

These options provide tools for inspecting PDF files. When any of the
options in this section are specified, no output file may be given.

Related Options
~~~~~~~~~~~~~~~

.. qpdf:option:: --is-encrypted

   .. help: silently test whether a file is encrypted

      Silently exit with a code indicating the file's encryption status:

      0: the file is encrypted
      1: not used
      2: the file is not encrypted

      This can be used with password-protected files even if you don't
      know the password.

   Silently exit with a code indicating the file's encryption status:

   .. list-table:: Exit Codes for ``--is-encrypted``
      :widths: 5 80
      :header-rows: 0

      - - ``0``
        - the file is encrypted

      - - ``1``
        - not used

      - - ``2``
        - the file is not encrypted

   This option can be used for password-protected files even if you
   don't know the password.

   This option is useful for shell scripts. Other options are ignored
   if this is given. This option is mutually exclusive with
   :qpdf:ref:`--requires-password`. Both this option and
   :qpdf:ref:`--requires-password` exit with status ``2`` for
   non-encrypted files.

.. qpdf:option:: --requires-password

   .. help: silently test a file's password

      Silently exit with a code indicating the file's password status:

      0: a password, other than as supplied, is required
      1: not used
      2: the file is not encrypted
      3: the file is encrypted, and correct password (if any) has been supplied

   Silently exit with a code indicating the file's password status:

   .. list-table:: Exit Codes for ``--requires-password``
      :widths: 5 80
      :header-rows: 0

      - - ``0``
        - a password, other than as supplied, is required

      - - ``1``
        - not used

      - - ``2``
        - the file is not encrypted

      - - ``3``
        - the file is encrypted, and correct password (if any) has
          been supplied

   Use with the :qpdf:ref:`--password` option to specify the password
   to test.

   The choice of exit status ``0`` to mean that a password is required
   is to enable code like

   .. code-block:: bash

      if [ qpdf --requires-password file.pdf ]; then
          # prompt for password
      fi

   If a password is supplied with :qpdf:ref:`--password`, that
   password is used to open the file just as with any normal
   invocation of :command:`qpdf`. That means that using this option
   with :qpdf:ref:`--password` can be used to check the correctness of
   the password. In that case, an exit status of ``3`` means the file
   works with the supplied password. This option is mutually exclusive
   with :qpdf:ref:`--is-encrypted`. Both this option and
   :qpdf:ref:`--is-encrypted` exit with status ``2`` for non-encrypted
   files.

.. qpdf:option:: --check

   .. help: partially check whether PDF is valid

      Check the structure of the PDF file as well as a number of other
      aspects of the file, and write information about the file to
      standard output. Note that qpdf does not perform any validation
      of the actual PDF page content or semantic correctness of the
      PDF file. It merely checks that the PDF file is syntactically
      valid. See also qpdf --help=exit-status.

   Check the file's structure as well as encryption, linearization,
   and encoding of stream data, and write information about the file
   to standard output. An exit status of ``0`` indicates syntactic
   correctness of the PDF file. Note that :samp:`--check` writes
   nothing to standard error when everything is valid, so if you are
   using this to programmatically validate files in bulk, it is safe
   to run without output redirected to :file:`/dev/null` and just
   check for a ``0`` exit code.

   A file for which :samp:`--check` reports no errors may still have
   errors in stream data content or may contain constructs that don't
   conform to the PDF specification, but it should be syntactically
   valid. If :samp:`--check` reports any errors, qpdf will exit with a
   status of ``2``. There are some recoverable conditions that
   :samp:`--check` detects. These are issued as warnings instead of
   errors. If qpdf finds no errors but finds warnings, it will exit
   with a status of ``3``. When :samp:`--check` is combined with other
   options, checks are always performed before any other options are
   processed. For erroneous files, :samp:`--check` will cause qpdf to
   attempt to recover, after which other options are effectively
   operating on the recovered file. Combining :samp:`--check` with
   other options in this way can be useful for manually recovering
   severely damaged files.

   See also :ref:`exit-status`.

.. qpdf:option:: --show-encryption

   .. help: information about encrypted files

      Show document encryption parameters. Also show the document's
      user password if the owner password is given and the file was
      encrypted using older encryption formats that allow user
      password recovery.

   This option shows document encryption parameters. It also shows the
   document's user password if the owner password is given and the
   file was encrypted using older encryption formats that allow user
   password recovery. (See :ref:`pdf-encryption` for a technical
   discussion of this feature.) The output of ``--show-encryption`` is
   included in the output of :qpdf:ref:`--check`.

.. qpdf:option:: --show-encryption-key

   .. help: show key with --show-encryption

      When used with --show-encryption or --check, causes the
      underlying encryption key to be displayed.

   When encryption information is being displayed, as when
   :qpdf:ref:`--check` or :qpdf:ref:`--show-encryption` is given, display the
   computed or retrieved encryption key as a hexadecimal string. This
   value is not ordinarily useful to users, but it can be used as the
   parameter to :qpdf:ref:`--password` if the :qpdf:ref:`--password-is-hex-key`
   is specified. Note that, when PDF files are encrypted, passwords
   and other metadata are used only to compute an encryption key, and
   the encryption key is what is actually used for encryption. This
   enables retrieval of that key. See :ref:`pdf-encryption` for a
   technical discussion.

.. qpdf:option:: --check-linearization

   .. help: check linearization tables

      Check to see whether a file is linearized and, if so, whether
      the linearization hint tables are correct.

   Check to see whether a file is linearized and, if so, whether the
   linearization hint tables are correct. qpdf does not check all
   aspects of linearization. A linearized PDF file with linearization
   errors that is otherwise correct is almost always readable by a PDF
   viewer. As such, "errors" in PDF linearization are treated by
   :command:`qpdf` as warnings.

.. qpdf:option:: --show-linearization

   .. help: show linearization hint tables

      Check and display all data in the linearization hint tables.

   Check and display all data in the linearization hint tables.

.. qpdf:option:: --show-xref

   .. help: show cross reference data

      Show the contents of the cross-reference table or stream (object
      locations in the file) in a human-readable form. This is
      especially useful for files with cross-reference streams, which
      are stored in a binary format.

   Show the contents of the cross-reference table or stream in a
   human-readable form. The cross-reference data gives the offset of
   regular objects and the object stream ID and 0-based index within
   the object stream for compressed objects. This is especially useful
   for files with cross-reference streams, which are stored in a
   binary format. If the file is invalid and cross reference table
   reconstruction is performed, this option will show the information
   in the reconstructed table.

.. qpdf:option:: --show-object={trailer|obj[,gen]}

   .. help: show contents of an object

      Show the contents of the given object. This is especially useful
      for inspecting objects that are inside of object streams (also
      known as "compressed objects").

   Show the contents of the given object. This is especially useful
   for inspecting objects that are inside of object streams (also
   known as "compressed objects").

.. qpdf:option:: --raw-stream-data

   .. help: show raw stream data

      When used with --show-object, if the object is a stream, write
      the raw (compressed) binary stream data to standard output
      instead of the object's contents. See also
      --filtered-stream-data.

   When used with :qpdf:ref:`--show-object`, if the object is a
   stream, write the raw (compressed) binary stream data to standard
   output instead of the object's contents. Avoid combining this with
   other inspection options to avoid commingling the stream data with
   other output. See also :qpdf:ref:`--filtered-stream-data`.

.. qpdf:option:: --filtered-stream-data

   .. help: show filtered stream data

      When used with --show-object, if the object is a stream, write
      the filtered (uncompressed, potentially binary) stream data to
      standard output instead of the object's contents. See also
      --raw-stream-data.

   When used with :qpdf:ref:`--show-object`, if the object is a stream,
   write the filtered (uncompressed, potentially binary) stream data
   to standard output instead of the object's contents. If the stream
   is filtered using filters that qpdf does not support, an error will
   be issued. This option acts as if ``--decode-level=all`` was
   specified (see :qpdf:ref:`--decode-level`), so it will uncompress
   images compressed with supported lossy compression schemes. Avoid
   combining this with other inspection options to avoid commingling
   the stream data with other output.

   This option may be combined with :qpdf:ref:`--normalize-content`.
   If you do this, qpdf will attempt to run content normalization even
   if the stream is not a content stream, which will probably produce
   unusable results.

   See also :qpdf:ref:`--raw-stream-data`.

.. qpdf:option:: --show-npages

   .. help: show number of pages

      Print the number of pages in the input file on a line by itself.
      Useful for scripts.

   Print the number of pages in the input file on a line by itself.
   Since the number of pages appears by itself on a line, this option
   can be useful for scripting if you need to know the number of pages
   in a file.

.. qpdf:option:: --show-pages

   .. help: display page dictionary information

      Show the object and generation number for each page dictionary
      object and for each content stream associated with the page.

   Show the object and generation number for each page dictionary
   object and for each content stream associated with the page. Having
   this information makes it more convenient to inspect objects from a
   particular page. See also :qpdf:ref:`--with-images`.

.. qpdf:option:: --with-images

   .. help: include image details with --show-pages

      When used with --show-pages, also shows the object and
      generation numbers for the image objects on each page.

   When used with :qpdf:ref:`--show-pages`, also shows the object and
   generation numbers for the image objects on each page.

.. qpdf:option:: --list-attachments

   .. help: list embedded files

      Show the key and stream number for each embedded file. Combine
      with --verbose for more detailed information.

   Show the *key* and stream number for each embedded file. With
   :qpdf:ref:`--verbose`, additional information, including preferred
   file name, description, dates, and more are also displayed. The key
   is usually but not always equal to the file name and is needed by
   some of the other options. See also :ref:`attachments`. Note that
   this option displays dates in PDF timestamp syntax. When attachment
   information is included in json output in the ``"attachments"`` key
   (see :qpdf:ref:`--json`), dates are shown (just within that object)
   in ISO-8601 format.

.. qpdf:option:: --show-attachment=key

   .. help: export an embedded file

      Write the contents of the specified attachment to standard
      output as binary data. Get the key with --list-attachments.

   Write the contents of the specified attachment to standard output
   as binary data. The key should match one of the keys shown by
   :qpdf:ref:`--list-attachments`. If this option is given more than
   once, only the last attachment will be shown. See also
   :ref:`attachments`.

.. _json-options:

JSON Options
------------

.. help-topic json: JSON output for PDF information

   Show information about the PDF file in JSON format. Please see the
   JSON chapter in the qpdf manual for details.

It is possible to view information about PDF files in a JSON format.
See :ref:`json` for details about the qpdf JSON format.

Related Options
~~~~~~~~~~~~~~~

.. qpdf:option:: --json[=version]

   .. help: show file in JSON format

      Generate a JSON representation of the file. This is described in
      depth in the JSON section of the manual. "version" may be a
      specific version or "latest" (the default). Run qpdf --json-help
      for a description of the generated JSON object.

   Generate a JSON representation of the file. This is described in
   depth in :ref:`json`. The version parameter can be used to specify
   which version of the qpdf JSON format should be output. The version
   number be a number or ``latest``. The default is ``latest``. As of
   qpdf 11, the latest version is ``2``. If you have code that reads
   qpdf JSON output, you can tell what version of the JSON output you
   have from the ``"version"`` key in the output. Use the
   :qpdf:ref:`--json-help` option to get a description of the JSON
   object.

   Starting with qpdf 11, when this option is specified, an output
   file is optional (for backward compatibility) and defaults to
   standard output. You may specify an output file to write the JSON
   to a file rather than standard output. (Example: ``qpdf --json
   in.pdf out.json``)

   Stream data is only included if :qpdf:ref:`--json-output` is
   specified or if a value other than ``none`` is passed to
   :qpdf:ref:`--json-stream-data`.

.. qpdf:option:: --json-help[=version]

   .. help: show format of JSON output

      Describe the format of the JSON output by writing to standard
      output a JSON object with the same keys and with values
      containing descriptive text.

   Describe the format of the corresponding version of JSON output by
   writing to standard output a JSON object with the same structure as
   the JSON generated by qpdf. In the output written by
   ``--json-help``, each key's value is a description of the key. The
   specific contract guaranteed by qpdf in its JSON representation is
   explained in more detail in the :ref:`json`. The default version of
   help is version ``2``, as with the :qpdf:ref:`--json` flag.

.. qpdf:option:: --json-key=key

   .. help: limit which keys are in JSON output

      This option is repeatable. If given, only the specified
      top-level keys will be included in the JSON output. Otherwise,
      all keys will be included. With --json-output, when not given,
      only the "qpdf" key will appear in the output.

   This option is repeatable. If given, only the specified top-level
   keys will be included in the JSON output. Otherwise, all keys will
   be included. If not given, all keys will be included, unless
   :qpdf:ref:`--json-output` was specified, in which case, only the
   ``"qpdf"`` key will be included by default. If
   :qpdf:ref:`--json-output` was not given, the ``version`` and
   ``parameters`` keys will always appear in the output.

.. qpdf:option:: --json-object={trailer|obj[,gen]}

   .. help: limit which objects are in JSON

      This option is repeatable. If given, only specified objects will
      be shown in the "objects" key of the JSON output. Otherwise, all
      objects will be shown.

   This option is repeatable. If given, only specified objects will be
   shown in the objects dictionary in the JSON output. Otherwise, all
   objects will be shown. See :ref:`json` for details about the qpdf
   JSON format.

.. qpdf:option:: --json-stream-data={none|inline|file}

   .. help: how to handle streams in json output

      When used with --json, this option controls whether streams in
      json output should be omitted, written inline (base64-encoded)
      or written to a file. If "file" is chosen, the file will be the
      name of the output file appended with -nnn where nnn is the
      object number. The prefix can be overridden with
      --json-stream-prefix. The default is "none", except
      when --json-output is specified, in which case the default is
      "inline".

   When used with :qpdf:ref:`--json`, this option controls
   whether streams in JSON output should be omitted, written inline
   (base64-encoded) or written to a file. If ``file`` is chosen, the
   file will be the name of the output file appended with
   :samp:`-{nnn}` where :samp:`{nnn}` is the object number. The stream
   data file prefix can be overridden with
   :qpdf:ref:`--json-stream-prefix`. The default value is ``none``,
   except when :qpdf:ref:`--json-output` is specified, in which case
   the default is ``inline``.

.. qpdf:option:: --json-stream-prefix=file-prefix

   .. help: prefix for json stream data files

      When used with --json-stream-data=file, --json-stream-data=file-prefix
      sets the prefix for stream data files, overriding the default,
      which is to use the output file name. Whatever is given here
      will be appended with -nnn to create the name of the file that
      will contain the data for the stream stream in object nnn.

   When used with ``--json-stream-data=file``,
   ``--json-stream-data=file-prefix`` sets the prefix for stream data
   files, overriding the default, which is to use the output file
   name. Whatever is given here will be appended with :samp:`-{nnn}`
   to create the name of the file that will contain the data for the
   stream stream in object :samp:`{nnn}`.

.. qpdf:option:: --json-output[=version]

   .. help: apply defaults for JSON serialization

      Implies --json=version. Changes default values for certain
      options so that the JSON output written is the most faithful
      representation of the original PDF and contains no additional
      JSON keys. See also --json-stream-data, --json-stream-prefix,
      and --decode-level.

   Implies :qpdf:ref:`--json` at the specified version. This option
   changes several default values, all of which can be overridden by
   specifying the stated option:

   - The default value for :qpdf:ref:`--json-stream-data` changes from
     ``none`` to ``inline``.

   - The default value for :qpdf:ref:`--decode-level` changes from
     ``generalized`` to ``none``.

   - By default, only the ``"qpdf"`` key is included in the JSON
     output, but you can add additional keys with
     :qpdf:ref:`--json-key`.

   - The ``"version"`` and ``"parameters"`` keys will be excluded from
     the JSON output.

   If you want to look at the contents of streams easily as you would
   in QDF mode (see :ref:`qdf`), you can use
   ``--decode-level=generalized`` and ``--json-stream-data=file`` for
   a convenient way to do that.

.. qpdf:option:: --json-input

   .. help: input file is qpdf JSON

      Treat the input file as a JSON file in qpdf JSON format. See the
      "qpdf JSON Format" section of the manual for information about
      how to use this option.

   Treat the input file as a JSON file in qpdf JSON format. The input
   file must be complete and include all stream data. The JSON version
   must be at least 2. All top-level keys are ignored except for
   ``"qpdf"``. For information about converting between PDF and JSON,
   please see :ref:`json`.

.. qpdf:option:: --update-from-json=qpdf-json-file

   .. help: update a PDF from qpdf JSON

      Update a PDF file from a JSON file. Please see the "qpdf JSON"
      chapter of the manual for information about how to use this
      option.

   This option updates a PDF file from the specified qpdf JSON file.
   For a information about how to use this option, please see
   :ref:`json`.

.. _test-options:

Options for Testing or Debugging
--------------------------------

.. help-topic testing: options for testing or debugging

   The options below are useful when writing automated test code that
   includes files created by qpdf or when testing qpdf itself.

The options below are useful when writing automated test code that
includes files created by qpdf or when testing qpdf itself. When
changes are made to qpdf, care is taken to avoid gratuitously changing
the output of PDF files. This is to make it easier to do direct
comparisons in test suites with files created by qpdf. However, there
are no guarantees that the PDF output won't change such as in the
event of a bug fix or feature enhancement to some aspect of the output
that qpdf creates.

.. _idempotency:

Idempotency
~~~~~~~~~~~

Note about idempotency of byte-for-byte content: there is no
expectation that qpdf is idempotent in the general case. In other
words, there is no expectation that, when qpdf is run on its own
output, it will create *byte-for-byte* identical output, even though
it will create semantically identical files. There are a variety of
reasons for this including document ID generation, which includes a
random element, as well as the interaction of stream length encoding
with dictionary key sorting.

It is possible to get idempotent behavior by using the
:qpdf:ref:`--static-id` or :qpdf:ref:`--deterministic-id` option with
qpdf and running it *three* times so that you are processing the
output of qpdf on its own previous output. For example, in this
sequence of commands:

::

   qpdf any-file.pdf 1.pdf
   qpdf --static-id 1.pdf 2.pdf
   qpdf --static-id 2.pdf 3.pdf

the files :file:`2.pdf` and :file:`3.pdf` should be *byte-for-byte*
identical. The qpdf test suite relies on this behavior. See also
:qpdf:ref:`--static-aes-iv`.

Related Options
~~~~~~~~~~~~~~~

.. qpdf:option:: --static-id

   .. help: use a fixed document ID

      Use a fixed value for the document ID. This is intended for
      testing only. Never use it for production files. See also
      qpdf --help=--deterministic-id.

   Use a fixed value for the document ID (``/ID`` in the trailer).
   **This is intended for testing only. Never use it for production
   files.** If you are trying to get the same ID each time for a given
   file and you are not generating encrypted files, consider using the
   :qpdf:ref:`--deterministic-id` option.

.. qpdf:option:: --static-aes-iv

   .. help: use a fixed AES vector

      Use a static initialization vector for AES-CBC. This is intended
      for testing only so that output files can be reproducible. Never
      use it for production files. This option is not secure since it
      significantly weakens the encryption.

   Use a static initialization vector for AES-CBC. This is intended
   for testing only so that output files can be reproducible. Never
   use it for production files. **This option in particular is not
   secure since it significantly weakens the encryption.** When
   combined with :qpdf:ref:`--static-id` and using the three-step
   process described in :ref:`idempotency`, it is possible to create
   byte-for-byte idempotent output with PDF files that use 256-bit
   encryption to assist with creating reproducible test suites.

.. qpdf:option:: --linearize-pass1=file

   .. help: save pass 1 of linearization

      Write the first pass of linearization to the named file. The
      resulting file is not a valid PDF file. This option is useful only
      for debugging qpdf.

   Write the first pass of linearization to the named file. *The
   resulting file is not a valid PDF file.* This option is useful only
   for debugging ``QPDFWriter``'s linearization code. When qpdf
   linearizes files, it writes the file in two passes, using the first
   pass to calculate sizes and offsets that are required for hint
   tables and the linearization dictionary. Ordinarily, the first pass
   is discarded. This option enables it to be captured, allowing
   inspection of the file before values calculated in pass 1 are
   inserted into the file for pass 2.

.. qpdf:option:: --test-json-schema

   .. help: test generated json against schema

      This is used by qpdf's test suite to check consistency between
      the output of qpdf --json and the output of qpdf --json-help.

   This is used by qpdf's test suite to check consistency between the
   output of ``qpdf --json`` and the output of ``qpdf --json-help``.
   This option causes an extra copy of the generated JSON to appear in
   memory and is therefore unsuitable for use with large files. This
   is why it's also not on by default.

.. qpdf:option:: --report-memory-usage

   .. help: best effort report of memory usage

      This is used by qpdf's performance test suite to report the
      maximum amount of memory used in supported environments.

   This is used by qpdf's performance test suite to report the maximum
   amount of memory used in supported environments.

.. _unicode-passwords:

Unicode Passwords
-----------------

At the library API level, all methods that perform encryption and
decryption interpret passwords as strings of bytes. It is up to the
caller to ensure that they are appropriately encoded. Starting with qpdf
version 8.4.0, qpdf will attempt to make this easier for you when
interacting with qpdf via its command line interface. The PDF specification
requires passwords used to encrypt files with 40-bit or 128-bit
encryption to be encoded with PDF Doc encoding. This encoding is a
single-byte encoding that supports ISO-Latin-1 and a handful of other
commonly used characters. It has a large overlap with Windows ANSI but
is not exactly the same. There is generally no way to provide PDF Doc
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

The :qpdf:ref:`--password-mode` option, as described earlier
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
:qpdf:ref:`--suppress-password-recovery` option. One reason
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

Please note that the :qpdf:ref:`--password-is-hex-key` option is
unrelated to all this. That flag bypasses the normal process of going
from password to encryption key entirely, allowing the raw
encryption key to be specified directly. That behavior is useful for
forensic purposes or for brute-force recovery of files with unknown
passwords and has nothing to do with the document's actual passwords.
