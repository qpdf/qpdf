#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>

#include <qpdf/QUtil.hh>
#include <qpdf/QTC.hh>
#include <qpdf/Pl_StdioFile.hh>
#include <qpdf/Pl_Discard.hh>
#include <qpdf/PointerHolder.hh>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFExc.hh>

#include <qpdf/QPDFWriter.hh>

static int const EXIT_ERROR = 2;
static int const EXIT_WARNING = 3;

static char const* whoami = 0;

struct PageSpec
{
    PageSpec(std::string const& filename,
             char const* password,
             char const* range) :
        filename(filename),
        password(password),
        range(range)
    {
    }

    std::string filename;
    char const* password;
    char const* range;
};

struct QPDFPageData
{
    QPDFPageData(QPDF* qpdf, char const* range);

    QPDF* qpdf;
    std::vector<QPDFObjectHandle> orig_pages;
    std::vector<int> selected_pages;
};

class DiscardContents: public QPDFObjectHandle::ParserCallbacks
{
  public:
    virtual ~DiscardContents() {}
    virtual void handleObject(QPDFObjectHandle) {}
    virtual void handleEOF() {}
};

// Note: let's not be too noisy about documenting the fact that this
// software purposely fails to enforce the distinction between user
// and owner passwords.  A user password is sufficient to gain full
// access to the PDF file, so there is nothing this software can do
// with an owner password that it couldn't do with a user password
// other than changing the /P value in the encryption dictionary.
// (Setting this value requires the owner password.)  The
// documentation discusses this as well.

static char const* help = "\
\n\
Usage: qpdf [ options ] { infilename | --empty } [ outfilename ]\n\
\n\
An option summary appears below.  Please see the documentation for details.\n\
\n\
Note that when contradictory options are provided, whichever options are\n\
provided last take precedence.\n\
\n\
\n\
Basic Options\n\
-------------\n\
\n\
--password=password     specify a password for accessing encrypted files\n\
--linearize             generated a linearized (web optimized) file\n\
--copy-encryption=file  copy encryption parameters from specified file\n\
--encryption-file-password=password\n\
                        password used to open the file from which encryption\n\
                        parameters are being copied\n\
--encrypt options --    generate an encrypted file\n\
--decrypt               remove any encryption on the file\n\
--pages options --      select specific pages from one or more files\n\
\n\
If none of --copy-encryption, --encrypt or --decrypt are given, qpdf will\n\
preserve any encryption data associated with a file.\n\
\n\
Note that when copying encryption parameters from another file, all\n\
parameters will be copied, including both user and owner passwords, even\n\
if the user password is used to open the other file.  This works even if\n\
the owner password is not known.\n\
\n\
\n\
Encryption Options\n\
------------------\n\
\n\
  --encrypt user-password owner-password key-length flags --\n\
\n\
Note that -- terminates parsing of encryption flags.\n\
\n\
Either or both of the user password and the owner password may be\n\
empty strings.\n\
\n\
key-length may be 40, 128, or 256\n\
\n\
Additional flags are dependent upon key length.\n\
\n\
  If 40:\n\
\n\
    --print=[yn]             allow printing\n\
    --modify=[yn]            allow document modification\n\
    --extract=[yn]           allow text/graphic extraction\n\
    --annotate=[yn]          allow comments and form fill-in and signing\n\
\n\
  If 128:\n\
\n\
    --accessibility=[yn]     allow accessibility to visually impaired\n\
    --extract=[yn]           allow other text/graphic extraction\n\
    --print=print-opt        control printing access\n\
    --modify=modify-opt      control modify access\n\
    --cleartext-metadata     prevents encryption of metadata\n\
    --use-aes=[yn]           indicates whether to use AES encryption\n\
    --force-V4               forces use of V=4 encryption handler\n\
\n\
  If 256, options are the same as 128 with these exceptions:\n\
    --force-V4               this option is not available with 256-bit keys\n\
    --use-aes                this option is always on with 256-bit keys\n\
    --force-R5               forces use of deprecated R=5 encryption\n\
\n\
    print-opt may be:\n\
\n\
      full                  allow full printing\n\
      low                   allow only low-resolution printing\n\
      none                  disallow printing\n\
\n\
    modify-opt may be:\n\
\n\
      all                   allow full document modification\n\
      annotate              allow comment authoring and form operations\n\
      form                  allow form field fill-in and signing\n\
      assembly              allow document assembly only\n\
      none                  allow no modifications\n\
\n\
The default for each permission option is to be fully permissive.\n\
\n\
Specifying cleartext-metadata forces the PDF version to at least 1.5.\n\
Specifying use of AES forces the PDF version to at least 1.6.  These\n\
options are both off by default.\n\
\n\
The --force-V4 flag forces the V=4 encryption handler introduced in PDF 1.5\n\
to be used even if not otherwise needed.  This option is primarily useful\n\
for testing qpdf and has no other practical use.\n\
\n\
\n\
Page Selection Options\n\
----------------------\n\
\n\
These options allow pages to be selected from one or more PDF files.\n\
Whatever file is given as the primary input file is used as the\n\
starting point, but its pages are replaced with pages as specified.\n\
\n\
--pages file [ --password=password ] [ page-range ] ... --\n\
\n\
For each file that pages should be taken from, specify the file, a\n\
password needed to open the file (if any), and a page range.  The\n\
password needs to be given only once per file.  If any of the input\n\
files are the same as the primary input file or the file used to copy\n\
encryption parameters (if specified), you do not need to repeat the\n\
password here.  The same file can be repeated multiple times.  All\n\
non-page data (info, outlines, page numbers, etc. are taken from the\n\
primary input file.  To discard this, use --empty as the primary\n\
input.\n\
\n\
The page range is a set of numbers separated by commas, ranges of\n\
numbers separated dashes, or combinations of those.  The character\n\
\"z\" represents the last page.  Pages can appear in any order.  Ranges\n\
can appear with a high number followed by a low number, which causes the\n\
pages to appear in reverse.  Repeating a number will cause an error, but\n\
the manual discusses a workaround should you really want to include the\n\
same page twice.\n\
\n\
If the page range is omitted, the range of 1-z is assumed.  qpdf decides\n\
that the page range is omitted if the range argument is either -- or a\n\
valid file name and not a valid range.\n\
\n\
See the manual for examples and a discussion of additional subtleties.\n\
\n\
\n\
Advanced Transformation Options\n\
-------------------------------\n\
\n\
These transformation options control fine points of how qpdf creates\n\
the output file.  Mostly these are of use only to people who are very\n\
familiar with the PDF file format or who are PDF developers.\n\
\n\
--stream-data=option      controls transformation of stream data (below)\n\
--normalize-content=[yn]  enables or disables normalization of content streams\n\
--suppress-recovery       prevents qpdf from attempting to recover damaged files\n\
--object-streams=mode     controls handing of object streams\n\
--ignore-xref-streams     tells qpdf to ignore any cross-reference streams\n\
--qdf                     turns on \"QDF mode\" (below)\n\
--min-version=version     sets the minimum PDF version of the output file\n\
--force-version=version   forces this to be the PDF version of the output file\n\
\n\
Version numbers may be expressed as major.minor.extension-level, so 1.7.3\n\
means PDF version 1.7 at extension level 3.\n\
\n\
Values for stream data options:\n\
\n\
  compress              recompress stream data when possible (default)\n\
  preserve              leave all stream data as is\n\
  uncompress            uncompress stream data when possible\n\
\n\
Values for object stream mode:\n\
\n\
  preserve                  preserve original object streams (default)\n\
  disable                   don't write any object streams\n\
  generate                  use object streams wherever possible\n\
\n\
In qdf mode, by default, content normalization is turned on, and the\n\
stream data mode is set to uncompress.\n\
\n\
Setting the minimum PDF version of the output file may raise the version\n\
but will never lower it.  Forcing the PDF version of the output file may\n\
set the PDF version to a lower value than actually allowed by the file's\n\
contents.  You should only do this if you have no other possible way to\n\
open the file or if you know that the file definitely doesn't include\n\
features not supported later versions.\n\
\n\
Testing, Inspection, and Debugging Options\n\
------------------------------------------\n\
\n\
These options can be useful for digging into PDF files or for use in\n\
automated test suites for software that uses the qpdf library.\n\
\n\
--deterministic-id        generate deterministic /ID\n\
--static-id               generate static /ID: FOR TESTING ONLY!\n\
--static-aes-iv           use a static initialization vector for AES-CBC\n\
                          This is option is not secure!  FOR TESTING ONLY!\n\
--no-original-object-ids  suppress original object ID comments in qdf mode\n\
--show-encryption         quickly show encryption parameters\n\
--check-linearization     check file integrity and linearization status\n\
--show-linearization      check and show all linearization data\n\
--show-xref               show the contents of the cross-reference table\n\
--show-object=obj[,gen]   show the contents of the given object\n\
  --raw-stream-data       show raw stream data instead of object contents\n\
  --filtered-stream-data  show filtered stream data instead of object contents\n\
--show-npages             print the number of pages in the file\n\
--show-pages              shows the object/generation number for each page\n\
  --with-images           also shows the object IDs for images on each page\n\
--check                   check file structure + encryption, linearization\n\
\n\
The --raw-stream-data and --filtered-stream-data options are ignored\n\
unless --show-object is given.  Either of these options will cause the\n\
stream data to be written to standard output.\n\
\n\
If --filtered-stream-data is given and --normalize-content=y is also\n\
given, qpdf will attempt to normalize the stream data as if it is a\n\
page content stream.  This attempt will be made even if it is not a\n\
page content stream, in which case it will produce unusable results.\n\
\n\
Ordinarily, qpdf exits with a status of 0 on success or a status of 2\n\
if any errors occurred.  In --check mode, if there were warnings but not\n\
errors, qpdf exits with a status of 3.\n\
\n";

