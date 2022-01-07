#include <qpdf/QPDFJob.hh>

#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <cstdio>
#include <ctype.h>
#include <memory>

#include <qpdf/QUtil.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QPDFCryptoProvider.hh>
#include <qpdf/QPDFArgParser.hh>
#include <qpdf/QPDFJob.hh>
#include <qpdf/QIntC.hh>

namespace
{
    class ArgParser
    {
      public:
        ArgParser(QPDFArgParser& ap, QPDFJob& o);
        void parseOptions();

      private:
#       include <qpdf/auto_job_decl.hh>

        void usage(std::string const& message);
        void initOptionTables();
        void doFinalChecks();
        void parseUnderOverlayOptions(QPDFJob::UnderOverlay*);
        void parseRotationParameter(std::string const&);
        std::vector<int> parseNumrange(char const* range, int max,
                                       bool throw_error = false);

        QPDFArgParser ap;
        QPDFJob& o;
        std::vector<char*> accumulated_args; // points to member in ap
        char* pages_password;
    };
}

ArgParser::ArgParser(QPDFArgParser& ap, QPDFJob& o) :
    ap(ap),
    o(o),
    pages_password(nullptr)
{
    initOptionTables();
}

void
ArgParser::initOptionTables()
{

#   include <qpdf/auto_job_init.hh>
    this->ap.addFinalCheck(
        QPDFArgParser::bindBare(&ArgParser::doFinalChecks, this));
}

void
ArgParser::argPositional(char* arg)
{
    if (o.infilename == 0)
    {
        o.infilename = arg;
    }
    else if (o.outfilename == 0)
    {
        o.outfilename = arg;
    }
    else
    {
        usage(std::string("unknown argument ") + arg);
    }
}

void
ArgParser::argVersion()
{
    auto whoami = this->ap.getProgname();
    std::cout
        << whoami << " version " << QPDF::QPDFVersion() << std::endl
        << "Run " << whoami << " --copyright to see copyright and license information."
        << std::endl;
}

void
ArgParser::argCopyright()
{
    // Make sure the output looks right on an 80-column display.
    //               1         2         3         4         5         6         7         8
    //      12345678901234567890123456789012345678901234567890123456789012345678901234567890
    std::cout
        << this->ap.getProgname()
        << " version " << QPDF::QPDFVersion() << std::endl
        << std::endl
        << "Copyright (c) 2005-2021 Jay Berkenbilt"
        << std::endl
        << "QPDF is licensed under the Apache License, Version 2.0 (the \"License\");"
        << std::endl
        << "you may not use this file except in compliance with the License."
        << std::endl
        << "You may obtain a copy of the License at"
        << std::endl
        << std::endl
        << "  http://www.apache.org/licenses/LICENSE-2.0"
        << std::endl
        << std::endl
        << "Unless required by applicable law or agreed to in writing, software"
        << std::endl
        << "distributed under the License is distributed on an \"AS IS\" BASIS,"
        << std::endl
        << "WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied."
        << std::endl
        << "See the License for the specific language governing permissions and"
        << std::endl
        << "limitations under the License."
        << std::endl
        << std::endl
        << "Versions of qpdf prior to version 7 were released under the terms"
        << std::endl
        << "of version 2.0 of the Artistic License. At your option, you may"
        << std::endl
        << "continue to consider qpdf to be licensed under those terms. Please"
        << std::endl
        << "see the manual for additional information."
        << std::endl;
}

