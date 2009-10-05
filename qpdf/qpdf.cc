#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

#include <qpdf/QUtil.hh>
#include <qpdf/QTC.hh>
#include <qpdf/Pl_StdioFile.hh>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFExc.hh>

#include <qpdf/QPDFWriter.hh>

static int const EXIT_ERROR = 2;
static int const EXIT_WARNING = 3;

static char const* whoami = 0;

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
Usage: qpdf [ options ] infilename outfilename\n\
\n\
An option summary appears below.  Please see the documentation for details.\n\
\n\
\n\
Basic Options\n\
-------------\n\
\n\
--password=password     specify a password for accessing encrypted files\n\
--linearize             generated a linearized (web optimized) file\n\
--encrypt options --    generate an encrypted file\n\
--decrypt               remove any encryption on the file\n\
\n\
If neither --encrypt or --decrypt are given, qpdf will preserve any\n\
encryption data associated with a file.\n\
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
key-length may be 40 or 128\n\
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
--static-id               generate static /ID: FOR TESTING ONLY!\n\
--no-original-object-ids  suppress original object ID comments in qdf mode\n\
--show-encryption         quickly show encryption parameters\n\
--check-linearization     check file integrity and linearization status\n\
--show-linearization      check and show all linearization data\n\
--show-xref               show the contents of the cross-reference table\n\
--show-object=obj[,gen]   show the contents of the given object\n\
  --raw-stream-data       show raw stream data instead of object contents\n\
  --filtered-stream-data  show filtered stream data instead of object contents\n\
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

static void show_encryption(QPDF& pdf)
{
    // Extract /P from /Encrypt
    int R = 0;
    int P = 0;
    if (! pdf.isEncrypted(R, P))
    {
	std::cout << "File is not encrypted" << std::endl;
    }
    else
    {
	std::cout << "R = " << R << std::endl;
	std::cout << "P = " << P << std::endl;
	std::string user_password = pdf.getTrimmedUserPassword();
	std::cout << "User password = " << user_password << std::endl;
	std::cout << "extract for accessibility: "
		  << show_bool(pdf.allowAccessibility()) << std::endl;
	std::cout << "extract for any purpose: "
		  << show_bool(pdf.allowExtractAll()) << std::endl;
	std::cout << "print low resolution: "
		  << show_bool(pdf.allowPrintLowRes()) << std::endl;
	std::cout << "print high resolution: "
		  << show_bool(pdf.allowPrintHighRes()) << std::endl;
	std::cout << "modify document assembly: "
		  << show_bool(pdf.allowModifyAssembly()) << std::endl;
	std::cout << "modify forms: "
		  << show_bool(pdf.allowModifyForm()) << std::endl;
	std::cout << "modify annotations: "
		  << show_bool(pdf.allowModifyAnnotation()) << std::endl;
	std::cout << "modify other: "
		  << show_bool(pdf.allowModifyOther()) << std::endl;
	std::cout << "modify anything: "
		  << show_bool(pdf.allowModifyAll()) << std::endl;
    }
}

static void
parse_encrypt_options(
    int argc, char* argv[], int& cur_arg,
    std::string& user_password, std::string& owner_password, int& keylen,
    bool& r2_print, bool& r2_modify, bool& r2_extract, bool& r2_annotate,
    bool& r3_accessibility, bool& r3_extract,
    QPDFWriter::r3_print_e& r3_print, QPDFWriter::r3_modify_e& r3_modify)
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
    else
    {
	usage("encryption key length must be 40 or 128");
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
		    r3_print = QPDFWriter::r3p_full;
		}
		else if (val == "low")
		{
		    r3_print = QPDFWriter::r3p_low;
		}
		else if (val == "none")
		{
		    r3_print = QPDFWriter::r3p_none;
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
		    r3_modify = QPDFWriter::r3m_all;
		}
		else if (val == "annotate")
		{
		    r3_modify = QPDFWriter::r3m_annotate;
		}
		else if (val == "form")
		{
		    r3_modify = QPDFWriter::r3m_form;
		}
		else if (val == "assembly")
		{
		    r3_modify = QPDFWriter::r3m_assembly;
		}
		else if (val == "none")
		{
		    r3_modify = QPDFWriter::r3m_none;
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
	    if (keylen == 128)
	    {
		r3_accessibility = result;
	    }
	    else
	    {
		usage("-accessibility invalid for 40-bit keys");
	    }
	}
	else
	{
	    usage(std::string("invalid encryption parameter --") + arg);
	}
    }
}