void usage(std::string const& msg)
{
    std::cerr
	<< std::endl
	<< whoami << ": " << msg << std::endl
	<< std::endl
	<< "Usage: " << whoami << " [options] infile outfile" << std::endl
	<< "For detailed help, run " << whoami << " --help" << std::endl
	<< std::endl;
    exit(EXIT_ERROR);
}

static std::string show_bool(bool v)
{
    return v ? "allowed" : "not allowed";
}

static std::string show_encryption_method(QPDF::encryption_method_e method)
{
    std::string result = "unknown";
    switch (method)
    {
      case QPDF::e_none:
        result = "none";
        break;
      case QPDF::e_unknown:
        result = "unknown";
        break;
      case QPDF::e_rc4:
        result = "RC4";
        break;
      case QPDF::e_aes:
        result = "AESv2";
        break;
      case QPDF::e_aesv3:
        result = "AESv3";
        break;
        // no default so gcc will warn for missing case
    }
    return result;
}

static void show_encryption(QPDF& pdf)
{
    // Extract /P from /Encrypt
    int R = 0;
    int P = 0;
    int V = 0;
    QPDF::encryption_method_e stream_method = QPDF::e_unknown;
    QPDF::encryption_method_e string_method = QPDF::e_unknown;
    QPDF::encryption_method_e file_method = QPDF::e_unknown;
    if (! pdf.isEncrypted(R, P, V,
                          stream_method, string_method, file_method))
    {
	std::cout << "File is not encrypted" << std::endl;
    }
    else
    {
	std::cout << "R = " << R << std::endl;
	std::cout << "P = " << P << std::endl;
	std::string user_password = pdf.getTrimmedUserPassword();
	std::cout << "User password = " << user_password << std::endl
                  << "extract for accessibility: "
		  << show_bool(pdf.allowAccessibility()) << std::endl
                  << "extract for any purpose: "
		  << show_bool(pdf.allowExtractAll()) << std::endl
                  << "print low resolution: "
		  << show_bool(pdf.allowPrintLowRes()) << std::endl
                  << "print high resolution: "
		  << show_bool(pdf.allowPrintHighRes()) << std::endl
                  << "modify document assembly: "
		  << show_bool(pdf.allowModifyAssembly()) << std::endl
                  << "modify forms: "
		  << show_bool(pdf.allowModifyForm()) << std::endl
                  << "modify annotations: "
		  << show_bool(pdf.allowModifyAnnotation()) << std::endl
                  << "modify other: "
		  << show_bool(pdf.allowModifyOther()) << std::endl
                  << "modify anything: "
		  << show_bool(pdf.allowModifyAll()) << std::endl;
        if (V >= 4)
        {
            std::cout << "stream encryption method: "
                      << show_encryption_method(stream_method) << std::endl
                      << "string encryption method: "
                      << show_encryption_method(string_method) << std::endl
                      << "file encryption method: "
                      << show_encryption_method(file_method) << std::endl;
        }
    }
}