#if 0
void
ArgParser::argHelp()
{
    // QXXXQ
    std::cout
        // 12345678901234567890123456789012345678901234567890123456789012345678901234567890
        << "Usage: qpdf [options] {infile | --empty} [page_selection_options] outfile\n"
        << "\n"
        << "An option summary appears below.  Please see the documentation for details.\n"
        << "\n"
        << "If @filename appears anywhere in the command-line, each line of filename\n"
        << "will be interpreted as an argument. No interpolation is done. Line\n"
        << "terminators are stripped, but leading and trailing whitespace is\n"
        << "intentionally preserved. @- can be specified to read from standard input.\n"
        << "\n"
        << "The output file can be - to indicate writing to standard output, or it can\n"
        << "be --replace-input to cause qpdf to replace the input file with the output.\n"
        << "\n"
        << "Note that when contradictory options are provided, whichever options are\n"
        << "provided last take precedence.\n"
        << "\n"
        << "\n"
        << "Basic Options\n"
        << "-------------\n"
        << "\n"
        << "--version               show version of qpdf\n"
        << "--copyright             show qpdf's copyright and license information\n"
        << "--help                  show command-line argument help\n"
        << "--show-crypto           show supported crypto providers; default is first\n"
        << "--completion-bash       output a bash complete command you can eval\n"
        << "--completion-zsh        output a zsh complete command you can eval\n"
        << "--password=password     specify a password for accessing encrypted files\n"
        << "--password-file=file    get the password the first line \"file\"; use \"-\"\n"
        << "                        to read the password from stdin (without prompt or\n"
        << "                        disabling echo, so use with caution)\n"
        << "--is-encrypted          silently exit 0 if the file is encrypted or 2\n"
        << "                        if not; useful for shell scripts\n"
        << "--requires-password     silently exit 0 if a password (other than as\n"
        << "                        supplied) is required, 2 if the file is not\n"
        << "                        encrypted, or 3 if the file is encrypted\n"
        << "                        but requires no password or the supplied password\n"
        << "                        is correct; useful for shell scripts\n"
        << "--verbose               provide additional informational output\n"
        << "--progress              give progress indicators while writing output\n"
        << "--no-warn               suppress warnings\n"
        << "--warning-exit-0        exit with code 0 instead of 3 if there are warnings\n"
        << "--linearize             generated a linearized (web optimized) file\n"
        << "--replace-input         use in place of specifying an output file; qpdf will\n"
        << "                        replace the input file with the output\n"
        << "--copy-encryption=file  copy encryption parameters from specified file\n"
        << "--encryption-file-password=password\n"
        << "                        password used to open the file from which encryption\n"
        << "                        parameters are being copied\n"
        << "--allow-weak-crypto     allow creation of files using weak cryptographic\n"
        << "                        algorithms\n"
        << "--encrypt options --    generate an encrypted file\n"
        << "--decrypt               remove any encryption on the file\n"
        << "--password-is-hex-key   treat primary password option as a hex-encoded key\n"
        << "--suppress-password-recovery\n"
        << "                        do not attempt recovering from password string\n"
        << "                        encoding errors\n"
        << "--password-mode=mode    control qpdf's encoding of passwords\n"
        << "--pages options --      select specific pages from one or more files\n"
        << "--collate=n             causes files specified in --pages to be collated\n"
        << "                        in groups of n pages (default 1) rather than\n"
        << "                        concatenated\n"
        << "--flatten-rotation      move page rotation from /Rotate key to content\n"
        << "--rotate=[+|-]angle[:page-range]\n"
        << "                        rotate each specified page 0, 90, 180, or 270\n"
        << "                        degrees; rotate all pages if no page range is given\n"
        << "--split-pages=[n]       write each output page to a separate file\n"
        << "--overlay options --    overlay pages from another file\n"
        << "--underlay options --   underlay pages from another file\n"
        << "\n"
        << "Note that you can use the @filename or @- syntax for any argument at any\n"
        << "point in the command. This provides a good way to specify a password without\n"
        << "having to explicitly put it on the command line. @filename or @- must be a\n"
        << "word by itself. Syntax such as --arg=@filename doesn't work.\n"
        << "\n"
        << "If none of --copy-encryption, --encrypt or --decrypt are given, qpdf will\n"
        << "preserve any encryption data associated with a file.\n"
        << "\n"
        << "Note that when copying encryption parameters from another file, all\n"
        << "parameters will be copied, including both user and owner passwords, even\n"
        << "if the user password is used to open the other file.  This works even if\n"
        << "the owner password is not known.\n"
        << "\n"
        << "The --password-is-hex-key option overrides the normal computation of\n"
        << "encryption keys. It only applies to the password used to open the main\n"
        << "file. This option is not ordinarily useful but can be helpful for forensic\n"
        << "or investigatory purposes. See manual for further discussion.\n"
        << "\n"
        << "The --rotate flag can be used to specify pages to rotate pages either\n"
        << "0, 90, 180, or 270 degrees. The page range is specified in the same\n"
        << "format as with the --pages option, described below. Repeat the option\n"
        << "to rotate multiple groups of pages. If the angle is preceded by + or -,\n"
        << "it is added to or subtracted from the original rotation. Otherwise, the\n"
        << "rotation angle is set explicitly to the given value. You almost always\n"
        << "want to use + or - unless you are certain about the internals of the PDF\n"
        << "you are working with.\n"
        << "\n"
        << "If --split-pages is specified, each page is written to a separate output\n"
        << "file. File names are generated as follows:\n"
        << "* If the string %d appears in the output file name, it is replaced with a\n"
        << "  zero-padded page range starting from 1\n"
        << "* Otherwise, if the output file name ends in .pdf (case insensitive), a\n"
        << "  zero-padded page range, preceded by a dash, is inserted before the file\n"
        << "  extension\n"
        << "* Otherwise, the file name is appended with a zero-padded page range\n"
        << "  preceded by a dash.\n"
        << "Page ranges are single page numbers for single-page groups or first-last\n"
        << "for multipage groups.\n"
        << "\n"
        << "\n"
        << "Encryption Options\n"
        << "------------------\n"
        << "\n"
        << "  --encrypt user-password owner-password key-length flags --\n"
        << "\n"
        << "Note that -- terminates parsing of encryption flags.\n"
        << "\n"
        << "Either or both of the user password and the owner password may be\n"
        << "empty strings.\n"
        << "\n"
        << "key-length may be 40, 128, or 256\n"
        << "\n"
        << "Additional flags are dependent upon key length.\n"
        << "\n"
        << "  If 40:\n"
        << "\n"
        << "    --print=[yn]             allow printing\n"
        << "    --modify=[yn]            allow document modification\n"
        << "    --extract=[yn]           allow text/graphic extraction\n"
        << "    --annotate=[yn]          allow comments and form fill-in and signing\n"
        << "\n"
        << "  If 128:\n"
        << "\n"
        << "    --accessibility=[yn]     allow accessibility to visually impaired\n"
        << "    --extract=[yn]           allow other text/graphic extraction\n"
        << "    --print=print-opt        control printing access\n"
        << "    --assemble=[yn]          allow document assembly\n"
        << "    --annotate=[yn]          allow commenting/filling form fields\n"
        << "    --form=[yn]              allow filling form fields\n"
        << "    --modify-other=[yn]      allow other modifications\n"
        << "    --modify=modify-opt      control modify access (old way)\n"
        << "    --cleartext-metadata     prevents encryption of metadata\n"
        << "    --use-aes=[yn]           indicates whether to use AES encryption\n"
        << "    --force-V4               forces use of V=4 encryption handler\n"
        << "\n"
        << "  If 256, options are the same as 128 with these exceptions:\n"
        << "    --force-V4               this option is not available with 256-bit keys\n"
        << "    --use-aes                this option is always on with 256-bit keys\n"
        << "    --force-R5               forces use of deprecated R=5 encryption\n"
        << "    --allow-insecure         allow the owner password to be empty when the\n"
        << "                             user password is not empty\n"
        << "\n"
        << "    print-opt may be:\n"
        << "\n"
        << "      full                  allow full printing\n"
        << "      low                   allow only low-resolution printing\n"
        << "      none                  disallow printing\n"
        << "\n"
        << "    modify-opt may be:\n"
        << "\n"
        << "      all                   allow full document modification\n"
        << "      annotate              allow comment authoring and form operations\n"
        << "      form                  allow form field fill-in and signing\n"
        << "      assembly              allow document assembly only\n"
        << "      none                  allow no modifications\n"
        << "\n"
        << "The default for each permission option is to be fully permissive. Please\n"
        << "refer to the manual for more details on the modify options.\n"
        << "\n"
        << "Specifying cleartext-metadata forces the PDF version to at least 1.5.\n"
        << "Specifying use of AES forces the PDF version to at least 1.6.  These\n"
        << "options are both off by default.\n"
        << "\n"
        << "The --force-V4 flag forces the V=4 encryption handler introduced in PDF 1.5\n"
        << "to be used even if not otherwise needed.  This option is primarily useful\n"
        << "for testing qpdf and has no other practical use.\n"
        << "\n"
        << "A warning will be issued if you attempt to encrypt a file with a format that\n"
        << "uses a weak cryptographic algorithm such as RC4. To suppress the warning,\n"
        << "specify the option --allow-weak-crypto. This option is outside of encryption\n"
        << "options (e.g. --allow-week-crypto --encrypt u o 128 --)\n"
        << "\n"
        << "\n"
        << "Password Modes\n"
        << "--------------\n"
        << "\n"
        << "The --password-mode controls how qpdf interprets passwords supplied\n"
        << "on the command-line. qpdf's default behavior is correct in almost all\n"
        << "cases, but you can fine-tune with this option.\n"
        << "\n"
        << "  bytes: use the password literally as supplied\n"
        << "  hex-bytes: interpret the password as a hex-encoded byte string\n"
        << "  unicode: interpret the password as a UTF-8 encoded string\n"
        << "  auto: attempt to infer the encoding and adjust as needed\n"
        << "\n"
        << "This is a complex topic. See the manual for a complete discussion.\n"
        << "\n"
        << "\n"
        << "Page Selection Options\n"
        << "----------------------\n"
        << "\n"
        << "These options allow pages to be selected from one or more PDF files.\n"
        << "Whatever file is given as the primary input file is used as the\n"
        << "starting point, but its pages are replaced with pages as specified.\n"
        << "\n"
        << "--keep-files-open=[yn]\n"
        << "--keep-files-open-threshold=count\n"
        << "--pages file [ --password=password ] [ page-range ] ... --\n"
        << "\n"
        << "For each file that pages should be taken from, specify the file, a\n"
        << "password needed to open the file (if any), and a page range.  The\n"
        << "password needs to be given only once per file.  If any of the input\n"
        << "files are the same as the primary input file or the file used to copy\n"
        << "encryption parameters (if specified), you do not need to repeat the\n"
        << "password here.  The same file can be repeated multiple times.  The\n"
        << "filename \".\" may be used to refer to the current input file.  All\n"
        << "non-page data (info, outlines, page numbers, etc. are taken from the\n"
        << "primary input file.  To discard this, use --empty as the primary\n"
        << "input.\n"
        << "\n"
        << "By default, when more than 200 distinct files are specified, qpdf will\n"
        << "close each file when not being referenced. With 200 files or fewer, all\n"
        << "files will be kept open at the same time. This behavior can be overridden\n"
        << "by specifying --keep-files-open=[yn]. Closing and opening files can have\n"
        << "very high overhead on certain file systems, especially networked file\n"
        << "systems. The threshold of 200 can be modified with\n"
        << "--keep-files-open-threshold\n"
        << "\n"
        << "The page range is a set of numbers separated by commas, ranges of\n"
        << "numbers separated dashes, or combinations of those.  The character\n"
        << "\"z\" represents the last page.  A number preceded by an \"r\" indicates\n"
        << "to count from the end, so \"r3-r1\" would be the last three pages of the\n"
        << "document.  Pages can appear in any order.  Ranges can appear with a\n"
        << "high number followed by a low number, which causes the pages to appear in\n"
        << "reverse.  Numbers may be repeated.  A page range may be appended with :odd\n"
        << "to indicate odd pages in the selected range or :even to indicate even\n"
        << "pages.\n"
        << "\n"
        << "If the page range is omitted, the range of 1-z is assumed.  qpdf decides\n"
        << "that the page range is omitted if the range argument is either -- or a\n"
        << "valid file name and not a valid range.\n"
        << "\n"
        << "The usual behavior of --pages is to add all pages from the first file,\n"
        << "then all pages from the second file, and so on. If the --collate option\n"
        << "is specified, then pages are collated instead. In other words, qpdf takes\n"
        << "the first page from the first file, the first page from the second file,\n"
        << "and so on until it runs out of files; then it takes the second page from\n"
        << "each file, etc. When a file runs out of pages, it is skipped until all\n"
        << "specified pages are taken from all files.\n"
        << "\n"
        << "See the manual for examples and a discussion of additional subtleties.\n"
        << "\n"
        << "\n"
        << "Overlay and Underlay Options\n"
        << "----------------------------\n"
        << "\n"
        << "These options allow pages from another file to be overlaid or underlaid\n"
        << "on the primary output. Overlaid pages are drawn on top of the destination\n"
        << "page and may obscure the page. Underlaid pages are drawn below the\n"
        << "destination page.\n"
        << "\n"
        << "{--overlay | --underlay } file\n"
        "      [ --password=password ]\n"
        "      [ --to=page-range ]\n"
        "      [ --from=[page-range] ]\n"
        "      [ --repeat=page-range ]\n"
        "      --\n"
        << "\n"
        << "For overlay and underlay, a file and optional password are specified, along\n"
        << "with a series of optional page ranges. The default behavior is that each\n"
        << "page of the overlay or underlay file is imposed on the corresponding page\n"
        << "of the primary output until it runs out of pages, and any extra pages are\n"
        << "ignored. The page range options all take page ranges in the same form as\n"
        << "the --pages option. They have the following meanings:\n"
        << "\n"
        << "  --to:     the pages in the primary output to which overlay/underlay is\n"
        << "            applied\n"
        << "  --from:   the pages from the overlay/underlay file that are used\n"
        << "  --repeat: pages from the overlay/underlay that are repeated after\n"
        << "            any \"from\" pages have been exhausted\n"
        << "\n"
        << "\n"
        << "Embedded Files/Attachments Options\n"
        << "----------------------------------\n"
        << "\n"
        << "These options can be used to work with embedded files, also known as\n"
        << "attachments.\n"
        << "\n"
        << "--list-attachments        show key and stream number for embedded files;\n"
        << "                          combine with --verbose for more detailed information\n"
        << "--show-attachment=key     write the contents of the specified attachment to\n"
        << "                          standard output as binary data\n"
        << "--add-attachment file options --\n"
        << "                          add or replace an attachment\n"
        << "--remove-attachment=key   remove the specified attachment; repeatable\n"
        << "--copy-attachments-from file options --\n"
        << "                          copy attachments from another file\n"
        << "\n"
        << "The \"key\" option is the unique name under which the attachment is registered\n"
        << "within the PDF file. You can get this using the --list-attachments option. This\n"
        << "is usually the same as the filename, but it doesn't have to be.\n"
        << "\n"
        << "Options for adding attachments:\n"
        << "\n"
        << "  file                    path to the file to attach\n"
        << "  --key=key               the name of this in the embedded files table;\n"
        << "                          defaults to the last path element of file\n"
        << "  --filename=name         the file name of the attachment; this is what is\n"
        << "                          usually displayed to the user; defaults to the\n"
        << "                          last path element of file\n"
        << "  --creationdate=date     creation date in PDF format; defaults to the\n"
        << "                          current time\n"
        << "  --moddate=date          modification date in PDF format; defaults to the\n"
        << "                          current time\n"
        << "  --mimetype=type/subtype   mime type of attachment (e.g. application/pdf)\n"
        << "  --description=\"text\"    attachment description\n"
        << "  --replace               replace any existing attachment with the same key\n"
        << "\n"
        << "Options for copying attachments:\n"
        << "\n"
        << "  file                    file whose attachments should be copied\n"
        << "  --password=password     password to open the other file, if needed\n"
        << "  --prefix=prefix         a prefix to insert in front of each key;\n"
        << "                          required if needed to ensure each attachment\n"
        << "                          has a unique key\n"
        << "\n"
        << "Date format: D:yyyymmddhhmmss<z> where <z> is either Z for UTC or a timezone\n"
        << "offset in the form -hh'mm' or +hh'mm'.\n"
        << "Examples: D:20210207161528-05'00', D:20210207211528Z\n"
        << "\n"
        << "\n"
        << "Advanced Parsing Options\n"
        << "------------------------\n"
        << "\n"
        << "These options control aspects of how qpdf reads PDF files. Mostly these are\n"
        << "of use to people who are working with damaged files. There is little reason\n"
        << "to use these options unless you are trying to solve specific problems.\n"
        << "\n"
        << "--suppress-recovery       prevents qpdf from attempting to recover damaged files\n"
        << "--ignore-xref-streams     tells qpdf to ignore any cross-reference streams\n"
        << "\n"
        << "\n"
        << "Advanced Transformation Options\n"
        << "-------------------------------\n"
        << "\n"
        << "These transformation options control fine points of how qpdf creates\n"
        << "the output file.  Mostly these are of use only to people who are very\n"
        << "familiar with the PDF file format or who are PDF developers.\n"
        << "\n"
        << "--stream-data=option      controls transformation of stream data (below)\n"
        << "--compress-streams=[yn]   controls whether to compress streams on output\n"
        << "--decode-level=option     controls how to filter streams from the input\n"
        << "--recompress-flate        recompress streams already compressed with Flate\n"
        << "--compression-level=n     set zlib compression level; most effective with\n"
        << "                          --recompress-flate --object-streams=generate\n"
        << "--normalize-content=[yn]  enables or disables normalization of content streams\n"
        << "--object-streams=mode     controls handing of object streams\n"
        << "--preserve-unreferenced   preserve unreferenced objects\n"
        << "--remove-unreferenced-resources={auto,yes,no}\n"
        << "                          whether to remove unreferenced page resources\n"
        << "--preserve-unreferenced-resources\n"
        << "                          synonym for --remove-unreferenced-resources=no\n"
        << "--newline-before-endstream  always put a newline before endstream\n"
        << "--coalesce-contents       force all pages' content to be a single stream\n"
        << "--flatten-annotations=option\n"
        << "                          incorporate rendering of annotations into page\n"
        << "                          contents including those for interactive form\n"
        << "                          fields; may also want --generate-appearances\n"
        << "--generate-appearances    generate appearance streams for form fields\n"
        << "--optimize-images         compress images with DCT (JPEG) when advantageous\n"
        << "--oi-min-width=w          do not optimize images whose width is below w;\n"
        << "                          default is 128. Use 0 to mean no minimum\n"
        << "--oi-min-height=h         do not optimize images whose height is below h\n"
        << "                          default is 128. Use 0 to mean no minimum\n"
        << "--oi-min-area=a           do not optimize images whose pixel count is below a\n"
        << "                          default is 16,384. Use 0 to mean no minimum\n"
        << "--externalize-inline-images  convert inline images to regular images; by\n"
        << "                          default, images of at least 1,024 bytes are\n"
        << "                          externalized\n"
        << "--ii-min-bytes=bytes      specify minimum size of inline images to be\n"
        << "                          converted to regular images\n"
        << "--keep-inline-images      exclude inline images from image optimization\n"
        << "--remove-page-labels      remove any page labels present in the output file\n"
        << "--qdf                     turns on \"QDF mode\" (below)\n"
        << "--linearize-pass1=file    write intermediate pass of linearized file\n"
        << "                          for debugging\n"
        << "--min-version=version     sets the minimum PDF version of the output file\n"
        << "--force-version=version   forces this to be the PDF version of the output file\n"
        << "\n"
        << "Options for --flatten-annotations are all, print, or screen. If the option\n"
        << "is print, only annotations marked as print are included. If the option is\n"
        << "screen, options marked as \"no view\" are excluded. Otherwise, annotations\n"
        << "are flattened regardless of the presence of print or NoView flags. It is\n"
        << "common for PDF files to have a flag set that appearance streams need to be\n"
        << "regenerated. This happens when someone changes a form value with software\n"
        << "that does not know how to render the new value. qpdf will not flatten form\n"
        << "fields in files like this. If you get this warning, you have two choices:\n"
        << "either use qpdf's --generate-appearances flag to tell qpdf to go ahead and\n"
        << "regenerate appearances, or use some other tool to generate the appearances.\n"
        << "qpdf does a pretty good job with most forms when only ASCII and \"Windows\n"
        << "ANSI\" characters are used in form field values, but if your form fields\n"
        << "contain other characters, rich text, or are other than left justified, you\n"
        << "will get better results first saving with other software.\n"
        << "\n"
        << "Version numbers may be expressed as major.minor.extension-level, so 1.7.3\n"
        << "means PDF version 1.7 at extension level 3.\n"
        << "\n"
        << "Values for stream data options:\n"
        << "\n"
        << "  compress              recompress stream data when possible (default)\n"
        << "  preserve              leave all stream data as is\n"
        << "  uncompress            uncompress stream data when possible\n"
        << "\n"
        << "Values for object stream mode:\n"
        << "\n"
        << "  preserve                  preserve original object streams (default)\n"
        << "  disable                   don't write any object streams\n"
        << "  generate                  use object streams wherever possible\n"
        << "\n"
        << "When --compress-streams=n is specified, this overrides the default behavior\n"
        << "of qpdf, which is to attempt compress uncompressed streams. Setting\n"
        << "stream data mode to uncompress or preserve has the same effect.\n"
        << "\n"
        << "The --decode-level parameter may be set to one of the following values:\n"
        << "  none              do not decode streams\n"
        << "  generalized       decode streams compressed with generalized filters\n"
        << "                    including LZW, Flate, and the ASCII encoding filters.\n"
        << "  specialized       additionally decode streams with non-lossy specialized\n"
        << "                    filters including RunLength\n"
        << "  all               additionally decode streams with lossy filters\n"
        << "                    including DCT (JPEG)\n"
        << "\n"
        << "In qdf mode, by default, content normalization is turned on, and the\n"
        << "stream data mode is set to uncompress. QDF mode does not support\n"
        << "linearized files. The --linearize flag disables qdf mode.\n"
        << "\n"
        << "Setting the minimum PDF version of the output file may raise the version\n"
        << "but will never lower it.  Forcing the PDF version of the output file may\n"
        << "set the PDF version to a lower value than actually allowed by the file's\n"
        << "contents.  You should only do this if you have no other possible way to\n"
        << "open the file or if you know that the file definitely doesn't include\n"
        << "features not supported later versions.\n"
        << "\n"
        << "Testing, Inspection, and Debugging Options\n"
        << "------------------------------------------\n"
        << "\n"
        << "These options can be useful for digging into PDF files or for use in\n"
        << "automated test suites for software that uses the qpdf library.\n"
        << "\n"
        << "--deterministic-id        generate deterministic /ID\n"
        << "--static-id               generate static /ID: FOR TESTING ONLY!\n"
        << "--static-aes-iv           use a static initialization vector for AES-CBC\n"
        << "                          This is option is not secure!  FOR TESTING ONLY!\n"
        << "--no-original-object-ids  suppress original object ID comments in qdf mode\n"
        << "--show-encryption         quickly show encryption parameters\n"
        << "--show-encryption-key     when showing encryption, reveal the actual key\n"
        << "--check-linearization     check file integrity and linearization status\n"
        << "--show-linearization      check and show all linearization data\n"
        << "--show-xref               show the contents of the cross-reference table\n"
        << "--show-object=trailer|obj[,gen]\n"
        << "                          show the contents of the given object\n"
        << "  --raw-stream-data       show raw stream data instead of object contents\n"
        << "  --filtered-stream-data  show filtered stream data instead of object contents\n"
        << "--show-npages             print the number of pages in the file\n"
        << "--show-pages              shows the object/generation number for each page\n"
        << "  --with-images           also shows the object IDs for images on each page\n"
        << "--check                   check file structure + encryption, linearization\n"
        << "--json                    generate a json representation of the file\n"
        << "--json-help               describe the format of the json representation\n"
        << "--json-key=key            repeatable; prune json structure to include only\n"
        << "                          specified keys. If absent, all keys are shown\n"
        << "--json-object=trailer|[obj,gen]\n"
        << "                          repeatable; include only specified objects in the\n"
        << "                          \"objects\" section of the json. If absent, all\n"
        << "                          objects are shown\n"
        << "\n"
        << "The json representation generated by qpdf is designed to facilitate\n"
        << "processing of qpdf from other programming languages that have a hard\n"
        << "time calling C++ APIs. Run qpdf --json-help for details on the format.\n"
        << "The manual has more in-depth information about the json representation\n"
        << "and certain compatibility guarantees that qpdf provides.\n"
        << "\n"
        << "The --raw-stream-data and --filtered-stream-data options are ignored\n"
        << "unless --show-object is given.  Either of these options will cause the\n"
        << "stream data to be written to standard output.\n"
        << "\n"
        << "If --filtered-stream-data is given and --normalize-content=y is also\n"
        << "given, qpdf will attempt to normalize the stream data as if it is a\n"
        << "page content stream.  This attempt will be made even if it is not a\n"
        << "page content stream, in which case it will produce unusable results.\n"
        << "\n"
        << "Ordinarily, qpdf exits with a status of 0 on success or a status of 2\n"
        << "if any errors occurred.  If there were warnings but not errors, qpdf\n"
        << "exits with a status of 3. If warnings would have been issued but --no-warn\n"
        << "was given, an exit status of 3 is still used. If you want qpdf to exit\n"
        << "with status 0 when there are warnings, use the --warning-exit-0 flag.\n"
        << "When --no-warn and --warning-exit-0 are used together, the effect is for\n"
        << "qpdf to completely ignore warnings.  qpdf does not use exit status 1,\n"
        << "since that is used by the shell if it can't execute qpdf.\n";
}
#endif