int main(int argc, char* argv[])
{
    whoami = QUtil::getWhoami(argv[0]);

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
	    << whoami << " version 2.1.a1" << std::endl
	    << "Copyright (c) 2005-2009 Jay Berkenbilt"
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

    char const* password = "";
    bool linearize = false;
    bool decrypt = false;

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
    QPDFWriter::r3_print_e r3_print = QPDFWriter::r3p_full;
    QPDFWriter::r3_modify_e r3_modify = QPDFWriter::r3m_all;

    bool stream_data_set = false;
    QPDFWriter::stream_data_e stream_data_mode = QPDFWriter::s_compress;
    bool normalize_set = false;
    bool normalize = false;
    bool suppress_recovery = false;
    bool object_stream_set = false;
    QPDFWriter::object_stream_e object_stream_mode = QPDFWriter::o_preserve;
    bool ignore_xref_streams = false;
    bool qdf_mode = false;
    std::string min_version;
    std::string force_version;

    bool static_id = false;
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
	    char* parameter = (char*)strchr(arg, '=');
	    if (parameter)
	    {
		*parameter++ = 0;
	    }

	    if (strcmp(arg, "password") == 0)
	    {
		if (parameter == 0)
		{
		    usage("--password must be given as --password=pass");
		}
		password = parameter;
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
		    r3_accessibility, r3_extract, r3_print, r3_modify);
		encrypt = true;
	    }
	    else if (strcmp(arg, "decrypt") == 0)
	    {
		decrypt = true;
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
		    stream_data_mode = QPDFWriter::s_compress;
		}
		else if (strcmp(parameter, "preserve") == 0)
		{
		    stream_data_mode = QPDFWriter::s_preserve;
		}
		else if (strcmp(parameter, "uncompress") == 0)
		{
		    stream_data_mode = QPDFWriter::s_uncompress;
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
		    object_stream_mode = QPDFWriter::o_disable;
		}
		else if (strcmp(parameter, "preserve") == 0)
		{
		    object_stream_mode = QPDFWriter::o_preserve;
		}
		else if (strcmp(parameter, "generate") == 0)
		{
		    object_stream_mode = QPDFWriter::o_generate;
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
	    else if (strcmp(arg, "static-id") == 0)
	    {
		static_id = true;
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
	if (ignore_xref_streams)
	{
	    pdf.setIgnoreXRefStreams(true);
	}
	if (suppress_recovery)
	{
	    pdf.setAttemptRecovery(false);
	}
	pdf.processFile(infilename, password);
	if (outfilename == 0)
	{
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
		pdf.showLinearizationData();
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
			    QTC::TC("qpdf", "unable to filter");
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
		bool okay = false;
		std::cout << "checking " << infilename << std::endl;
		try
		{
		    std::cout << "PDF Version: " << pdf.getPDFVersion()
			      << std::endl;
		    ::show_encryption(pdf);
		    if (pdf.isLinearized())
		    {
			std::cout << "File is linearized\n";
			okay = pdf.checkLinearization();
			// any errors are reported by checkLinearization().
		    }
		    else
		    {
			std::cout << "File is not linearized\n";
			// calling flattenScalarReferences causes full
			// traversal of file, so any structural errors
			// would be exposed.
			pdf.flattenScalarReferences();
			// Also explicitly decode all streams.
			pdf.decodeStreams();
			okay = true;
		    }
		}
		catch (std::exception& e)
		{
		    std::cout << e.what() << std::endl;
		}
		if (okay)
		{
		    if (! pdf.getWarnings().empty())
		    {
			exit(EXIT_WARNING);
		    }
		    else
		    {
			std::cout << "No errors found" << std::endl;
		    }
		}
	    }
	}
	else
	{
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
	    if (static_id)
	    {
		w.setStaticID(true);
	    }
	    if (suppress_original_object_id)
	    {
		w.setSuppressOriginalObjectIDs(true);
	    }
	    if (encrypt)
	    {
		if (keylen == 40)
		{
		    w.setR2EncryptionParameters(
			user_password.c_str(), owner_password.c_str(),
			r2_print, r2_modify, r2_extract, r2_annotate);
		}
		else if (keylen == 128)
		{
		    w.setR3EncryptionParameters(
			user_password.c_str(), owner_password.c_str(),
			r3_accessibility, r3_extract, r3_print, r3_modify);
		}
		else
		{
		    throw std::logic_error("bad encryption keylen");
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
		w.setMinimumPDFVersion(min_version);
	    }
	    if (! force_version.empty())
	    {
		w.forcePDFVersion(force_version);
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