static std::vector<int> parse_numrange(char const* range, int max,
                                       bool throw_error = false)
{
    std::vector<int> result;
    char const* p = range;
    try
    {
        std::vector<int> work;
        static int const comma = -1;
        static int const dash = -2;

        enum { st_top,
               st_in_number,
               st_after_number } state = st_top;
        bool last_separator_was_dash = false;
        int cur_number = 0;
        while (*p)
        {
            char ch = *p;
            if (isdigit(ch))
            {
                if (! ((state == st_top) || (state == st_in_number)))
                {
                    throw std::runtime_error("digit not expected");
                }
                state = st_in_number;
                cur_number *= 10;
                cur_number += (ch - '0');
            }
            else if (ch == 'z')
            {
                // z represents max
                if (! (state == st_top))
                {
                    throw std::runtime_error("z not expected");
                }
                state = st_after_number;
                cur_number = max;
            }
            else if ((ch == ',') || (ch == '-'))
            {
                if (! ((state == st_in_number) || (state == st_after_number)))
                {
                    throw std::runtime_error("unexpected separator");
                }
                work.push_back(cur_number);
                cur_number = 0;
                if (ch == ',')
                {
                    state = st_top;
                    last_separator_was_dash = false;
                    work.push_back(comma);
                }
                else if (ch == '-')
                {
                    if (last_separator_was_dash)
                    {
                        throw std::runtime_error("unexpected dash");
                    }
                    state = st_top;
                    last_separator_was_dash = true;
                    work.push_back(dash);
                }
            }
            else
            {
                throw std::runtime_error("unexpected character");
            }
            ++p;
        }
        if ((state == st_in_number) || (state == st_after_number))
        {
            work.push_back(cur_number);
        }
        else
        {
            throw std::runtime_error("number expected");
        }

        p = 0;
        for (size_t i = 0; i < work.size(); i += 2)
        {
            int num = work.at(i);
            // max == 0 means we don't know the max and are just
            // testing for valid syntax.
            if ((max > 0) && ((num < 1) || (num > max)))
            {
                throw std::runtime_error(
                    "number " + QUtil::int_to_string(num) + " out of range");
            }
            if (i == 0)
            {
                result.push_back(work.at(i));
            }
            else
            {
                int separator = work.at(i-1);
                if (separator == comma)
                {
                    result.push_back(num);
                }
                else if (separator == dash)
                {
                    int lastnum = result.back();
                    if (num > lastnum)
                    {
                        for (int j = lastnum + 1; j <= num; ++j)
                        {
                            result.push_back(j);
                        }
                    }
                    else
                    {
                        for (int j = lastnum - 1; j >= num; --j)
                        {
                            result.push_back(j);
                        }
                    }
                }
                else
                {
                    throw std::logic_error(
                        "INTERNAL ERROR parsing numeric range");
                }
            }
        }
    }
    catch (std::runtime_error e)
    {
        if (throw_error)
        {
            throw e;
        }
        if (p)
        {
            usage("error at * in numeric range " +
                  std::string(range, p - range) + "*" + p + ": " + e.what());
        }
        else
        {
            usage("error in numeric range " +
                  std::string(range) + ": " + e.what());
        }
    }
    return result;
}