void
ArgParser::argJsonHelp()
{
    // Make sure the output looks right on an 80-column display.
    //               1         2         3         4         5         6         7         8
    //      12345678901234567890123456789012345678901234567890123456789012345678901234567890
    std::cout
        << "The json block below contains the same structure with the same keys as the"
        << std::endl
        << "json generated by qpdf. In the block below, the values are descriptions of"
        << std::endl
        << "the meanings of those entries. The specific contract guaranteed by qpdf in"
        << std::endl
        << "its json representation is explained in more detail in the manual. You can"
        << std::endl
        << "specify a subset of top-level keys when you invoke qpdf, but the \"version\""
        << std::endl
        << "and \"parameters\" keys will always be present. Note that the \"encrypt\""
        << std::endl
        << "key's values will be populated for non-encrypted files. Some values will"
        << std::endl
        << "be null, and others will have values that apply to unencrypted files."
        << std::endl
        << QPDFJob::json_schema().unparse()
        << std::endl;
}

void
ArgParser::argShowCrypto()
{
    auto crypto = QPDFCryptoProvider::getRegisteredImpls();
    std::string default_crypto = QPDFCryptoProvider::getDefaultProvider();
    std::cout << default_crypto << std::endl;
    for (auto const& iter: crypto)
    {
        if (iter != default_crypto)
        {
            std::cout << iter << std::endl;
        }
    }
}