static void
parse_encrypt_options(
    int argc, char* argv[], int& cur_arg,
    std::string& user_password, std::string& owner_password, int& keylen,
    bool& r2_print, bool& r2_modify, bool& r2_extract, bool& r2_annotate,
    bool& r3_accessibility, bool& r3_extract,
    qpdf_r3_print_e& r3_print, qpdf_r3_modify_e& r3_modify,
    bool& force_V4, bool& cleartext_metadata, bool& use_aes,
    bool& force_R5)
{
    if (cur_arg + 3 >= argc)
    {
	usage("insufficient arguments to --encrypt");
    }
    user_password = argv[cur_arg++];
    owner_password = argv[cur_arg++];
    std::string len_str = argv[cur_arg++];
    if (len_str == "40")
    {
	keylen = 40;
    }
    else if (len_str == "128")
    {
	keylen = 128;
    }
    else if (len_str == "256")
    {
	keylen = 256;
        use_aes = true;
    }
    else
    {
	usage("encryption key length must be 40, 128, or 256");
    }
    while (1)
    {
	char* arg = argv[cur_arg];
	if (arg == 0)
	{
	    usage("insufficient arguments to --encrypt");
	}
	else if (strcmp(arg, "--") == 0)
	{
	    return;
	}
	if (arg[0] == '-')
	{
	    ++arg;
	    if (arg[0] == '-')
	    {
		++arg;
	    }
	}
	else
	{
	    usage(std::string("invalid encryption parameter ") + arg);
	}
	++cur_arg;
	char* parameter = strchr(arg, '=');
	if (parameter)
	{
	    *parameter++ = 0;
	}
	if (strcmp(arg, "print") == 0)
	{
	    if (parameter == 0)
	    {
		usage("--print must be given as --print=option");
	    }
	    std::string val = parameter;
	    if (keylen == 40)
	    {
		if (val == "y")
		{
		    r2_print = true;
		}
		else if (val == "n")
		{
		    r2_print = false;
		}
		else
		{
		    usage("invalid 40-bit -print parameter");
		}
	    }
	    else
	    {
		if (val == "full")
		{
		    r3_print = qpdf_r3p_full;
		}
		else if (val == "low")
		{
		    r3_print = qpdf_r3p_low;
		}
		else if (val == "none")
		{
		    r3_print = qpdf_r3p_none;
		}
		else
		{
		    usage("invalid 128-bit -print parameter");
		}
	    }
	}
	else if (strcmp(arg, "modify") == 0)
	{
	    if (parameter == 0)
	    {
		usage("--modify must be given as --modify=option");
	    }
	    std::string val = parameter;
	    if (keylen == 40)
	    {
		if (val == "y")
		{
		    r2_modify = true;
		}
		else if (val == "n")
		{
		    r2_modify = false;
		}
		else
		{
		    usage("invalid 40-bit -modify parameter");
		}
	    }
	    else
	    {
		if (val == "all")
		{
		    r3_modify = qpdf_r3m_all;
		}
		else if (val == "annotate")
		{
		    r3_modify = qpdf_r3m_annotate;
		}
		else if (val == "form")
		{
		    r3_modify = qpdf_r3m_form;
		}
		else if (val == "assembly")
		{
		    r3_modify = qpdf_r3m_assembly;
		}
		else if (val == "none")
		{
		    r3_modify = qpdf_r3m_none;
		}
		else
		{
		    usage("invalid 128-bit -modify parameter");
		}
	    }
	}
	else if (strcmp(arg, "extract") == 0)
	{
	    if (parameter == 0)
	    {
		usage("--extract must be given as --extract=option");
	    }
	    std::string val = parameter;
	    bool result = false;
	    if (val == "y")
	    {
		result = true;
	    }
	    else if (val == "n")
	    {
		result = false;
	    }
	    else
	    {
		usage("invalid -extract parameter");
	    }
	    if (keylen == 40)
	    {
		r2_extract = result;
	    }
	    else
	    {
		r3_extract = result;
	    }
	}
	else if (strcmp(arg, "annotate") == 0)
	{
	    if (parameter == 0)
	    {
		usage("--annotate must be given as --annotate=option");
	    }
	    std::string val = parameter;
	    bool result = false;
	    if (val == "y")
	    {
		result = true;
	    }
	    else if (val == "n")
	    {
		result = false;
	    }
	    else
	    {
		usage("invalid -annotate parameter");
	    }
	    if (keylen == 40)
	    {
		r2_annotate = result;
	    }
	    else
	    {
		usage("-annotate invalid for 128-bit keys");
	    }
	}
	else if (strcmp(arg, "accessibility") == 0)
	{
	    if (parameter == 0)
	    {
		usage("--accessibility must be given as"
		      " --accessibility=option");
	    }
	    std::string val = parameter;
	    bool result = false;
	    if (val == "y")
	    {
		result = true;
	    }
	    else if (val == "n")
	    {
		result = false;
	    }
	    else
	    {
		usage("invalid -accessibility parameter");
	    }
	    if (keylen == 40)
	    {
		usage("-accessibility invalid for 40-bit keys");
	    }
	    else
	    {
		r3_accessibility = result;
	    }
	}
	else if (strcmp(arg, "cleartext-metadata") == 0)
	{
	    if (parameter)
	    {
		usage("--cleartext-metadata does not take a parameter");
	    }
	    if (keylen == 40)
	    {
		usage("--cleartext-metadata is invalid for 40-bit keys");
	    }
	    else
	    {
		cleartext_metadata = true;
	    }
	}
	else if (strcmp(arg, "force-V4") == 0)
	{
	    if (parameter)
	    {
		usage("--force-V4 does not take a parameter");
	    }
	    if (keylen != 128)
	    {
		usage("--force-V4 is invalid only for 128-bit keys");
	    }
	    else
	    {
		force_V4 = true;
	    }
	}
	else if (strcmp(arg, "force-R5") == 0)
	{
	    if (parameter)
	    {
		usage("--force-R5 does not take a parameter");
	    }
	    if (keylen != 256)
	    {
		usage("--force-R5 is invalid only for 256-bit keys");
	    }
	    else
	    {
		force_R5 = true;
	    }
	}
	else if (strcmp(arg, "use-aes") == 0)
	{
	    if (parameter == 0)
	    {
		usage("--use-aes must be given as --extract=option");
	    }
	    std::string val = parameter;
	    bool result = false;
	    if (val == "y")
	    {
		result = true;
	    }
	    else if (val == "n")
	    {
		result = false;
	    }
	    else
	    {
		usage("invalid -use-aes parameter");
	    }
	    if ((keylen == 40) && result)
	    {
		usage("use-aes is invalid for 40-bit keys");
	    }
            else if ((keylen == 256) && (! result))
            {
                // qpdf would happily create files encrypted with RC4
                // using /V=5, but Adobe reader can't read them.
                usage("use-aes can't be disabled with 256-bit keys");
            }
	    else
	    {
		use_aes = result;
	    }
	}
	else
	{
	    usage(std::string("invalid encryption parameter --") + arg);
	}
    }
}

static std::vector<PageSpec>
parse_pages_options(
    int argc, char* argv[], int& cur_arg)
{
    std::vector<PageSpec> result;
    while (1)
    {
        if ((cur_arg < argc) && (strcmp(argv[cur_arg], "--") == 0))
        {
            break;
        }
        if (cur_arg + 2 >= argc)
        {
            usage("insufficient arguments to --pages");
        }
        char const* file = argv[cur_arg++];
        char const* password = 0;
        char const* range = argv[cur_arg++];
        if (strncmp(range, "--password=", 11) == 0)
        {
            // Oh, that's the password, not the range
            if (cur_arg + 1 >= argc)
            {
                usage("insufficient arguments to --pages");
            }
            password = range + 11;
            range = argv[cur_arg++];
        }

        // See if the user omitted the range entirely, in which case
        // we assume "1-z".
        bool range_omitted = false;
        if (strcmp(range, "--") == 0)
        {
            // The filename or password was the last argument
            QTC::TC("qpdf", "qpdf pages range omitted at end");
            range_omitted = true;
        }
        else
        {
            try
            {
                parse_numrange(range, 0, true);
            }
            catch (std::runtime_error& e1)
            {
                // The range is invalid.  Let's see if it's a file.
                try
                {
                    fclose(QUtil::safe_fopen(range, "rb"));
                    // Yup, it's a file.
                    QTC::TC("qpdf", "qpdf pages range omitted in middle");
                    range_omitted = true;
                }
                catch (std::runtime_error& e2)
                {
                    // Ignore.  The range is invalid and not a file.
                    // We'll get an error message later.
                }
            }
        }
        if (range_omitted)
        {
            --cur_arg;
            range = "1-z";
        }

        result.push_back(PageSpec(file, password, range));
    }
    return result;
}

static void test_numrange(char const* range)
{
    if (range == 0)
    {
        std::cout << "null" << std::endl;
    }
    else
    {
        std::vector<int> result = parse_numrange(range, 15);
        std::cout << "numeric range " << range << " ->";
        for (std::vector<int>::iterator iter = result.begin();
             iter != result.end(); ++iter)
        {
            std::cout << " " << *iter;
        }
        std::cout << std::endl;
    }
}

QPDFPageData::QPDFPageData(QPDF* qpdf, char const* range) :
    qpdf(qpdf),
    orig_pages(qpdf->getAllPages())
{
    this->selected_pages = parse_numrange(range, this->orig_pages.size());
}