void
ArgParser::argPassword(char* parameter)
{
    o.password = parameter;
}

void
ArgParser::argPasswordFile(char* parameter)
{
    std::list<std::string> lines;
    if (strcmp(parameter, "-") == 0)
    {
        QTC::TC("qpdf", "qpdf password stdin");
        lines = QUtil::read_lines_from_file(std::cin);
    }
    else
    {
        QTC::TC("qpdf", "qpdf password file");
        lines = QUtil::read_lines_from_file(parameter);
    }
    if (lines.size() >= 1)
    {
        // Make sure the memory for this stays in scope.
        o.password_alloc = std::shared_ptr<char>(
            QUtil::copy_string(lines.front().c_str()),
            std::default_delete<char[]>());
        o.password = o.password_alloc.get();

        if (lines.size() > 1)
        {
            std::cerr << this->ap.getProgname()
                      << ": WARNING: all but the first line of"
                      << " the password file are ignored" << std::endl;
        }
    }
}

void
ArgParser::argEmpty()
{
    o.infilename = "";
}

void
ArgParser::argLinearize()
{
    o.linearize = true;
}

void
ArgParser::argEncrypt()
{
    this->accumulated_args.clear();
    if (this->ap.isCompleting() && this->ap.argsLeft() == 0)
    {
        this->ap.insertCompletion("user-password");
    }
    this->ap.selectOptionTable(O_ENCRYPTION);
}

void
ArgParser::argEncPositional(char* arg)
{
    this->accumulated_args.push_back(arg);
    size_t n_args = this->accumulated_args.size();
    if (n_args < 3)
    {
        if (this->ap.isCompleting() && (this->ap.argsLeft() == 0))
        {
            if (n_args == 1)
            {
                this->ap.insertCompletion("owner-password");
            }
            else if (n_args == 2)
            {
                this->ap.insertCompletion("40");
                this->ap.insertCompletion("128");
                this->ap.insertCompletion("256");
            }
        }
        return;
    }
    o.user_password = this->accumulated_args.at(0);
    o.owner_password = this->accumulated_args.at(1);
    std::string len_str = this->accumulated_args.at(2);
    if (len_str == "40")
    {
        o.keylen = 40;
        this->ap.selectOptionTable(O_40_BIT_ENCRYPTION);
    }
    else if (len_str == "128")
    {
        o.keylen = 128;
        this->ap.selectOptionTable(O_128_BIT_ENCRYPTION);
    }
    else if (len_str == "256")
    {
        o.keylen = 256;
        o.use_aes = true;
        this->ap.selectOptionTable(O_256_BIT_ENCRYPTION);
    }
    else
    {
        usage("encryption key length must be 40, 128, or 256");
    }
}

void
ArgParser::argDecrypt()
{
    o.decrypt = true;
    o.encrypt = false;
    o.copy_encryption = false;
}

void
ArgParser::argPasswordIsHexKey()
{
    o.password_is_hex_key = true;
}

void
ArgParser::argSuppressPasswordRecovery()
{
    o.suppress_password_recovery = true;
}

void
ArgParser::argPasswordMode(char* parameter)
{
    if (strcmp(parameter, "bytes") == 0)
    {
        o.password_mode = QPDFJob::pm_bytes;
    }
    else if (strcmp(parameter, "hex-bytes") == 0)
    {
        o.password_mode = QPDFJob::pm_hex_bytes;
    }
    else if (strcmp(parameter, "unicode") == 0)
    {
        o.password_mode = QPDFJob::pm_unicode;
    }
    else if (strcmp(parameter, "auto") == 0)
    {
        o.password_mode = QPDFJob::pm_auto;
    }
    else
    {
        usage("invalid password-mode option");
    }
}

void
ArgParser::argEnc256AllowInsecure()
{
    o.allow_insecure = true;
}

void
ArgParser::argAllowWeakCrypto()
{
    o.allow_weak_crypto = true;
}

void
ArgParser::argCopyEncryption(char* parameter)
{
    o.encryption_file = parameter;
    o.copy_encryption = true;
    o.encrypt = false;
    o.decrypt = false;
}

void
ArgParser::argEncryptionFilePassword(char* parameter)
{
    o.encryption_file_password = parameter;
}

void
ArgParser::argCollate(char* parameter)
{
    auto n = ((parameter == 0) ? 1 :
              QUtil::string_to_uint(parameter));
    o.collate = QIntC::to_size(n);
}

void
ArgParser::argPages()
{
    if (! o.page_specs.empty())
    {
        usage("the --pages may only be specified one time");
    }
    this->accumulated_args.clear();
    this->ap.selectOptionTable(O_PAGES);
}

void
ArgParser::argPagesPassword(char* parameter)
{
    if (this->pages_password != nullptr)
    {
        QTC::TC("qpdf", "qpdf duplicated pages password");
        usage("--password already specified for this file");
    }
    if (this->accumulated_args.size() != 1)
    {
        QTC::TC("qpdf", "qpdf misplaced pages password");
        usage("in --pages, --password must immediately follow a file name");
    }
    this->pages_password = parameter;
}

void
ArgParser::argPagesPositional(char* arg)
{
    if (arg == nullptr)
    {
        if (this->accumulated_args.empty())
        {
            return;
        }
    }
    else
    {
        this->accumulated_args.push_back(arg);
    }

    char const* file = this->accumulated_args.at(0);
    char const* range = nullptr;

    size_t n_args = this->accumulated_args.size();
    if (n_args >= 2)
    {
        range = this->accumulated_args.at(1);
    }

    // See if the user omitted the range entirely, in which case we
    // assume "1-z".
    char* next_file = nullptr;
    if (range == nullptr)
    {
        if (arg == nullptr)
        {
            // The filename or password was the last argument
            QTC::TC("qpdf", "qpdf pages range omitted at end",
                    this->pages_password == nullptr ? 0 : 1);
        }
        else
        {
            // We need to accumulate some more arguments
            return;
        }
    }
    else
    {
        try
        {
            parseNumrange(range, 0, true);
        }
        catch (std::runtime_error& e1)
        {
            // The range is invalid.  Let's see if it's a file.
            if (strcmp(range, ".") == 0)
            {
                // "." means the input file.
                QTC::TC("qpdf", "qpdf pages range omitted with .");
            }
            else if (QUtil::file_can_be_opened(range))
            {
                QTC::TC("qpdf", "qpdf pages range omitted in middle");
                // Yup, it's a file.
            }
            else
            {
                // Give the range error
                usage(e1.what());
            }
            next_file = const_cast<char*>(range);
            range = nullptr;
        }
    }
    if (range == nullptr)
    {
        range = "1-z";
    }
    o.page_specs.push_back(QPDFJob::PageSpec(file, this->pages_password, range));
    this->accumulated_args.clear();
    this->pages_password = nullptr;
    if (next_file != nullptr)
    {
        this->accumulated_args.push_back(next_file);
    }
}