static void parse_version(std::string const& full_version_string,
                          std::string& version, int& extension_level)
{
    PointerHolder<char> vp(true, QUtil::copy_string(full_version_string));
    char* v = vp.getPointer();
    char* p1 = strchr(v, '.');
    char* p2 = (p1 ? strchr(1 + p1, '.') : 0);
    if (p2 && *(p2 + 1))
    {
        *p2++ = '\0';
        extension_level = atoi(p2);
    }
    version = v;
}

int main(int argc, char* argv[])
{
    whoami = QUtil::getWhoami(argv[0]);
    QUtil::setLineBuf(stdout);

    // For libtool's sake....
    if (strncmp(whoami, "lt-", 3) == 0)
    {
	whoami += 3;
    }

    if ((argc == 2) &&
	((strcmp(argv[1], "--version") == 0) ||
	 (strcmp(argv[1], "-version") == 0)))
    {
	// make_dist looks for the line of code here that actually
	// prints the version number, so read make_dist if you change
	// anything other than the version number.  Don't worry about
	// the numbers.  That's just a guide to 80 columns so that the
	// help message looks right on an 80-column display.

	//               1         2         3         4         5         6         7         8
	//      12345678901234567890123456789012345678901234567890123456789012345678901234567890
	std::cout
	    << whoami << " version " << QPDF::QPDFVersion() << std::endl
	    << "Copyright (c) 2005-2015 Jay Berkenbilt"
	    << std::endl
	    << "This software may be distributed under the terms of version 2 of the"
	    << std::endl
	    << "Artistic License which may be found in the source distribution.  It is"
	    << std::endl
	    << "provided \"as is\" without express or implied warranty."
	    << std::endl;
	exit(0);
    }

    if ((argc == 2) &&
	((strcmp(argv[1], "--help") == 0) ||
	 (strcmp(argv[1], "-help") == 0)))
    {
	std::cout << help;
	exit(0);
    }

    char const* password = 0;
    bool linearize = false;
    bool decrypt = false;

    bool copy_encryption = false;
    char const* encryption_file = 0;
    char const* encryption_file_password = 0;

    bool encrypt = false;
    std::string user_password;
    std::string owner_password;
    int keylen = 0;
    bool r2_print = true;
    bool r2_modify = true;
    bool r2_extract = true;
    bool r2_annotate = true;
    bool r3_accessibility = true;
    bool r3_extract = true;
    qpdf_r3_print_e r3_print = qpdf_r3p_full;
    qpdf_r3_modify_e r3_modify = qpdf_r3m_all;
    bool force_V4 = false;
    bool force_R5 = false;
    bool cleartext_metadata = false;
    bool use_aes = false;

    bool stream_data_set = false;
    qpdf_stream_data_e stream_data_mode = qpdf_s_compress;
    bool normalize_set = false;
    bool normalize = false;
    bool suppress_recovery = false;
    bool object_stream_set = false;
    qpdf_object_stream_e object_stream_mode = qpdf_o_preserve;
    bool ignore_xref_streams = false;
    bool qdf_mode = false;
    std::string min_version;
    std::string force_version;

    bool show_npages = false;
    bool deterministic_id = false;
    bool static_id = false;
    bool static_aes_iv = false;
    bool suppress_original_object_id = false;
    bool show_encryption = false;
    bool check_linearization = false;
    bool show_linearization = false;
    bool show_xref = false;
    int show_obj = 0;
    int show_gen = 0;
    bool show_raw_stream_data = false;
    bool show_filtered_stream_data = false;
    bool show_pages = false;
    bool show_page_images = false;
    bool check = false;

    std::vector<PageSpec> page_specs;

    bool require_outfile = true;
    char const* infilename = 0;
    char const* outfilename = 0;

    for (int i = 1; i < argc; ++i)
    {
	char const* arg = argv[i];
	if ((arg[0] == '-') && (strcmp(arg, "-") != 0))
	{
	    ++arg;
	    if (arg[0] == '-')
	    {
		// Be lax about -arg vs --arg
		++arg;
	    }
	    char* parameter = const_cast<char*>(strchr(arg, '='));
	    if (parameter)
	    {
		*parameter++ = 0;
	    }

            // Arguments that start with space are undocumented and
            // are for use by the test suite.
            if (strcmp(arg, " test-numrange") == 0)
            {
                test_numrange(parameter);
                exit(0);
            }
            else if (strcmp(arg, "password") == 0)
	    {
		if (parameter == 0)
		{
		    usage("--password must be given as --password=pass");
		}
		password = parameter;
	    }
            else if (strcmp(arg, "empty") == 0)
            {
                infilename = "";
            }
	    else if (strcmp(arg, "linearize") == 0)
	    {
		linearize = true;
	    }
	    else if (strcmp(arg, "encrypt") == 0)
	    {
		parse_encrypt_options(
		    argc, argv, ++i,
		    user_password, owner_password, keylen,
		    r2_print, r2_modify, r2_extract, r2_annotate,
		    r3_accessibility, r3_extract, r3_print, r3_modify,
		    force_V4, cleartext_metadata, use_aes, force_R5);
		encrypt = true;
                decrypt = false;
                copy_encryption = false;
	    }
	    else if (strcmp(arg, "decrypt") == 0)
	    {
		decrypt = true;
                encrypt = false;
                copy_encryption = false;
	    }
            else if (strcmp(arg, "copy-encryption") == 0)
            {
		if (parameter == 0)
		{
		    usage("--copy-encryption must be given as"
			  "--copy_encryption=file");
		}
                encryption_file = parameter;
                copy_encryption = true;
                encrypt = false;
                decrypt = false;
            }
            else if (strcmp(arg, "encryption-file-password") == 0)
            {
		if (parameter == 0)
		{
		    usage("--encryption-file-password must be given as"
			  "--encryption-file-password=password");
		}
                encryption_file_password = parameter;
            }
            else if (strcmp(arg, "pages") == 0)
            {
		page_specs = parse_pages_options(argc, argv, ++i);
                if (page_specs.empty())
                {
                    usage("--pages: no page specifications given");
                }
            }
	    else if (strcmp(arg, "stream-data") == 0)
	    {
		if (parameter == 0)
		{
		    usage("--stream-data must be given as"
			  "--stream-data=option");
		}
		stream_data_set = true;
		if (strcmp(parameter, "compress") == 0)
		{
		    stream_data_mode = qpdf_s_compress;
		}
		else if (strcmp(parameter, "preserve") == 0)
		{
		    stream_data_mode = qpdf_s_preserve;
		}
		else if (strcmp(parameter, "uncompress") == 0)
		{
		    stream_data_mode = qpdf_s_uncompress;
		}
		else
		{
		    usage("invalid stream-data option");
		}
	    }
	    else if (strcmp(arg, "normalize-content") == 0)
	    {
		if ((parameter == 0) || (*parameter == '\0'))
		{
		    usage("--normalize-content must be given as"
			  " --normalize-content=[yn]");
		}
		normalize_set = true;
		normalize = (parameter[0] == 'y');
	    }
	    else if (strcmp(arg, "suppress-recovery") == 0)
	    {
		suppress_recovery = true;
	    }
	    else if (strcmp(arg, "object-streams") == 0)
	    {
		if (parameter == 0)
		{
		    usage("--object-streams must be given as"
			  " --object-streams=option");
		}
		object_stream_set = true;
		if (strcmp(parameter, "disable") == 0)
		{
		    object_stream_mode = qpdf_o_disable;
		}
		else if (strcmp(parameter, "preserve") == 0)
		{
		    object_stream_mode = qpdf_o_preserve;
		}
		else if (strcmp(parameter, "generate") == 0)
		{
		    object_stream_mode = qpdf_o_generate;
		}
		else
		{
		    usage("invalid object stream mode");
		}
	    }
	    else if (strcmp(arg, "ignore-xref-streams") == 0)
	    {
		ignore_xref_streams = true;
	    }
	    else if (strcmp(arg, "qdf") == 0)
	    {
		qdf_mode = true;
	    }
	    else if (strcmp(arg, "min-version") == 0)
	    {
		if (parameter == 0)
		{
		    usage("--min-version be given as"
			  "--min-version=version");
		}
		min_version = parameter;
	    }
	    else if (strcmp(arg, "force-version") == 0)
	    {
		if (parameter == 0)
		{
		    usage("--force-version be given as"
			  "--force-version=version");
		}
		force_version = parameter;
	    }
	    else if (strcmp(arg, "deterministic-id") == 0)
	    {
		deterministic_id = true;
	    }
	    else if (strcmp(arg, "static-id") == 0)
	    {
		static_id = true;
	    }
	    else if (strcmp(arg, "static-aes-iv") == 0)
	    {
		static_aes_iv = true;
	    }
	    else if (strcmp(arg, "no-original-object-ids") == 0)
	    {
		suppress_original_object_id = true;
	    }
	    else if (strcmp(arg, "show-encryption") == 0)
	    {
		show_encryption = true;
		require_outfile = false;
	    }
	    else if (strcmp(arg, "check-linearization") == 0)
	    {
		check_linearization = true;
		require_outfile = false;
	    }
	    else if (strcmp(arg, "show-linearization") == 0)
	    {
		show_linearization = true;
		require_outfile = false;
	    }
	    else if (strcmp(arg, "show-xref") == 0)
	    {
		show_xref = true;
		require_outfile = false;
	    }
	    else if (strcmp(arg, "show-object") == 0)
	    {
		if (parameter == 0)
		{
		    usage("--show-object must be given as"
			  " --show-object=obj[,gen]");
		}
		char* obj = parameter;
		char* gen = obj;
		if ((gen = strchr(obj, ',')) != 0)
		{
		    *gen++ = 0;
		    show_gen = atoi(gen);
		}
		show_obj = atoi(obj);
		require_outfile = false;
	    }
	    else if (strcmp(arg, "raw-stream-data") == 0)
	    {
		show_raw_stream_data = true;
	    }
	    else if (strcmp(arg, "filtered-stream-data") == 0)
	    {
		show_filtered_stream_data = true;
	    }
            else if (strcmp(arg, "show-npages") == 0)
            {
                show_npages = true;
                require_outfile = false;
            }
	    else if (strcmp(arg, "show-pages") == 0)
	    {
		show_pages = true;
		require_outfile = false;
	    }
	    else if (strcmp(arg, "with-images") == 0)
	    {
		show_page_images = true;
	    }
	    else if (strcmp(arg, "check") == 0)
	    {
		check = true;
		require_outfile = false;
	    }
	    else
	    {
		usage(std::string("unknown option --") + arg);
	    }
	}
	else if (infilename == 0)
	{
	    infilename = arg;
	}
	else if (outfilename == 0)
	{
	    outfilename = arg;
	}
	else
	{
	    usage(std::string("unknown argument ") + arg);
	}
    }

    if (infilename == 0)
    {
	usage("an input file name is required");
    }
    else if (require_outfile && (outfilename == 0))
    {
	usage("an output file name is required; use - for standard output");
    }
    else if ((! require_outfile) && (outfilename != 0))
    {
	usage("no output file may be given for this option");
    }

    try
    {
	QPDF pdf;
        QPDF encryption_pdf;
	if (ignore_xref_streams)
	{
	    pdf.setIgnoreXRefStreams(true);
	}
	if (suppress_recovery)
	{
	    pdf.setAttemptRecovery(false);
	}
        if (strcmp(infilename, "") == 0)
        {
            pdf.emptyPDF();
        }
        else
        {
            pdf.processFile(infilename, password);
        }
	if (outfilename == 0)
	{
            if (show_npages)
            {
                QTC::TC("qpdf", "qpdf npages");
                std::cout << pdf.getRoot().getKey("/Pages").
                    getKey("/Count").getIntValue() << std::endl;
            }
	    if (show_encryption)
	    {
		::show_encryption(pdf);
	    }
	    if (check_linearization)
	    {
		if (pdf.checkLinearization())
		{
		    std::cout << infilename << ": no linearization errors"
			      << std::endl;
		}
		else
		{
		    exit(EXIT_ERROR);
		}
	    }
	    if (show_linearization)
	    {
		if (pdf.isLinearized())
		{
		    pdf.showLinearizationData();
		}
		else
		{
		    std::cout << infilename << " is not linearized"
			      << std::endl;
		}
	    }
	    if (show_xref)
	    {
		pdf.showXRefTable();
	    }
	    if (show_obj > 0)
	    {
		QPDFObjectHandle obj = pdf.getObjectByID(show_obj, show_gen);
		if (obj.isStream())
		{
		    if (show_raw_stream_data || show_filtered_stream_data)
		    {
			bool filter = show_filtered_stream_data;
			if (filter &&
			    (! obj.pipeStreamData(0, true, false, false)))
			{
			    QTC::TC("qpdf", "qpdf unable to filter");
			    std::cerr << "Unable to filter stream data."
				      << std::endl;
			    exit(EXIT_ERROR);
			}
			else
			{
			    QUtil::binary_stdout();
			    Pl_StdioFile out("stdout", stdout);
			    obj.pipeStreamData(&out, filter, normalize, false);
			}
		    }
		    else
		    {
			std::cout
			    << "Object is stream.  Dictionary:" << std::endl
			    << obj.getDict().unparseResolved() << std::endl;
		    }
		}
		else
		{
		    std::cout << obj.unparseResolved() << std::endl;
		}
	    }
	    if (show_pages)
	    {
                if (show_page_images)
                {
                    pdf.pushInheritedAttributesToPage();
                }
		std::vector<QPDFObjectHandle> pages = pdf.getAllPages();
		int pageno = 0;
		for (std::vector<QPDFObjectHandle>::iterator iter =
			 pages.begin();
		     iter != pages.end(); ++iter)
		{
		    QPDFObjectHandle& page = *iter;
		    ++pageno;

		    std::cout << "page " << pageno << ": "
			      << page.getObjectID() << " "
			      << page.getGeneration() << " R" << std::endl;
		    if (show_page_images)
		    {
			std::map<std::string, QPDFObjectHandle> images =
			    page.getPageImages();
			if (! images.empty())
			{
			    std::cout << "  images:" << std::endl;
			    for (std::map<std::string,
				     QPDFObjectHandle>::iterator
				     iter = images.begin();
				 iter != images.end(); ++iter)
			    {
				std::string const& name = (*iter).first;
				QPDFObjectHandle image = (*iter).second;
				QPDFObjectHandle dict = image.getDict();
				int width =
				    dict.getKey("/Width").getIntValue();
				int height =
				    dict.getKey("/Height").getIntValue();
				std::cout << "    " << name << ": "
					  << image.unparse()
					  << ", " << width << " x " << height
					  << std::endl;
			    }
			}
		    }

		    std::cout << "  content:" << std::endl;
		    std::vector<QPDFObjectHandle> content =
			page.getPageContents();
		    for (std::vector<QPDFObjectHandle>::iterator iter =
			     content.begin();
			 iter != content.end(); ++iter)
		    {
			std::cout << "    " << (*iter).unparse() << std::endl;
		    }
		}
	    }
	    if (check)
	    {
                // Code below may set okay to false but not to true.
                // We assume okay until we prove otherwise but may
                // continue to perform additional checks after finding
                // errors.
		bool okay = true;
		std::cout << "checking " << infilename << std::endl;
		try
		{
                    int extension_level = pdf.getExtensionLevel();
		    std::cout << "PDF Version: " << pdf.getPDFVersion();
                    if (extension_level > 0)
                    {
                        std::cout << " extension level "
                                  << pdf.getExtensionLevel();
                    }
                    std::cout << std::endl;
		    ::show_encryption(pdf);
		    if (pdf.isLinearized())
		    {
			std::cout << "File is linearized\n";
			if (! pdf.checkLinearization())
                        {
                            // any errors are reported by checkLinearization()
                            okay = false;
                        }
		    }
		    else
		    {
			std::cout << "File is not linearized\n";
                    }

                    // Write the file no nowhere, uncompressing
                    // streams.  This causes full file traversal and
                    // decoding of all streams we can decode.
                    QPDFWriter w(pdf);
                    Pl_Discard discard;
                    w.setOutputPipeline(&discard);
                    w.setStreamDataMode(qpdf_s_uncompress);
                    w.write();

                    // Parse all content streams
                    std::vector<QPDFObjectHandle> pages = pdf.getAllPages();
                    DiscardContents discard_contents;
                    int pageno = 0;
                    for (std::vector<QPDFObjectHandle>::iterator iter =
                             pages.begin();
                         iter != pages.end(); ++iter)
                    {
                        ++pageno;
                        try
                        {
                            QPDFObjectHandle::parseContentStream(
                                (*iter).getKey("/Contents"),
                                &discard_contents);
                        }
                        catch (QPDFExc& e)
                        {
                            okay = false;
                            std::cout << "page " << pageno << ": "
                                      << e.what() << std::endl;
                        }
                    }
		}
		catch (std::exception& e)
		{
		    std::cout << e.what() << std::endl;
                    okay = false;
		}
		if (okay)
		{
		    if (! pdf.getWarnings().empty())
		    {
			exit(EXIT_WARNING);
		    }
		    else
		    {
			std::cout << "No syntax or stream encoding errors"
				  << " found; the file may still contain"
				  << std::endl
				  << "errors that qpdf cannot detect"
				  << std::endl;
		    }
		}
                else
                {
                    exit(EXIT_ERROR);
                }
	    }
	}
	else
	{
            std::vector<PointerHolder<QPDF> > page_heap;
            if (! page_specs.empty())
            {
                // Parse all page specifications and translate them
                // into lists of actual pages.

                // Create a QPDF object for each file that we may take
                // pages from.
                std::map<std::string, QPDF*> page_spec_qpdfs;
                page_spec_qpdfs[infilename] = &pdf;
                std::vector<QPDFPageData> parsed_specs;
                for (std::vector<PageSpec>::iterator iter = page_specs.begin();
                     iter != page_specs.end(); ++iter)
                {
                    PageSpec& page_spec = *iter;
                    if (page_spec_qpdfs.count(page_spec.filename) == 0)
                    {
                        // Open the PDF file and store the QPDF
                        // object.  Throw a PointerHolder to the qpdf
                        // into a heap so that it survives through
                        // writing the output but gets cleaned up
                        // automatically at the end.  Do not
                        // canonicalize the file name.  Using two
                        // different paths to refer to the same file
                        // is a document workaround for duplicating a
                        // page.  If you are using this an example of
                        // how to do this with the API, you can just
                        // create two different QPDF objects to the
                        // same underlying file with the same path to
                        // achieve the same affect.
                        PointerHolder<QPDF> qpdf_ph = new QPDF();
                        page_heap.push_back(qpdf_ph);
                        QPDF* qpdf = qpdf_ph.getPointer();
                        char const* password = page_spec.password;
                        if (encryption_file && (password == 0) &&
                            (page_spec.filename == encryption_file))
                        {
                            QTC::TC("qpdf", "qpdf pages encryption password");
                            password = encryption_file_password;
                        }
                        qpdf->processFile(
                            page_spec.filename.c_str(), password);
                        page_spec_qpdfs[page_spec.filename] = qpdf;
                    }

                    // Read original pages from the PDF, and parse the
                    // page range associated with this occurrence of
                    // the file.
                    parsed_specs.push_back(
                        QPDFPageData(page_spec_qpdfs[page_spec.filename],
                                     page_spec.range));
                }

                // Clear all pages out of the primary QPDF's pages
                // tree but leave the objects in place in the file so
                // they can be re-added without changing their object
                // numbers.  This enables other things in the original
                // file, such as outlines, to continue to work.
                std::vector<QPDFObjectHandle> orig_pages = pdf.getAllPages();
                for (std::vector<QPDFObjectHandle>::iterator iter =
                         orig_pages.begin();
                     iter != orig_pages.end(); ++iter)
                {
                    pdf.removePage(*iter);
                }

                // Add all the pages from all the files in the order
                // specified.  Keep track of any pages from the
                // original file that we are selecting.
                std::set<int> selected_from_orig;
                for (std::vector<QPDFPageData>::iterator iter =
                         parsed_specs.begin();
                     iter != parsed_specs.end(); ++iter)
                {
                    QPDFPageData& page_data = *iter;
                    for (std::vector<int>::iterator pageno_iter =
                             page_data.selected_pages.begin();
                         pageno_iter != page_data.selected_pages.end();
                         ++pageno_iter)
                    {
                        // Pages are specified from 1 but numbered
                        // from 0 in the vector
                        int pageno = *pageno_iter - 1;
                        pdf.addPage(page_data.orig_pages.at(pageno), false);
                        if (page_data.qpdf == &pdf)
                        {
                            // This is a page from the original file.
                            // Keep track of the fact that we are
                            // using it.
                            selected_from_orig.insert(pageno);
                        }
                    }
                }

                // Delete page objects for unused page in primary.
                // This prevents those objects from being preserved by
                // being referred to from other places, such as the
                // outlines dictionary.
                for (size_t pageno = 0; pageno < orig_pages.size(); ++pageno)
                {
                    if (selected_from_orig.count(pageno) == 0)
                    {
                        pdf.replaceObject(orig_pages.at(pageno).getObjGen(),
                                          QPDFObjectHandle::newNull());
                    }
                }
            }

	    if (strcmp(outfilename, "-") == 0)
	    {
		outfilename = 0;
	    }
	    QPDFWriter w(pdf, outfilename);
	    if (qdf_mode)
	    {
		w.setQDFMode(true);
	    }
	    if (normalize_set)
	    {
		w.setContentNormalization(normalize);
	    }
	    if (stream_data_set)
	    {
		w.setStreamDataMode(stream_data_mode);
	    }
	    if (decrypt)
	    {
		w.setPreserveEncryption(false);
	    }
            if (deterministic_id)
            {
                w.setDeterministicID(true);
            }
	    if (static_id)
	    {
		w.setStaticID(true);
	    }
	    if (static_aes_iv)
	    {
		w.setStaticAesIV(true);
	    }
	    if (suppress_original_object_id)
	    {
		w.setSuppressOriginalObjectIDs(true);
	    }
            if (copy_encryption)
            {
                encryption_pdf.processFile(
                    encryption_file, encryption_file_password);
                w.copyEncryptionParameters(encryption_pdf);
            }
	    if (encrypt)
	    {
                int R = 0;
		if (keylen == 40)
		{
                    R = 2;
		}
		else if (keylen == 128)
		{
		    if (force_V4 || cleartext_metadata || use_aes)
		    {
                        R = 4;
		    }
		    else
		    {
                        R = 3;
		    }
		}
		else if (keylen == 256)
		{
		    if (force_R5)
                    {
                        R = 5;
		    }
		    else
		    {
                        R = 6;
		    }
		}
		else
		{
		    throw std::logic_error("bad encryption keylen");
		}
                if ((R > 3) && (r3_accessibility == false))
                {
                    std::cerr << whoami
                              << ": -accessibility=n is ignored for modern"
                              << " encryption formats" << std::endl;
                }
                switch (R)
                {
                  case 2:
		    w.setR2EncryptionParameters(
			user_password.c_str(), owner_password.c_str(),
			r2_print, r2_modify, r2_extract, r2_annotate);
                    break;
                  case 3:
                    w.setR3EncryptionParameters(
                        user_password.c_str(), owner_password.c_str(),
                        r3_accessibility, r3_extract, r3_print, r3_modify);
                    break;
                  case 4:
                    w.setR4EncryptionParameters(
                        user_password.c_str(), owner_password.c_str(),
                        r3_accessibility, r3_extract, r3_print, r3_modify,
                        !cleartext_metadata, use_aes);
                    break;
                  case 5:
                    w.setR5EncryptionParameters(
                        user_password.c_str(), owner_password.c_str(),
                        r3_accessibility, r3_extract, r3_print, r3_modify,
                        !cleartext_metadata);
                    break;
                  case 6:
                    w.setR6EncryptionParameters(
                        user_password.c_str(), owner_password.c_str(),
                        r3_accessibility, r3_extract, r3_print, r3_modify,
                        !cleartext_metadata);
                    break;
                  default:
		    throw std::logic_error("bad encryption R value");
                    break;
                }
	    }
	    if (linearize)
	    {
		w.setLinearization(true);
	    }
	    if (object_stream_set)
	    {
		w.setObjectStreamMode(object_stream_mode);
	    }
	    if (! min_version.empty())
	    {
                std::string version;
                int extension_level = 0;
                parse_version(min_version, version, extension_level);
		w.setMinimumPDFVersion(version, extension_level);
	    }
	    if (! force_version.empty())
	    {
                std::string version;
                int extension_level = 0;
                parse_version(force_version, version, extension_level);
		w.forcePDFVersion(version, extension_level);
	    }
	    w.write();
	}
	if (! pdf.getWarnings().empty())
	{
	    std::cerr << whoami << ": operation succeeded with warnings;"
		      << " resulting file may have some problems" << std::endl;
	    exit(EXIT_WARNING);
	}
    }
    catch (std::exception& e)
    {
	std::cerr << e.what() << std::endl;
	exit(EXIT_ERROR);
    }

    return 0;
}