void
ArgParser::argEndPages()
{
    argPagesPositional(nullptr);
    if (o.page_specs.empty())
    {
        usage("--pages: no page specifications given");
    }
}

void
ArgParser::argUnderlay()
{
    parseUnderOverlayOptions(&o.underlay);
}

void
ArgParser::argOverlay()
{
    parseUnderOverlayOptions(&o.overlay);
}

void
ArgParser::argRotate(char* parameter)
{
    parseRotationParameter(parameter);
}

void
ArgParser::argFlattenRotation()
{
    o.flatten_rotation = true;
}

void
ArgParser::argListAttachments()
{
    o.list_attachments = true;
    o.require_outfile = false;
}

void
ArgParser::argShowAttachment(char* parameter)
{
    o.attachment_to_show = parameter;
    o.require_outfile = false;
}

void
ArgParser::argRemoveAttachment(char* parameter)
{
    o.attachments_to_remove.push_back(parameter);
}

void
ArgParser::argAddAttachment()
{
    o.attachments_to_add.push_back(QPDFJob::AddAttachment());
    this->ap.selectOptionTable(O_ATTACHMENT);
}

void
ArgParser::argCopyAttachmentsFrom()
{
    o.attachments_to_copy.push_back(QPDFJob::CopyAttachmentFrom());
    this->ap.selectOptionTable(O_COPY_ATTACHMENT);
}

void
ArgParser::argStreamData(char* parameter)
{
    o.stream_data_set = true;
    if (strcmp(parameter, "compress") == 0)
    {
        o.stream_data_mode = qpdf_s_compress;
    }
    else if (strcmp(parameter, "preserve") == 0)
    {
        o.stream_data_mode = qpdf_s_preserve;
    }
    else if (strcmp(parameter, "uncompress") == 0)
    {
        o.stream_data_mode = qpdf_s_uncompress;
    }
    else
    {
        // If this happens, it means streamDataChoices in
        // ArgParser::initOptionTable is wrong.
        usage("invalid stream-data option");
    }
}

void
ArgParser::argCompressStreams(char* parameter)
{
    o.compress_streams_set = true;
    o.compress_streams = (strcmp(parameter, "y") == 0);
}

void
ArgParser::argRecompressFlate()
{
    o.recompress_flate_set = true;
    o.recompress_flate = true;
}

void
ArgParser::argCompressionLevel(char* parameter)
{
    o.compression_level = QUtil::string_to_int(parameter);
}

void
ArgParser::argDecodeLevel(char* parameter)
{
    o.decode_level_set = true;
    if (strcmp(parameter, "none") == 0)
    {
        o.decode_level = qpdf_dl_none;
    }
    else if (strcmp(parameter, "generalized") == 0)
    {
        o.decode_level = qpdf_dl_generalized;
    }
    else if (strcmp(parameter, "specialized") == 0)
    {
        o.decode_level = qpdf_dl_specialized;
    }
    else if (strcmp(parameter, "all") == 0)
    {
        o.decode_level = qpdf_dl_all;
    }
    else
    {
        // If this happens, it means decodeLevelChoices in
        // ArgParser::initOptionTable is wrong.
        usage("invalid option");
    }
}

void
ArgParser::argNormalizeContent(char* parameter)
{
    o.normalize_set = true;
    o.normalize = (strcmp(parameter, "y") == 0);
}

void
ArgParser::argSuppressRecovery()
{
    o.suppress_recovery = true;
}

void
ArgParser::argObjectStreams(char* parameter)
{
    o.object_stream_set = true;
    if (strcmp(parameter, "disable") == 0)
    {
        o.object_stream_mode = qpdf_o_disable;
    }
    else if (strcmp(parameter, "preserve") == 0)
    {
        o.object_stream_mode = qpdf_o_preserve;
    }
    else if (strcmp(parameter, "generate") == 0)
    {
        o.object_stream_mode = qpdf_o_generate;
    }
    else
    {
        // If this happens, it means objectStreamsChoices in
        // ArgParser::initOptionTable is wrong.
        usage("invalid object stream mode");
    }
}

void
ArgParser::argIgnoreXrefStreams()
{
    o.ignore_xref_streams = true;
}

void
ArgParser::argQdf()
{
    o.qdf_mode = true;
}

void
ArgParser::argPreserveUnreferenced()
{
    o.preserve_unreferenced_objects = true;
}

void
ArgParser::argPreserveUnreferencedResources()
{
    o.remove_unreferenced_page_resources = QPDFJob::re_no;
}

void
ArgParser::argRemoveUnreferencedResources(char* parameter)
{
    if (strcmp(parameter, "auto") == 0)
    {
        o.remove_unreferenced_page_resources = QPDFJob::re_auto;
    }
    else if (strcmp(parameter, "yes") == 0)
    {
        o.remove_unreferenced_page_resources = QPDFJob::re_yes;
    }
    else if (strcmp(parameter, "no") == 0)
    {
        o.remove_unreferenced_page_resources = QPDFJob::re_no;
    }
    else
    {
        // If this happens, it means remove_unref_choices in
        // ArgParser::initOptionTable is wrong.
        usage("invalid value for --remove-unreferenced-page-resources");
    }
}

void
ArgParser::argKeepFilesOpen(char* parameter)
{
    o.keep_files_open_set = true;
    o.keep_files_open = (strcmp(parameter, "y") == 0);
}

void
ArgParser::argKeepFilesOpenThreshold(char* parameter)
{
    o.keep_files_open_threshold = QUtil::string_to_uint(parameter);
}

void
ArgParser::argNewlineBeforeEndstream()
{
    o.newline_before_endstream = true;
}

void
ArgParser::argLinearizePass1(char* parameter)
{
    o.linearize_pass1 = parameter;
}

void
ArgParser::argCoalesceContents()
{
    o.coalesce_contents = true;
}

void
ArgParser::argFlattenAnnotations(char* parameter)
{
    o.flatten_annotations = true;
    if (strcmp(parameter, "screen") == 0)
    {
        o.flatten_annotations_forbidden |= an_no_view;
    }
    else if (strcmp(parameter, "print") == 0)
    {
        o.flatten_annotations_required |= an_print;
    }
}

void
ArgParser::argGenerateAppearances()
{
    o.generate_appearances = true;
}

void
ArgParser::argMinVersion(char* parameter)
{
    o.min_version = parameter;
}

void
ArgParser::argForceVersion(char* parameter)
{
    o.force_version = parameter;
}

void
ArgParser::argSplitPages(char* parameter)
{
    int n = ((parameter == 0) ? 1 :
             QUtil::string_to_int(parameter));
    o.split_pages = n;
}

void
ArgParser::argVerbose()
{
    o.verbose = true;
}

void
ArgParser::argProgress()
{
    o.progress = true;
}

void
ArgParser::argNoWarn()
{
    o.suppress_warnings = true;
}

void
ArgParser::argWarningExit0()
{
    o.warnings_exit_zero = true;
}

void
ArgParser::argDeterministicId()
{
    o.deterministic_id = true;
}

void
ArgParser::argStaticId()
{
    o.static_id = true;
}

void
ArgParser::argStaticAesIv()
{
    o.static_aes_iv = true;
}

void
ArgParser::argNoOriginalObjectIds()
{
    o.suppress_original_object_id = true;
}

void
ArgParser::argShowEncryption()
{
    o.show_encryption = true;
    o.require_outfile = false;
}

void
ArgParser::argShowEncryptionKey()
{
    o.show_encryption_key = true;
}

void
ArgParser::argCheckLinearization()
{
    o.check_linearization = true;
    o.require_outfile = false;
}

void
ArgParser::argShowLinearization()
{
    o.show_linearization = true;
    o.require_outfile = false;
}

void
ArgParser::argShowXref()
{
    o.show_xref = true;
    o.require_outfile = false;
}

void
ArgParser::argShowObject(char* parameter)
{
    QPDFJob::parse_object_id(parameter, o.show_trailer, o.show_obj, o.show_gen);
    o.require_outfile = false;
}

void
ArgParser::argRawStreamData()
{
    o.show_raw_stream_data = true;
}

void
ArgParser::argFilteredStreamData()
{
    o.show_filtered_stream_data = true;
}

void
ArgParser::argShowNpages()
{
    o.show_npages = true;
    o.require_outfile = false;
}

void
ArgParser::argShowPages()
{
    o.show_pages = true;
    o.require_outfile = false;
}

void
ArgParser::argWithImages()
{
    o.show_page_images = true;
}

void
ArgParser::argJson()
{
    o.json = true;
    o.require_outfile = false;
}

void
ArgParser::argJsonKey(char* parameter)
{
    o.json_keys.insert(parameter);
}

void
ArgParser::argJsonObject(char* parameter)
{
    o.json_objects.insert(parameter);
}

void
ArgParser::argCheck()
{
    o.check = true;
    o.require_outfile = false;
}

void
ArgParser::argOptimizeImages()
{
    o.optimize_images = true;
}

void
ArgParser::argExternalizeInlineImages()
{
    o.externalize_inline_images = true;
}

void
ArgParser::argKeepInlineImages()
{
    o.keep_inline_images = true;
}

void
ArgParser::argRemovePageLabels()
{
    o.remove_page_labels = true;
}

void
ArgParser::argOiMinWidth(char* parameter)
{
    o.oi_min_width = QUtil::string_to_uint(parameter);
}

void
ArgParser::argOiMinHeight(char* parameter)
{
    o.oi_min_height = QUtil::string_to_uint(parameter);
}

void
ArgParser::argOiMinArea(char* parameter)
{
    o.oi_min_area = QUtil::string_to_uint(parameter);
}

void
ArgParser::argIiMinBytes(char* parameter)
{
    o.ii_min_bytes = QUtil::string_to_uint(parameter);
}

void
ArgParser::argEnc40Print(char* parameter)
{
    o.r2_print = (strcmp(parameter, "y") == 0);
}

void
ArgParser::argEnc40Modify(char* parameter)
{
    o.r2_modify = (strcmp(parameter, "y") == 0);
}

void
ArgParser::argEnc40Extract(char* parameter)
{
    o.r2_extract = (strcmp(parameter, "y") == 0);
}

void
ArgParser::argEnc40Annotate(char* parameter)
{
    o.r2_annotate = (strcmp(parameter, "y") == 0);
}

void
ArgParser::argEnc128Accessibility(char* parameter)
{
    o.r3_accessibility = (strcmp(parameter, "y") == 0);
}

void
ArgParser::argEnc128Extract(char* parameter)
{
    o.r3_extract = (strcmp(parameter, "y") == 0);
}

void
ArgParser::argEnc128Print(char* parameter)
{
    if (strcmp(parameter, "full") == 0)
    {
        o.r3_print = qpdf_r3p_full;
    }
    else if (strcmp(parameter, "low") == 0)
    {
        o.r3_print = qpdf_r3p_low;
    }
    else if (strcmp(parameter, "none") == 0)
    {
        o.r3_print = qpdf_r3p_none;
    }
    else
    {
        usage("invalid print option");
    }
}

void
ArgParser::argEnc128Modify(char* parameter)
{
    if (strcmp(parameter, "all") == 0)
    {
        o.r3_assemble = true;
        o.r3_annotate_and_form = true;
        o.r3_form_filling = true;
        o.r3_modify_other = true;
    }
    else if (strcmp(parameter, "annotate") == 0)
    {
        o.r3_assemble = true;
        o.r3_annotate_and_form = true;
        o.r3_form_filling = true;
        o.r3_modify_other = false;
    }
    else if (strcmp(parameter, "form") == 0)
    {
        o.r3_assemble = true;
        o.r3_annotate_and_form = false;
        o.r3_form_filling = true;
        o.r3_modify_other = false;
    }
    else if (strcmp(parameter, "assembly") == 0)
    {
        o.r3_assemble = true;
        o.r3_annotate_and_form = false;
        o.r3_form_filling = false;
        o.r3_modify_other = false;
    }
    else if (strcmp(parameter, "none") == 0)
    {
        o.r3_assemble = false;
        o.r3_annotate_and_form = false;
        o.r3_form_filling = false;
        o.r3_modify_other = false;
    }
    else
    {
        usage("invalid modify option");
    }
}

void
ArgParser::argEnc128CleartextMetadata()
{
    o.cleartext_metadata = true;
}

void
ArgParser::argEnc128Assemble(char* parameter)
{
    o.r3_assemble = (strcmp(parameter, "y") == 0);
}

void
ArgParser::argEnc128Annotate(char* parameter)
{
    o.r3_annotate_and_form = (strcmp(parameter, "y") == 0);
}

void
ArgParser::argEnc128Form(char* parameter)
{
    o.r3_form_filling = (strcmp(parameter, "y") == 0);
}

void
ArgParser::argEnc128ModifyOther(char* parameter)
{
    o.r3_modify_other = (strcmp(parameter, "y") == 0);
}

void
ArgParser::argEnc128UseAes(char* parameter)
{
    o.use_aes = (strcmp(parameter, "y") == 0);
}

void
ArgParser::argEnc128ForceV4()
{
    o.force_V4 = true;
}

void
ArgParser::argEnc256ForceR5()
{
    o.force_R5 = true;
}

void
ArgParser::argEndEncryption()
{
    o.encrypt = true;
    o.decrypt = false;
    o.copy_encryption = false;
}

void
ArgParser::argEnd40BitEncryption()
{
    argEndEncryption();
}

void
ArgParser::argEnd128BitEncryption()
{
    argEndEncryption();
}

void
ArgParser::argEnd256BitEncryption()
{
    argEndEncryption();
}

void
ArgParser::argUOPositional(char* arg)
{
    if (o.under_overlay->filename)
    {
        usage(o.under_overlay->which + " file already specified");
    }
    else
    {
        o.under_overlay->filename = arg;
    }
}

void
ArgParser::argUOTo(char* parameter)
{
    parseNumrange(parameter, 0);
    o.under_overlay->to_nr = parameter;
}

void
ArgParser::argUOFrom(char* parameter)
{
    if (strlen(parameter))
    {
        parseNumrange(parameter, 0);
    }
    o.under_overlay->from_nr = parameter;
}

void
ArgParser::argUORepeat(char* parameter)
{
    if (strlen(parameter))
    {
        parseNumrange(parameter, 0);
    }
    o.under_overlay->repeat_nr = parameter;
}

void
ArgParser::argUOPassword(char* parameter)
{
    o.under_overlay->password = parameter;
}

void
ArgParser::argEndUnderlayOverlay()
{
    if (0 == o.under_overlay->filename)
    {
        usage(o.under_overlay->which + " file not specified");
    }
    o.under_overlay = 0;
}

void
ArgParser::argReplaceInput()
{
    o.replace_input = true;
}

void
ArgParser::argIsEncrypted()
{
    o.check_is_encrypted = true;
    o.require_outfile = false;
}

void
ArgParser::argRequiresPassword()
{
    o.check_requires_password = true;
    o.require_outfile = false;
}

void
ArgParser::argAttPositional(char* arg)
{
    o.attachments_to_add.back().path = arg;
}

void
ArgParser::argAttKey(char* parameter)
{
    o.attachments_to_add.back().key = parameter;
}

void
ArgParser::argAttFilename(char* parameter)
{
    o.attachments_to_add.back().filename = parameter;
}

void
ArgParser::argAttCreationdate(char* parameter)
{
    if (! QUtil::pdf_time_to_qpdf_time(parameter))
    {
        usage(std::string(parameter) + " is not a valid PDF timestamp");
    }
    o.attachments_to_add.back().creationdate = parameter;
}

void
ArgParser::argAttModdate(char* parameter)
{
    if (! QUtil::pdf_time_to_qpdf_time(parameter))
    {
        usage(std::string(parameter) + " is not a valid PDF timestamp");
    }
    o.attachments_to_add.back().moddate = parameter;
}

void
ArgParser::argAttMimetype(char* parameter)
{
    if (strchr(parameter, '/') == nullptr)
    {
        usage("mime type should be specified as type/subtype");
    }
    o.attachments_to_add.back().mimetype = parameter;
}

void
ArgParser::argAttDescription(char* parameter)
{
    o.attachments_to_add.back().description = parameter;
}

void
ArgParser::argAttReplace()
{
    o.attachments_to_add.back().replace = true;
}

void
ArgParser::argEndAttachment()
{
    static std::string now = QUtil::qpdf_time_to_pdf_time(
        QUtil::get_current_qpdf_time());
    auto& cur = o.attachments_to_add.back();
    if (cur.path.empty())
    {
        usage("add attachment: no path specified");
    }
    std::string last_element = QUtil::path_basename(cur.path);
    if (last_element.empty())
    {
        usage("path for --add-attachment may not be empty");
    }
    if (cur.filename.empty())
    {
        cur.filename = last_element;
    }
    if (cur.key.empty())
    {
        cur.key = last_element;
    }
    if (cur.creationdate.empty())
    {
        cur.creationdate = now;
    }
    if (cur.moddate.empty())
    {
        cur.moddate = now;
    }
}

void
ArgParser::argCopyAttPositional(char* arg)
{
    o.attachments_to_copy.back().path = arg;
}

void
ArgParser::argCopyAttPrefix(char* parameter)
{
    o.attachments_to_copy.back().prefix = parameter;
}

void
ArgParser::argCopyAttPassword(char* parameter)
{
    o.attachments_to_copy.back().password = parameter;
}

void
ArgParser::argEndCopyAttachment()
{
    if (o.attachments_to_copy.back().path.empty())
    {
        usage("copy attachments: no path specified");
    }
}

void
ArgParser::usage(std::string const& message)
{
    this->ap.usage(message);
}

std::vector<int>
ArgParser::parseNumrange(char const* range, int max, bool throw_error)
{
    try
    {
        return QUtil::parse_numrange(range, max);
    }
    catch (std::runtime_error& e)
    {
        if (throw_error)
        {
            throw(e);
        }
        else
        {
            usage(e.what());
        }
    }
    return std::vector<int>();
}

void
ArgParser::parseUnderOverlayOptions(QPDFJob::UnderOverlay* uo)
{
    o.under_overlay = uo;
    this->ap.selectOptionTable(O_UNDERLAY_OVERLAY);
}

void
ArgParser::parseRotationParameter(std::string const& parameter)
{
    std::string angle_str;
    std::string range;
    size_t colon = parameter.find(':');
    int relative = 0;
    if (colon != std::string::npos)
    {
        if (colon > 0)
        {
            angle_str = parameter.substr(0, colon);
        }
        if (colon + 1 < parameter.length())
        {
            range = parameter.substr(colon + 1);
        }
    }
    else
    {
        angle_str = parameter;
    }
    if (angle_str.length() > 0)
    {
        char first = angle_str.at(0);
        if ((first == '+') || (first == '-'))
        {
            relative = ((first == '+') ? 1 : -1);
            angle_str = angle_str.substr(1);
        }
        else if (! QUtil::is_digit(angle_str.at(0)))
        {
            angle_str = "";
        }
    }
    if (range.empty())
    {
        range = "1-z";
    }
    bool range_valid = false;
    try
    {
        parseNumrange(range.c_str(), 0, true);
        range_valid = true;
    }
    catch (std::runtime_error const&)
    {
        // ignore
    }
    if (range_valid &&
        ((angle_str == "0") ||(angle_str == "90") ||
         (angle_str == "180") || (angle_str == "270")))
    {
        int angle = QUtil::string_to_int(angle_str.c_str());
        if (relative == -1)
        {
            angle = -angle;
        }
        o.rotations[range] = QPDFJob::RotationSpec(angle, (relative != 0));
    }
    else
    {
        usage("invalid parameter to rotate: " + parameter);
    }
}

void
ArgParser::parseOptions()
{
    try
    {
        this->ap.parseArgs();
    }
    catch (QPDFArgParser::Usage& e)
    {
        usage(e.what());
    }
}

void
ArgParser::doFinalChecks()
{
    if (o.replace_input)
    {
        if (o.outfilename)
        {
            usage("--replace-input may not be used when"
                  " an output file is specified");
        }
        else if (o.split_pages)
        {
            usage("--split-pages may not be used with --replace-input");
        }
    }
    if (o.infilename == 0)
    {
        usage("an input file name is required");
    }
    else if (o.require_outfile && (o.outfilename == 0) && (! o.replace_input))
    {
        usage("an output file name is required; use - for standard output");
    }
    else if ((! o.require_outfile) &&
             ((o.outfilename != 0) || o.replace_input))
    {
        usage("no output file may be given for this option");
    }
    if (o.check_requires_password && o.check_is_encrypted)
    {
        usage("--requires-password and --is-encrypted may not be given"
              " together");
    }

    if (o.encrypt && (! o.allow_insecure) &&
        (o.owner_password.empty() &&
         (! o.user_password.empty()) &&
         (o.keylen == 256)))
    {
        // Note that empty owner passwords for R < 5 are copied from
        // the user password, so this lack of security is not an issue
        // for those files. Also we are consider only the ability to
        // open the file without a password to be insecure. We are not
        // concerned about whether the viewer enforces security
        // settings when the user and owner password match.
        usage("A PDF with a non-empty user password and an empty owner"
              " password encrypted with a 256-bit key is insecure as it"
              " can be opened without a password. If you really want to"
              " do this, you must also give the --allow-insecure option"
              " before the -- that follows --encrypt.");
    }

    if (o.require_outfile && o.outfilename &&
        (strcmp(o.outfilename, "-") == 0))
    {
        if (o.split_pages)
        {
            usage("--split-pages may not be used when"
                  " writing to standard output");
        }
        if (o.verbose)
        {
            usage("--verbose may not be used when"
                  " writing to standard output");
        }
        if (o.progress)
        {
            usage("--progress may not be used when"
                  " writing to standard output");
        }
    }

    if ((! o.split_pages) && QUtil::same_file(o.infilename, o.outfilename))
    {
        QTC::TC("qpdf", "qpdf same file error");
        usage("input file and output file are the same;"
              " use --replace-input to intentionally overwrite the input file");
    }
}

void
QPDFJob::initializeFromArgv(int argc, char* argv[], char const* progname_env)
{
    if (progname_env == nullptr)
    {
        progname_env = "QPDF_EXECUTABLE";
    }
    // QPDFArgParser must stay in scope for the life of the QPDFJob
    // object since it holds dynamic memory used for argv, which is
    // pointed to by other member variables.
    this->m->ap = new QPDFArgParser(argc, argv, progname_env);
    setMessagePrefix(this->m->ap->getProgname());
    ArgParser ap(*this->m->ap, *this);
    ap.parseOptions();
}
