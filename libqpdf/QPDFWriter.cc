#include <qpdf/qpdf-config.h>  // include first for large file support
#include <qpdf/QPDFWriter.hh>

#include <assert.h>
#include <qpdf/Pl_StdioFile.hh>
#include <qpdf/Pl_Count.hh>
#include <qpdf/Pl_Discard.hh>
#include <qpdf/Pl_RC4.hh>
#include <qpdf/Pl_AES_PDF.hh>
#include <qpdf/Pl_Flate.hh>
#include <qpdf/Pl_PNGFilter.hh>
#include <qpdf/Pl_MD5.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/MD5.hh>
#include <qpdf/RC4.hh>
#include <qpdf/QTC.hh>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDF_Name.hh>
#include <qpdf/QPDF_String.hh>
#include <qpdf/QIntC.hh>

#include <algorithm>
#include <stdlib.h>

QPDFWriter::Members::Members(QPDF& pdf) :
    pdf(pdf),
    filename("unspecified"),
    file(0),
    close_file(false),
    buffer_pipeline(0),
    output_buffer(0),
    normalize_content_set(false),
    normalize_content(false),
    compress_streams(true),
    compress_streams_set(false),
    stream_decode_level(qpdf_dl_none),
    stream_decode_level_set(false),
    recompress_flate(false),
    qdf_mode(false),
    preserve_unreferenced_objects(false),
    newline_before_endstream(false),
    static_id(false),
    suppress_original_object_ids(false),
    direct_stream_lengths(true),
    encrypted(false),
    preserve_encryption(true),
    linearized(false),
    pclm(false),
    object_stream_mode(qpdf_o_preserve),
    encrypt_metadata(true),
    encrypt_use_aes(false),
    encryption_V(0),
    encryption_R(0),
    final_extension_level(0),
    min_extension_level(0),
    forced_extension_level(0),
    encryption_dict_objid(0),
    pipeline(0),
    next_objid(1),
    cur_stream_length_id(0),
    cur_stream_length(0),
    added_newline(false),
    max_ostream_index(0),
    next_stack_id(0),
    deterministic_id(false),
    md5_pipeline(0),
    did_write_setup(false),
    events_expected(0),
    events_seen(0),
    next_progress_report(0)
{
}

QPDFWriter::Members::~Members()
{
    if (file && close_file)
    {
	fclose(file);
    }
    delete output_buffer;
}

QPDFWriter::QPDFWriter(QPDF& pdf) :
    m(new Members(pdf))
{
}

QPDFWriter::QPDFWriter(QPDF& pdf, char const* filename) :
    m(new Members(pdf))
{
    setOutputFilename(filename);
}

QPDFWriter::QPDFWriter(QPDF& pdf, char const* description,
                       FILE *file, bool close_file) :
    m(new Members(pdf))
{
    setOutputFile(description, file, close_file);
}

QPDFWriter::~QPDFWriter()
{
}

void
QPDFWriter::setOutputFilename(char const* filename)
{
    char const* description = filename;
    FILE* f = 0;
    bool close_file = false;
    if (filename == 0)
    {
	description = "standard output";
	QTC::TC("qpdf", "QPDFWriter write to stdout");
	f = stdout;
	QUtil::binary_stdout();
    }
    else
    {
	QTC::TC("qpdf", "QPDFWriter write to file");
	f = QUtil::safe_fopen(filename, "wb+");
	close_file = true;
    }
    setOutputFile(description, f, close_file);
}

void
QPDFWriter::setOutputFile(char const* description, FILE* file, bool close_file)
{
    this->m->filename = description;
    this->m->file = file;
    this->m->close_file = close_file;
    Pipeline* p = new Pl_StdioFile("qpdf output", file);
    this->m->to_delete.push_back(p);
    initializePipelineStack(p);
}

void
QPDFWriter::setOutputMemory()
{
    this->m->filename = "memory buffer";
    this->m->buffer_pipeline = new Pl_Buffer("qpdf output");
    this->m->to_delete.push_back(this->m->buffer_pipeline);
    initializePipelineStack(this->m->buffer_pipeline);
}

Buffer*
QPDFWriter::getBuffer()
{
    Buffer* result = this->m->output_buffer;
    this->m->output_buffer = 0;
    return result;
}

void
QPDFWriter::setOutputPipeline(Pipeline* p)
{
    this->m->filename = "custom pipeline";
    initializePipelineStack(p);
}

void
QPDFWriter::setObjectStreamMode(qpdf_object_stream_e mode)
{
    this->m->object_stream_mode = mode;
}

void
QPDFWriter::setStreamDataMode(qpdf_stream_data_e mode)
{
    switch (mode)
    {
      case qpdf_s_uncompress:
        this->m->stream_decode_level =
            std::max(qpdf_dl_generalized, this->m->stream_decode_level);
        this->m->compress_streams = false;
        break;

      case qpdf_s_preserve:
        this->m->stream_decode_level = qpdf_dl_none;
        this->m->compress_streams = false;
        break;

      case qpdf_s_compress:
        this->m->stream_decode_level =
            std::max(qpdf_dl_generalized, this->m->stream_decode_level);
        this->m->compress_streams = true;
        break;
    }
    this->m->stream_decode_level_set = true;
    this->m->compress_streams_set = true;
}


void
QPDFWriter::setCompressStreams(bool val)
{
    this->m->compress_streams = val;
    this->m->compress_streams_set = true;
}

void
QPDFWriter::setDecodeLevel(qpdf_stream_decode_level_e val)
{
    this->m->stream_decode_level = val;
    this->m->stream_decode_level_set = true;
}

void
QPDFWriter::setRecompressFlate(bool val)
{
    this->m->recompress_flate = val;
}

void
QPDFWriter::setContentNormalization(bool val)
{
    this->m->normalize_content_set = true;
    this->m->normalize_content = val;
}

void
QPDFWriter::setQDFMode(bool val)
{
    this->m->qdf_mode = val;
}

void
QPDFWriter::setPreserveUnreferencedObjects(bool val)
{
    this->m->preserve_unreferenced_objects = val;
}

void
QPDFWriter::setNewlineBeforeEndstream(bool val)
{
    this->m->newline_before_endstream = val;
}

void
QPDFWriter::setMinimumPDFVersion(std::string const& version,
                                 int extension_level)
{
    bool set_version = false;
    bool set_extension_level = false;
    if (this->m->min_pdf_version.empty())
    {
	set_version = true;
        set_extension_level = true;
    }
    else
    {
	int old_major = 0;
	int old_minor = 0;
	int min_major = 0;
	int min_minor = 0;
	parseVersion(version, old_major, old_minor);
	parseVersion(this->m->min_pdf_version, min_major, min_minor);
        int compare = compareVersions(
            old_major, old_minor, min_major, min_minor);
	if (compare > 0)
	{
	    QTC::TC("qpdf", "QPDFWriter increasing minimum version",
                    extension_level == 0 ? 0 : 1);
	    set_version = true;
            set_extension_level = true;
	}
        else if (compare == 0)
        {
            if (extension_level > this->m->min_extension_level)
            {
                QTC::TC("qpdf", "QPDFWriter increasing extension level");
                set_extension_level = true;
            }
	}
    }

    if (set_version)
    {
	this->m->min_pdf_version = version;
    }
    if (set_extension_level)
    {
        this->m->min_extension_level = extension_level;
    }
}

void
QPDFWriter::forcePDFVersion(std::string const& version,
                            int extension_level)
{
    this->m->forced_pdf_version = version;
    this->m->forced_extension_level = extension_level;
}

void
QPDFWriter::setExtraHeaderText(std::string const& text)
{
    this->m->extra_header_text = text;
    if ((this->m->extra_header_text.length() > 0) &&
        (*(this->m->extra_header_text.rbegin()) != '\n'))
    {
        QTC::TC("qpdf", "QPDFWriter extra header text add newline");
        this->m->extra_header_text += "\n";
    }
    else
    {
        QTC::TC("qpdf", "QPDFWriter extra header text no newline");
    }
}

void
QPDFWriter::setStaticID(bool val)
{
    this->m->static_id = val;
}

void
QPDFWriter::setDeterministicID(bool val)
{
    this->m->deterministic_id = val;
}

void
QPDFWriter::setStaticAesIV(bool val)
{
    if (val)
    {
	Pl_AES_PDF::useStaticIV();
    }
}

void
QPDFWriter::setSuppressOriginalObjectIDs(bool val)
{
    this->m->suppress_original_object_ids = val;
}

void
QPDFWriter::setPreserveEncryption(bool val)
{
    this->m->preserve_encryption = val;
}

void
QPDFWriter::setLinearization(bool val)
{
    this->m->linearized = val;
    if (val)
    {
        this->m->pclm = false;
    }
}

void
QPDFWriter::setLinearizationPass1Filename(std::string const& filename)
{
    this->m->lin_pass1_filename = filename;
}

void
QPDFWriter::setPCLm(bool val)
{
    this->m->pclm = val;
    if (val)
    {
        this->m->linearized = false;
    }
}

void
QPDFWriter::setR2EncryptionParameters(
    char const* user_password, char const* owner_password,
    bool allow_print, bool allow_modify,
    bool allow_extract, bool allow_annotate)
{
    std::set<int> clear;
    if (! allow_print)
    {
	clear.insert(3);
    }
    if (! allow_modify)
    {
	clear.insert(4);
    }
    if (! allow_extract)
    {
	clear.insert(5);
    }
    if (! allow_annotate)
    {
	clear.insert(6);
    }

    setEncryptionParameters(user_password, owner_password, 1, 2, 5, clear);
}

void
QPDFWriter::setR3EncryptionParameters(
    char const* user_password, char const* owner_password,
    bool allow_accessibility, bool allow_extract,
    qpdf_r3_print_e print, qpdf_r3_modify_e modify)
{
    std::set<int> clear;
    interpretR3EncryptionParameters(
	clear, user_password, owner_password,
	allow_accessibility, allow_extract,
        true, true, true, true, print, modify);
    setEncryptionParameters(user_password, owner_password, 2, 3, 16, clear);
}

void
QPDFWriter::setR3EncryptionParameters(
    char const* user_password, char const* owner_password,
    bool allow_accessibility, bool allow_extract,
    bool allow_assemble, bool allow_annotate_and_form,
    bool allow_form_filling, bool allow_modify_other,
    qpdf_r3_print_e print)
{
    std::set<int> clear;
    interpretR3EncryptionParameters(
	clear, user_password, owner_password,
	allow_accessibility, allow_extract,
        allow_assemble, allow_annotate_and_form,
        allow_form_filling, allow_modify_other,
        print, qpdf_r3m_all);
    setEncryptionParameters(user_password, owner_password, 2, 3, 16, clear);
}

void
QPDFWriter::setR4EncryptionParameters(
    char const* user_password, char const* owner_password,
    bool allow_accessibility, bool allow_extract,
    qpdf_r3_print_e print, qpdf_r3_modify_e modify,
    bool encrypt_metadata, bool use_aes)
{
    std::set<int> clear;
    interpretR3EncryptionParameters(
	clear, user_password, owner_password,
	allow_accessibility, allow_extract,
        true, true, true, true, print, modify);
    this->m->encrypt_use_aes = use_aes;
    this->m->encrypt_metadata = encrypt_metadata;
    setEncryptionParameters(user_password, owner_password, 4, 4, 16, clear);
}

void
QPDFWriter::setR4EncryptionParameters(
    char const* user_password, char const* owner_password,
    bool allow_accessibility, bool allow_extract,
    bool allow_assemble, bool allow_annotate_and_form,
    bool allow_form_filling, bool allow_modify_other,
    qpdf_r3_print_e print,
    bool encrypt_metadata, bool use_aes)
{
    std::set<int> clear;
    interpretR3EncryptionParameters(
	clear, user_password, owner_password,
	allow_accessibility, allow_extract,
        allow_assemble, allow_annotate_and_form,
        allow_form_filling, allow_modify_other,
        print, qpdf_r3m_all);
    this->m->encrypt_use_aes = use_aes;
    this->m->encrypt_metadata = encrypt_metadata;
    setEncryptionParameters(user_password, owner_password, 4, 4, 16, clear);
}

void
QPDFWriter::setR5EncryptionParameters(
    char const* user_password, char const* owner_password,
    bool allow_accessibility, bool allow_extract,
    qpdf_r3_print_e print, qpdf_r3_modify_e modify,
    bool encrypt_metadata)
{
    std::set<int> clear;
    interpretR3EncryptionParameters(
	clear, user_password, owner_password,
	allow_accessibility, allow_extract,
        true, true, true, true, print, modify);
    this->m->encrypt_use_aes = true;
    this->m->encrypt_metadata = encrypt_metadata;
    setEncryptionParameters(user_password, owner_password, 5, 5, 32, clear);
}

void
QPDFWriter::setR5EncryptionParameters(
    char const* user_password, char const* owner_password,
    bool allow_accessibility, bool allow_extract,
    bool allow_assemble, bool allow_annotate_and_form,
    bool allow_form_filling, bool allow_modify_other,
    qpdf_r3_print_e print,
    bool encrypt_metadata)
{
    std::set<int> clear;
    interpretR3EncryptionParameters(
	clear, user_password, owner_password,
	allow_accessibility, allow_extract,
        allow_assemble, allow_annotate_and_form,
        allow_form_filling, allow_modify_other,
        print, qpdf_r3m_all);
    this->m->encrypt_use_aes = true;
    this->m->encrypt_metadata = encrypt_metadata;
    setEncryptionParameters(user_password, owner_password, 5, 5, 32, clear);
}

void
QPDFWriter::setR6EncryptionParameters(
    char const* user_password, char const* owner_password,
    bool allow_accessibility, bool allow_extract,
    qpdf_r3_print_e print, qpdf_r3_modify_e modify,
    bool encrypt_metadata)
{
    std::set<int> clear;
    interpretR3EncryptionParameters(
	clear, user_password, owner_password,
	allow_accessibility, allow_extract,
        true, true, true, true, print, modify);
    this->m->encrypt_use_aes = true;
    this->m->encrypt_metadata = encrypt_metadata;
    setEncryptionParameters(user_password, owner_password, 5, 6, 32, clear);
}

void
QPDFWriter::setR6EncryptionParameters(
    char const* user_password, char const* owner_password,
    bool allow_accessibility, bool allow_extract,
    bool allow_assemble, bool allow_annotate_and_form,
    bool allow_form_filling, bool allow_modify_other,
    qpdf_r3_print_e print,
    bool encrypt_metadata)
{
    std::set<int> clear;
    interpretR3EncryptionParameters(
	clear, user_password, owner_password,
	allow_accessibility, allow_extract,
        allow_assemble, allow_annotate_and_form,
        allow_form_filling, allow_modify_other,
        print, qpdf_r3m_all);
    this->m->encrypt_use_aes = true;
    this->m->encrypt_metadata = encrypt_metadata;
    setEncryptionParameters(user_password, owner_password, 5, 6, 32, clear);
}

void
QPDFWriter::interpretR3EncryptionParameters(
    std::set<int>& clear,
    char const* user_password, char const* owner_password,
    bool allow_accessibility, bool allow_extract,
    bool allow_assemble, bool allow_annotate_and_form,
    bool allow_form_filling, bool allow_modify_other,
    qpdf_r3_print_e print, qpdf_r3_modify_e modify)
{
    // Acrobat 5 security options:

    // Checkboxes:
    //   Enable Content Access for the Visually Impaired
    //   Allow Content Copying and Extraction

    // Allowed changes menu:
    //   None
    //   Only Document Assembly
    //   Only Form Field Fill-in or Signing
    //   Comment Authoring, Form Field Fill-in or Signing
    //   General Editing, Comment and Form Field Authoring

    // Allowed printing menu:
    //   None
    //   Low Resolution
    //   Full printing

    // Meanings of bits in P when R >= 3
    //
    //  3: low-resolution printing
    //  4: document modification except as controlled by 6, 9, and 11
    //  5: extraction
    //  6: add/modify annotations (comment), fill in forms
    //     if 4+6 are set, also allows modification of form fields
    //  9: fill in forms even if 6 is clear
    // 10: accessibility; ignored by readers, should always be set
    // 11: document assembly even if 4 is clear
    // 12: high-resolution printing

    if (! allow_accessibility)
    {
        // setEncryptionParameters sets this if R > 3
	clear.insert(10);
    }
    if (! allow_extract)
    {
	clear.insert(5);
    }

    // Note: these switch statements all "fall through" (no break
    // statements).  Each option clears successively more access bits.
    switch (print)
    {
      case qpdf_r3p_none:
	clear.insert(3);	// any printing

      case qpdf_r3p_low:
	clear.insert(12);	// high resolution printing

      case qpdf_r3p_full:
	break;

	// no default so gcc warns for missing cases
    }

    // Modify options. The qpdf_r3_modify_e options control groups of
    // bits and lack the full flexibility of the spec. This is
    // unfortunate, but it's been in the API for ages, and we're stuck
    // with it. See also allow checks below to control the bits
    // individually.

    // NOT EXERCISED IN TEST SUITE
    switch (modify)
    {
      case qpdf_r3m_none:
	clear.insert(11);	// document assembly

      case qpdf_r3m_assembly:
	clear.insert(9);	// filling in form fields

      case qpdf_r3m_form:
	clear.insert(6);	// modify annotations, fill in form fields

      case qpdf_r3m_annotate:
	clear.insert(4);	// other modifications

      case qpdf_r3m_all:
	break;

	// no default so gcc warns for missing cases
    }
    // END NOT EXERCISED IN TEST SUITE

    if (! allow_assemble)
    {
        clear.insert(11);
    }
    if (! allow_annotate_and_form)
    {
        clear.insert(6);
    }
    if (! allow_form_filling)
    {
        clear.insert(9);
    }
    if (! allow_modify_other)
    {
        clear.insert(4);
    }
}

void
QPDFWriter::setEncryptionParameters(
    char const* user_password, char const* owner_password,
    int V, int R, int key_len, std::set<int>& bits_to_clear)
{
    // PDF specification refers to bits with the low bit numbered 1.
    // We have to convert this into a bit field.

    // Specification always requires bits 1 and 2 to be cleared.
    bits_to_clear.insert(1);
    bits_to_clear.insert(2);

    if (R > 3)
    {
        // Bit 10 is deprecated and should always be set.  This used
        // to mean accessibility.  There is no way to disable
        // accessibility with R > 3.
        bits_to_clear.erase(10);
    }

    int P = 0;
    // Create the complement of P, then invert.
    for (std::set<int>::iterator iter = bits_to_clear.begin();
	 iter != bits_to_clear.end(); ++iter)
    {
	P |= (1 << ((*iter) - 1));
    }
    P = ~P;

    generateID();
    std::string O;
    std::string U;
    std::string OE;
    std::string UE;
    std::string Perms;
    std::string encryption_key;
    if (V < 5)
    {
        QPDF::compute_encryption_O_U(
            user_password, owner_password, V, R, key_len, P,
            this->m->encrypt_metadata, this->m->id1, O, U);
    }
    else
    {
        QPDF::compute_encryption_parameters_V5(
            user_password, owner_password, V, R, key_len, P,
            this->m->encrypt_metadata, this->m->id1,
            encryption_key, O, U, OE, UE, Perms);
    }
    setEncryptionParametersInternal(
	V, R, key_len, P, O, U, OE, UE, Perms,
        this->m->id1, user_password, encryption_key);
}

void
QPDFWriter::copyEncryptionParameters(QPDF& qpdf)
{
    this->m->preserve_encryption = false;
    QPDFObjectHandle trailer = qpdf.getTrailer();
    if (trailer.hasKey("/Encrypt"))
    {
        generateID();
        this->m->id1 =
            trailer.getKey("/ID").getArrayItem(0).getStringValue();
	QPDFObjectHandle encrypt = trailer.getKey("/Encrypt");
	int V = encrypt.getKey("/V").getIntValueAsInt();
	int key_len = 5;
	if (V > 1)
	{
	    key_len = encrypt.getKey("/Length").getIntValueAsInt() / 8;
	}
	if (encrypt.hasKey("/EncryptMetadata") &&
	    encrypt.getKey("/EncryptMetadata").isBool())
	{
	    this->m->encrypt_metadata =
		encrypt.getKey("/EncryptMetadata").getBoolValue();
	}
        if (V >= 4)
        {
            // When copying encryption parameters, use AES even if the
            // original file did not.  Acrobat doesn't create files
            // with V >= 4 that don't use AES, and the logic of
            // figuring out whether AES is used or not is complicated
            // with /StmF, /StrF, and /EFF all potentially having
            // different values.
            this->m->encrypt_use_aes = true;
        }
	QTC::TC("qpdf", "QPDFWriter copy encrypt metadata",
		this->m->encrypt_metadata ? 0 : 1);
        QTC::TC("qpdf", "QPDFWriter copy use_aes",
                this->m->encrypt_use_aes ? 0 : 1);
        std::string OE;
        std::string UE;
        std::string Perms;
        std::string encryption_key;
        if (V >= 5)
        {
            QTC::TC("qpdf", "QPDFWriter copy V5");
	    OE = encrypt.getKey("/OE").getStringValue();
            UE = encrypt.getKey("/UE").getStringValue();
	    Perms = encrypt.getKey("/Perms").getStringValue();
            encryption_key = qpdf.getEncryptionKey();
        }

	setEncryptionParametersInternal(
	    V,
	    encrypt.getKey("/R").getIntValueAsInt(),
    	    key_len,
	    encrypt.getKey("/P").getIntValueAsInt(),
	    encrypt.getKey("/O").getStringValue(),
	    encrypt.getKey("/U").getStringValue(),
            OE,
            UE,
            Perms,
	    this->m->id1,		// this->m->id1 == the other file's id1
	    qpdf.getPaddedUserPassword(),
            encryption_key);
    }
}

void
QPDFWriter::disableIncompatibleEncryption(int major, int minor,
                                          int extension_level)
{
    if (! this->m->encrypted)
    {
	return;
    }

    bool disable = false;
    if (compareVersions(major, minor, 1, 3) < 0)
    {
	disable = true;
    }
    else
    {
	int V = QUtil::string_to_int(
            this->m->encryption_dictionary["/V"].c_str());
	int R = QUtil::string_to_int(
            this->m->encryption_dictionary["/R"].c_str());
	if (compareVersions(major, minor, 1, 4) < 0)
	{
	    if ((V > 1) || (R > 2))
	    {
		disable = true;
	    }
	}
	else if (compareVersions(major, minor, 1, 5) < 0)
	{
	    if ((V > 2) || (R > 3))
	    {
		disable = true;
	    }
	}
	else if (compareVersions(major, minor, 1, 6) < 0)
	{
	    if (this->m->encrypt_use_aes)
	    {
		disable = true;
	    }
	}
        else if ((compareVersions(major, minor, 1, 7) < 0) ||
                 ((compareVersions(major, minor, 1, 7) == 0) &&
                  extension_level < 3))
        {
            if ((V >= 5) || (R >= 5))
            {
                disable = true;
            }
        }
    }
    if (disable)
    {
	QTC::TC("qpdf", "QPDFWriter forced version disabled encryption");
	this->m->encrypted = false;
    }
}

void
QPDFWriter::parseVersion(std::string const& version,
			 int& major, int& minor) const
{
    major = QUtil::string_to_int(version.c_str());
    minor = 0;
    size_t p = version.find('.');
    if ((p != std::string::npos) && (version.length() > p))
    {
	minor = QUtil::string_to_int(version.substr(p + 1).c_str());
    }
    std::string tmp = QUtil::int_to_string(major) + "." +
	QUtil::int_to_string(minor);
    if (tmp != version)
    {
        // The version number in the input is probably invalid. This
        // happens with some files that are designed to exercise bugs,
        // such as files in the fuzzer corpus. Unfortunately
        // QPDFWriter doesn't have a way to give a warning, so we just
        // ignore this case.
    }
}

int
QPDFWriter::compareVersions(int major1, int minor1,
			    int major2, int minor2) const
{
    if (major1 < major2)
    {
	return -1;
    }
    else if (major1 > major2)
    {
	return 1;
    }
    else if (minor1 < minor2)
    {
	return -1;
    }
    else if (minor1 > minor2)
    {
	return 1;
    }
    else
    {
	return 0;
    }
}

void
QPDFWriter::setEncryptionParametersInternal(
    int V, int R, int key_len, int P,
    std::string const& O, std::string const& U,
    std::string const& OE, std::string const& UE, std::string const& Perms,
    std::string const& id1, std::string const& user_password,
    std::string const& encryption_key)
{
    this->m->encryption_V = V;
    this->m->encryption_R = R;
    this->m->encryption_dictionary["/Filter"] = "/Standard";
    this->m->encryption_dictionary["/V"] = QUtil::int_to_string(V);
    this->m->encryption_dictionary["/Length"] =
        QUtil::int_to_string(key_len * 8);
    this->m->encryption_dictionary["/R"] = QUtil::int_to_string(R);
    this->m->encryption_dictionary["/P"] = QUtil::int_to_string(P);
    this->m->encryption_dictionary["/O"] = QPDF_String(O).unparse(true);
    this->m->encryption_dictionary["/U"] = QPDF_String(U).unparse(true);
    if (V >= 5)
    {
        this->m->encryption_dictionary["/OE"] = QPDF_String(OE).unparse(true);
        this->m->encryption_dictionary["/UE"] = QPDF_String(UE).unparse(true);
        this->m->encryption_dictionary["/Perms"] =
            QPDF_String(Perms).unparse(true);
    }
    if (R >= 6)
    {
        setMinimumPDFVersion("1.7", 8);
    }
    else if (R == 5)
    {
        setMinimumPDFVersion("1.7", 3);
    }
    else if (R == 4)
    {
        setMinimumPDFVersion(this->m->encrypt_use_aes ? "1.6" : "1.5");
    }
    else if (R == 3)
    {
        setMinimumPDFVersion("1.4");
    }
    else
    {
        setMinimumPDFVersion("1.3");
    }

    if ((R >= 4) && (! this->m->encrypt_metadata))
    {
	this->m->encryption_dictionary["/EncryptMetadata"] = "false";
    }
    if ((V == 4) || (V == 5))
    {
	// The spec says the value for the crypt filter key can be
	// anything, and xpdf seems to agree.  However, Adobe Reader
	// won't open our files unless we use /StdCF.
	this->m->encryption_dictionary["/StmF"] = "/StdCF";
	this->m->encryption_dictionary["/StrF"] = "/StdCF";
	std::string method = (this->m->encrypt_use_aes
                              ? ((V < 5) ? "/AESV2" : "/AESV3")
                              : "/V2");
        // The PDF spec says the /Length key is optional, but the PDF
        // previewer on some versions of MacOS won't open encrypted
        // files without it.
	this->m->encryption_dictionary["/CF"] =
	    "<< /StdCF << /AuthEvent /DocOpen /CFM " + method +
            " /Length " + std::string((V < 5) ? "16" : "32") + " >> >>";
    }

    this->m->encrypted = true;
    QPDF::EncryptionData encryption_data(
	V, R, key_len, P, O, U, OE, UE, Perms, id1, this->m->encrypt_metadata);
    if (V < 5)
    {
        this->m->encryption_key = QPDF::compute_encryption_key(
            user_password, encryption_data);
    }
    else
    {
        this->m->encryption_key = encryption_key;
    }
}

void
QPDFWriter::setDataKey(int objid)
{
    this->m->cur_data_key = QPDF::compute_data_key(
	this->m->encryption_key, objid, 0,
        this->m->encrypt_use_aes, this->m->encryption_V, this->m->encryption_R);
}

unsigned int
QPDFWriter::bytesNeeded(long long n)
{
    unsigned int bytes = 0;
    while (n)
    {
	++bytes;
	n >>= 8;
    }
    return bytes;
}

void
QPDFWriter::writeBinary(unsigned long long val, unsigned int bytes)
{
    if (bytes > sizeof(unsigned long long))
    {
        throw std::logic_error(
            "QPDFWriter::writeBinary called with too many bytes");
    }
    unsigned char data[sizeof(unsigned long long)];
    for (unsigned int i = 0; i < bytes; ++i)
    {
	data[bytes - i - 1] = static_cast<unsigned char>(val & 0xff);
	val >>= 8;
    }
    this->m->pipeline->write(data, bytes);
}

void
QPDFWriter::writeString(std::string const& str)
{
    this->m->pipeline->write(QUtil::unsigned_char_pointer(str), str.length());
}

void
QPDFWriter::writeBuffer(PointerHolder<Buffer>& b)
{
    this->m->pipeline->write(b->getBuffer(), b->getSize());
}

void
QPDFWriter::writeStringQDF(std::string const& str)
{
    if (this->m->qdf_mode)
    {
	writeString(str);
    }
}

void
QPDFWriter::writeStringNoQDF(std::string const& str)
{
    if (! this->m->qdf_mode)
    {
	writeString(str);
    }
}

void
QPDFWriter::writePad(int nspaces)
{
    for (int i = 0; i < nspaces; ++i)
    {
	writeString(" ");
    }
}

Pipeline*
QPDFWriter::pushPipeline(Pipeline* p)
{
    assert(dynamic_cast<Pl_Count*>(p) == 0);
    this->m->pipeline_stack.push_back(p);
    return p;
}

void
QPDFWriter::initializePipelineStack(Pipeline *p)
{
    this->m->pipeline = new Pl_Count("pipeline stack base", p);
    this->m->to_delete.push_back(this->m->pipeline);
    this->m->pipeline_stack.push_back(this->m->pipeline);
}

void
QPDFWriter::activatePipelineStack(PipelinePopper& pp)
{
    std::string stack_id(
        "stack " + QUtil::uint_to_string(this->m->next_stack_id));
    Pl_Count* c = new Pl_Count(stack_id.c_str(),
                               this->m->pipeline_stack.back());
    ++this->m->next_stack_id;
    this->m->pipeline_stack.push_back(c);
    this->m->pipeline = c;
    pp.stack_id = stack_id;
}

QPDFWriter::PipelinePopper::~PipelinePopper()
{
    if (stack_id.empty())
    {
        return;
    }
    assert(qw->m->pipeline_stack.size() >= 2);
    qw->m->pipeline->finish();
    assert(dynamic_cast<Pl_Count*>(qw->m->pipeline_stack.back()) ==
	   qw->m->pipeline);
    // It might be possible for this assertion to fail if
    // writeLinearized exits by exception when deterministic ID, but I
    // don't think so. As of this writing, this is the only case in
    // which two dynamically allocated PipelinePopper objects ever
    // exist at the same time, so the assertion will fail if they get
    // popped out of order from automatic destruction.
    assert(qw->m->pipeline->getIdentifier() == stack_id);
    delete qw->m->pipeline_stack.back();
    qw->m->pipeline_stack.pop_back();
    while (dynamic_cast<Pl_Count*>(qw->m->pipeline_stack.back()) == 0)
    {
	Pipeline* p = qw->m->pipeline_stack.back();
        if (dynamic_cast<Pl_MD5*>(p) == qw->m->md5_pipeline)
        {
            qw->m->md5_pipeline = 0;
        }
	qw->m->pipeline_stack.pop_back();
	Pl_Buffer* buf = dynamic_cast<Pl_Buffer*>(p);
	if (bp && buf)
	{
	    *bp = buf->getBuffer();
	}
	delete p;
    }
    qw->m->pipeline = dynamic_cast<Pl_Count*>(qw->m->pipeline_stack.back());
}

void
QPDFWriter::adjustAESStreamLength(size_t& length)
{
    if (this->m->encrypted && (! this->m->cur_data_key.empty()) &&
	this->m->encrypt_use_aes)
    {
	// Stream length will be padded with 1 to 16 bytes to end up
	// as a multiple of 16.  It will also be prepended by 16 bits
	// of random data.
	length += 32 - (length & 0xf);
    }
}

void
QPDFWriter::pushEncryptionFilter(PipelinePopper& pp)
{
    if (this->m->encrypted && (! this->m->cur_data_key.empty()))
    {
	Pipeline* p = 0;
	if (this->m->encrypt_use_aes)
	{
	    p = new Pl_AES_PDF(
		"aes stream encryption", this->m->pipeline, true,
		QUtil::unsigned_char_pointer(this->m->cur_data_key),
                this->m->cur_data_key.length());
	}
	else
	{
	    p = new Pl_RC4("rc4 stream encryption", this->m->pipeline,
			   QUtil::unsigned_char_pointer(this->m->cur_data_key),
			   QIntC::to_int(this->m->cur_data_key.length()));
	}
	pushPipeline(p);
    }
    // Must call this unconditionally so we can call popPipelineStack
    // to balance pushEncryptionFilter().
    activatePipelineStack(pp);
}

void
QPDFWriter::pushDiscardFilter(PipelinePopper& pp)
{
    pushPipeline(new Pl_Discard());
    activatePipelineStack(pp);
}

void
QPDFWriter::pushMD5Pipeline(PipelinePopper& pp)
{
    if (! this->m->id2.empty())
    {
        // Can't happen in the code
        throw std::logic_error(
            "Deterministic ID computation enabled after ID"
            " generation has already occurred.");
    }
    assert(this->m->deterministic_id);
    assert(this->m->md5_pipeline == 0);
    assert(this->m->pipeline->getCount() == 0);
    this->m->md5_pipeline = new Pl_MD5("qpdf md5", this->m->pipeline);
    this->m->md5_pipeline->persistAcrossFinish(true);
    // Special case code in popPipelineStack clears this->m->md5_pipeline
    // upon deletion.
    pushPipeline(this->m->md5_pipeline);
    activatePipelineStack(pp);
}

void
QPDFWriter::computeDeterministicIDData()
{
    assert(this->m->md5_pipeline != 0);
    assert(this->m->deterministic_id_data.empty());
    this->m->deterministic_id_data = this->m->md5_pipeline->getHexDigest();
    this->m->md5_pipeline->enable(false);
}

int
QPDFWriter::openObject(int objid)
{
    if (objid == 0)
    {
	objid = this->m->next_objid++;
    }
    this->m->xref[objid] = QPDFXRefEntry(1, this->m->pipeline->getCount(), 0);
    writeString(QUtil::int_to_string(objid));
    writeString(" 0 obj\n");
    return objid;
}

void
QPDFWriter::closeObject(int objid)
{
    // Write a newline before endobj as it makes the file easier to
    // repair.
    writeString("\nendobj\n");
    writeStringQDF("\n");
    this->m->lengths[objid] = this->m->pipeline->getCount() -
        this->m->xref[objid].getOffset();
}

void
QPDFWriter::assignCompressedObjectNumbers(QPDFObjGen const& og)
{
    int objid = og.getObj();
    if ((og.getGen() != 0) ||
        (this->m->object_stream_to_objects.count(objid) == 0))
    {
        // This is not an object stream.
	return;
    }

    // Reserve numbers for the objects that belong to this object
    // stream.
    for (std::set<QPDFObjGen>::iterator iter =
	     this->m->object_stream_to_objects[objid].begin();
	 iter != this->m->object_stream_to_objects[objid].end();
	 ++iter)
    {
	this->m->obj_renumber[*iter] = this->m->next_objid++;
    }
}

void
QPDFWriter::enqueueObject(QPDFObjectHandle object)
{
    if (object.isIndirect())
    {
        if (object.getOwningQPDF() != &(this->m->pdf))
        {
            QTC::TC("qpdf", "QPDFWriter foreign object");
            throw std::logic_error(
                "QPDFObjectHandle from different QPDF found while writing."
                "  Use QPDF::copyForeignObject to add objects from"
                " another file.");
        }

	QPDFObjGen og = object.getObjGen();

	if (this->m->obj_renumber.count(og) == 0)
	{
	    if (this->m->object_to_object_stream.count(og))
	    {
		// This is in an object stream.  Don't process it
		// here.  Instead, enqueue the object stream.  Object
		// streams always have generation 0.
		int stream_id = this->m->object_to_object_stream[og];
                // Detect loops by storing invalid object ID 0, which
                // will get overwritten later.
                this->m->obj_renumber[og] = 0;
		enqueueObject(this->m->pdf.getObjectByID(stream_id, 0));
	    }
	    else
	    {
		this->m->object_queue.push_back(object);
		this->m->obj_renumber[og] = this->m->next_objid++;

		if ((og.getGen() == 0) &&
                    this->m->object_stream_to_objects.count(og.getObj()))
		{
		    // For linearized files, uncompressed objects go
		    // at end, and we take care of assigning numbers
		    // to them elsewhere.
		    if (! this->m->linearized)
		    {
			assignCompressedObjectNumbers(og);
		    }
		}
		else if ((! this->m->direct_stream_lengths) &&
                         object.isStream())
		{
		    // reserve next object ID for length
		    ++this->m->next_objid;
		}
	    }
	}
        else if (this->m->obj_renumber[og] == 0)
        {
            // This can happen if a specially constructed file
            // indicates that an object stream is inside itself.
            QTC::TC("qpdf", "QPDFWriter ignore self-referential object stream");
        }
    }
    else if (object.isArray())
    {
	int n = object.getArrayNItems();
	for (int i = 0; i < n; ++i)
	{
	    if (! this->m->linearized)
	    {
		enqueueObject(object.getArrayItem(i));
	    }
	}
    }
    else if (object.isDictionary())
    {
	std::set<std::string> keys = object.getKeys();
	for (std::set<std::string>::iterator iter = keys.begin();
	     iter != keys.end(); ++iter)
	{
	    if (! this->m->linearized)
	    {
		enqueueObject(object.getKey(*iter));
	    }
	}
    }
    else
    {
	// ignore
    }
}

void
QPDFWriter::unparseChild(QPDFObjectHandle child, int level, int flags)
{
    if (! this->m->linearized)
    {
	enqueueObject(child);
    }
    if (child.isIndirect())
    {
	QPDFObjGen old_og = child.getObjGen();
	int new_id = this->m->obj_renumber[old_og];
	writeString(QUtil::int_to_string(new_id));
	writeString(" 0 R");
    }
    else
    {
	unparseObject(child, level, flags);
    }
}

void
QPDFWriter::writeTrailer(trailer_e which, int size, bool xref_stream,
                         qpdf_offset_t prev, int linearization_pass)
{
    QPDFObjectHandle trailer = getTrimmedTrailer();
    if (! xref_stream)
    {
	writeString("trailer <<");
    }
    writeStringQDF("\n");
    if (which == t_lin_second)
    {
	writeString(" /Size ");
	writeString(QUtil::int_to_string(size));
    }
    else
    {
	std::set<std::string> keys = trailer.getKeys();
	for (std::set<std::string>::iterator iter = keys.begin();
	     iter != keys.end(); ++iter)
	{
	    std::string const& key = *iter;
	    writeStringQDF("  ");
	    writeStringNoQDF(" ");
	    writeString(QPDF_Name::normalizeName(key));
	    writeString(" ");
	    if (key == "/Size")
	    {
		writeString(QUtil::int_to_string(size));
		if (which == t_lin_first)
		{
		    writeString(" /Prev ");
		    qpdf_offset_t pos = this->m->pipeline->getCount();
		    writeString(QUtil::int_to_string(prev));
		    int nspaces =
                        QIntC::to_int(pos - this->m->pipeline->getCount() + 21);
		    if (nspaces < 0)
                    {
                        throw std::logic_error(
                            "QPDFWriter: no padding required in trailer");
                    }
		    writePad(nspaces);
		}
	    }
	    else
	    {
		unparseChild(trailer.getKey(key), 1, 0);
	    }
	    writeStringQDF("\n");
	}
    }

    // Write ID
    writeStringQDF(" ");
    writeString(" /ID [");
    if (linearization_pass == 1)
    {
	std::string original_id1 = getOriginalID1();
        if (original_id1.empty())
        {
            writeString("<00000000000000000000000000000000>");
        }
        else
        {
            // Write a string of zeroes equal in length to the
            // representation of the original ID. While writing the
            // original ID would have the same number of bytes, it
            // would cause a change to the deterministic ID generated
            // by older versions of the software that hard-coded the
            // length of the ID to 16 bytes.
            writeString("<");
            size_t len = QPDF_String(original_id1).unparse(true).length() - 2;
            for (size_t i = 0; i < len; ++i)
            {
                writeString("0");
            }
            writeString(">");
        }
        writeString("<00000000000000000000000000000000>");
    }
    else
    {
        if ((linearization_pass == 0) && (this->m->deterministic_id))
        {
            computeDeterministicIDData();
        }
        generateID();
        writeString(QPDF_String(this->m->id1).unparse(true));
        writeString(QPDF_String(this->m->id2).unparse(true));
    }
    writeString("]");

    if (which != t_lin_second)
    {
	// Write reference to encryption dictionary
	if (this->m->encrypted)
	{
	    writeString(" /Encrypt ");
	    writeString(QUtil::int_to_string(this->m->encryption_dict_objid));
	    writeString(" 0 R");
	}
    }

    writeStringQDF("\n");
    writeStringNoQDF(" ");
    writeString(">>");
}

void
QPDFWriter::unparseObject(QPDFObjectHandle object, int level,
			  int flags, size_t stream_length,
                          bool compress)
{
    QPDFObjGen old_og = object.getObjGen();
    int child_flags = flags & ~f_stream;

    std::string indent;
    for (int i = 0; i < level; ++i)
    {
	indent += "  ";
    }

    if (object.isArray())
    {
	// Note: PDF spec 1.4 implementation note 121 states that
	// Acrobat requires a space after the [ in the /H key of the
	// linearization parameter dictionary.  We'll do this
	// unconditionally for all arrays because it looks nicer and
	// doesn't make the files that much bigger.
	writeString("[");
	writeStringQDF("\n");
	int n = object.getArrayNItems();
	for (int i = 0; i < n; ++i)
	{
	    writeStringQDF(indent);
	    writeStringQDF("  ");
	    writeStringNoQDF(" ");
	    unparseChild(object.getArrayItem(i), level + 1, child_flags);
	    writeStringQDF("\n");
	}
	writeStringQDF(indent);
	writeStringNoQDF(" ");
	writeString("]");
    }
    else if (object.isDictionary())
    {
        // Make a shallow copy of this object so we can modify it
        // safely without affecting the original.  This code makes
        // assumptions about things that are made true in
        // prepareFileForWrite, such as that certain things are direct
        // objects so that replacing them doesn't leave unreferenced
        // objects in the output.
        object = object.shallowCopy();

        // Handle special cases for specific dictionaries.

        // Extensions dictionaries.

        // We have one of several cases:
        //
        // * We need ADBE
        //    - We already have Extensions
        //       - If it has the right ADBE, preserve it
        //       - Otherwise, replace ADBE
        //    - We don't have Extensions: create one from scratch
        // * We don't want ADBE
        //    - We already have Extensions
        //       - If it only has ADBE, remove it
        //       - If it has other things, keep those and remove ADBE
        //    - We have no extensions: no action required
        //
        // Before writing, we guarantee that /Extensions, if present,
        // is direct through the ADBE dictionary, so we can modify in
        // place.

        bool is_root = false;
        bool have_extensions_other = false;
        bool have_extensions_adbe = false;

        QPDFObjectHandle extensions;
        if (old_og == this->m->pdf.getRoot().getObjGen())
        {
            is_root = true;
            if (object.hasKey("/Extensions") &&
                object.getKey("/Extensions").isDictionary())
            {
                extensions = object.getKey("/Extensions");
            }
        }

        if (extensions.isInitialized())
        {
            std::set<std::string> keys = extensions.getKeys();
            if (keys.count("/ADBE") > 0)
            {
                have_extensions_adbe = true;
                keys.erase("/ADBE");
            }
            if (keys.size() > 0)
            {
                have_extensions_other = true;
            }
        }

        bool need_extensions_adbe = (this->m->final_extension_level > 0);

        if (is_root)
        {
            if (need_extensions_adbe)
            {
                if (! (have_extensions_other || have_extensions_adbe))
                {
                    // We need Extensions and don't have it.  Create
                    // it here.
                    QTC::TC("qpdf", "QPDFWriter create Extensions",
                            this->m->qdf_mode ? 0 : 1);
                    extensions = QPDFObjectHandle::newDictionary();
                    object.replaceKey("/Extensions", extensions);
                }
            }
            else if (! have_extensions_other)
            {
                // We have Extensions dictionary and don't want one.
                if (have_extensions_adbe)
                {
                    QTC::TC("qpdf", "QPDFWriter remove existing Extensions");
                    object.removeKey("/Extensions");
                    extensions = QPDFObjectHandle(); // uninitialized
                }
            }
        }

        if (extensions.isInitialized())
        {
            QTC::TC("qpdf", "QPDFWriter preserve Extensions");
            QPDFObjectHandle adbe = extensions.getKey("/ADBE");
            if (adbe.isDictionary() &&
                adbe.hasKey("/BaseVersion") &&
                adbe.getKey("/BaseVersion").isName() &&
                (adbe.getKey("/BaseVersion").getName() ==
                 "/" + this->m->final_pdf_version) &&
                adbe.hasKey("/ExtensionLevel") &&
                adbe.getKey("/ExtensionLevel").isInteger() &&
                (adbe.getKey("/ExtensionLevel").getIntValue() ==
                 this->m->final_extension_level))
            {
                QTC::TC("qpdf", "QPDFWriter preserve ADBE");
            }
            else
            {
                if (need_extensions_adbe)
                {
                    extensions.replaceKey(
                        "/ADBE",
                        QPDFObjectHandle::parse(
                            "<< /BaseVersion /" + this->m->final_pdf_version +
                            " /ExtensionLevel " +
                            QUtil::int_to_string(
                                this->m->final_extension_level) +
                            " >>"));
                }
                else
                {
                    QTC::TC("qpdf", "QPDFWriter remove ADBE");
                    extensions.removeKey("/ADBE");
                }
            }
        }

        // Stream dictionaries.

        if (flags & f_stream)
        {
            // Suppress /Length since we will write it manually
            object.removeKey("/Length");

            // If /DecodeParms is an empty list, remove it.
            if (object.getKey("/DecodeParms").isArray() &&
                (0 == object.getKey("/DecodeParms").getArrayNItems()))
            {
                QTC::TC("qpdf", "QPDFWriter remove empty DecodeParms");
                object.removeKey("/DecodeParms");
            }

	    if (flags & f_filtered)
            {
                // We will supply our own filter and decode
                // parameters.
                object.removeKey("/Filter");
                object.removeKey("/DecodeParms");
            }
            else
            {
                // Make sure, no matter what else we have, that we
                // don't have /Crypt in the output filters.
                QPDFObjectHandle filter = object.getKey("/Filter");
                QPDFObjectHandle decode_parms = object.getKey("/DecodeParms");
                if (filter.isOrHasName("/Crypt"))
                {
                    if (filter.isName())
                    {
                        object.removeKey("/Filter");
                        object.removeKey("/DecodeParms");
                    }
                    else
                    {
                        int idx = -1;
                        for (int i = 0; i < filter.getArrayNItems(); ++i)
                        {
                            QPDFObjectHandle item = filter.getArrayItem(i);
                            if (item.isName() && item.getName() == "/Crypt")
                            {
                                idx = i;
                                break;
                            }
                        }
                        if (idx >= 0)
                        {
                            // If filter is an array, then the code in
                            // QPDF_Stream has already verified that
                            // DecodeParms and Filters are arrays of
                            // the same length, but if they weren't
                            // for some reason, eraseItem does type
                            // and bounds checking.
                            QTC::TC("qpdf", "QPDFWriter remove Crypt");
                            filter.eraseItem(idx);
                            decode_parms.eraseItem(idx);
                        }
                    }
                }
            }
        }

	writeString("<<");
	writeStringQDF("\n");

	std::set<std::string> keys = object.getKeys();
	for (std::set<std::string>::iterator iter = keys.begin();
	     iter != keys.end(); ++iter)
	{
	    std::string const& key = *iter;

	    writeStringQDF(indent);
	    writeStringQDF("  ");
	    writeStringNoQDF(" ");
	    writeString(QPDF_Name::normalizeName(key));
	    writeString(" ");
            unparseChild(object.getKey(key), level + 1, child_flags);
	    writeStringQDF("\n");
	}

	if (flags & f_stream)
	{
	    writeStringQDF(indent);
	    writeStringQDF(" ");
	    writeString(" /Length ");

	    if (this->m->direct_stream_lengths)
	    {
		writeString(QUtil::uint_to_string(stream_length));
	    }
	    else
	    {
		writeString(
		    QUtil::int_to_string(this->m->cur_stream_length_id));
		writeString(" 0 R");
	    }
	    writeStringQDF("\n");
	    if (compress && (flags & f_filtered))
	    {
		writeStringQDF(indent);
		writeStringQDF(" ");
		writeString(" /Filter /FlateDecode");
		writeStringQDF("\n");
	    }
	}

	writeStringQDF(indent);
	writeStringNoQDF(" ");
	writeString(">>");
    }
    else if (object.isStream())
    {
	// Write stream data to a buffer.
	int new_id = this->m->obj_renumber[old_og];
	if (! this->m->direct_stream_lengths)
	{
	    this->m->cur_stream_length_id = new_id + 1;
	}
	QPDFObjectHandle stream_dict = object.getDict();

	bool is_metadata = false;
	if (stream_dict.getKey("/Type").isName() &&
	    (stream_dict.getKey("/Type").getName() == "/Metadata"))
	{
	    is_metadata = true;
	}
	bool filter = (object.isDataModified() ||
                       this->m->compress_streams ||
                       this->m->stream_decode_level);
	if (this->m->compress_streams)
	{
	    // Don't filter if the stream is already compressed with
	    // FlateDecode. This way we don't make it worse if the
	    // original file used a better Flate algorithm, and we
	    // don't spend time and CPU cycles uncompressing and
	    // recompressing stuff. This can be overridden with
	    // setRecompressFlate(true).
	    QPDFObjectHandle filter_obj = stream_dict.getKey("/Filter");
            if ((! this->m->recompress_flate) &&
                (! object.isDataModified()) &&
                filter_obj.isName() &&
		((filter_obj.getName() == "/FlateDecode") ||
		 (filter_obj.getName() == "/Fl")))
	    {
		QTC::TC("qpdf", "QPDFWriter not recompressing /FlateDecode");
		filter = false;
	    }
	}
	bool normalize = false;
	bool compress = false;
        bool uncompress = false;
	if (is_metadata &&
	    ((! this->m->encrypted) || (this->m->encrypt_metadata == false)))
	{
	    QTC::TC("qpdf", "QPDFWriter not compressing metadata");
	    filter = true;
	    compress = false;
            uncompress = true;
	}
	else if (this->m->normalize_content &&
                 this->m->normalized_streams.count(old_og))
	{
	    normalize = true;
	    filter = true;
	}
	else if (filter && this->m->compress_streams)
	{
	    compress = true;
	    QTC::TC("qpdf", "QPDFWriter compressing uncompressed stream");
	}

	flags |= f_stream;

        PointerHolder<Buffer> stream_data;
        bool filtered = false;
        for (int attempt = 1; attempt <= 2; ++attempt)
        {
            pushPipeline(new Pl_Buffer("stream data"));
            PipelinePopper pp_stream_data(this, &stream_data);
            activatePipelineStack(pp_stream_data);
            filtered =
                object.pipeStreamData(
                    this->m->pipeline,
                    (((filter && normalize) ? qpdf_ef_normalize : 0) |
                     ((filter && compress) ? qpdf_ef_compress : 0)),
                    (filter
                     ? (uncompress ? qpdf_dl_all : this->m->stream_decode_level)
                     : qpdf_dl_none), false, (attempt == 1));
            if (filter && (! filtered))
            {
                // Try again
                filter = false;
            }
            else
            {
                break;
            }
        }
	if (filtered)
	{
	    flags |= f_filtered;
	}
	else
	{
	    compress = false;
	}

	this->m->cur_stream_length = stream_data->getSize();
	if (is_metadata && this->m->encrypted && (! this->m->encrypt_metadata))
	{
	    // Don't encrypt stream data for the metadata stream
	    this->m->cur_data_key.clear();
	}
	adjustAESStreamLength(this->m->cur_stream_length);
	unparseObject(stream_dict, 0, flags,
                      this->m->cur_stream_length, compress);
	unsigned char last_char = '\0';
	writeString("\nstream\n");
        {
            PipelinePopper pp_enc(this);
            pushEncryptionFilter(pp_enc);
            writeBuffer(stream_data);
            last_char = this->m->pipeline->getLastChar();
        }

        if (this->m->newline_before_endstream ||
            (this->m->qdf_mode && (last_char != '\n')))
        {
            writeString("\n");
            this->m->added_newline = true;
        }
        else
        {
            this->m->added_newline = false;
        }
	writeString("endstream");
    }
    else if (object.isString())
    {
	std::string val;
	if (this->m->encrypted &&
	    (! (flags & f_in_ostream)) &&
	    (! this->m->cur_data_key.empty()))
	{
	    val = object.getStringValue();
	    if (this->m->encrypt_use_aes)
	    {
		Pl_Buffer bufpl("encrypted string");
		Pl_AES_PDF pl(
                    "aes encrypt string", &bufpl, true,
                    QUtil::unsigned_char_pointer(this->m->cur_data_key),
                    this->m->cur_data_key.length());
		pl.write(QUtil::unsigned_char_pointer(val), val.length());
		pl.finish();
		PointerHolder<Buffer> buf = bufpl.getBuffer();
		val = QPDF_String(
		    std::string(reinterpret_cast<char*>(buf->getBuffer()),
				buf->getSize())).unparse(true);
	    }
	    else
	    {
		PointerHolder<char> tmp_ph =
                    PointerHolder<char>(true, QUtil::copy_string(val));
                char* tmp = tmp_ph.getPointer();
		size_t vlen = val.length();
		RC4 rc4(QUtil::unsigned_char_pointer(this->m->cur_data_key),
			QIntC::to_int(this->m->cur_data_key.length()));
		rc4.process(QUtil::unsigned_char_pointer(tmp), vlen);
		val = QPDF_String(std::string(tmp, vlen)).unparse();
	    }
	}
	else
	{
	    val = object.unparseResolved();
	}
	writeString(val);
    }
    else
    {
	writeString(object.unparseResolved());
    }
}

void
QPDFWriter::writeObjectStreamOffsets(std::vector<qpdf_offset_t>& offsets,
				     int first_obj)
{
    for (size_t i = 0; i < offsets.size(); ++i)
    {
	if (i != 0)
	{
	    writeStringQDF("\n");
	    writeStringNoQDF(" ");
	}
	writeString(QUtil::uint_to_string(i + QIntC::to_size(first_obj)));
	writeString(" ");
	writeString(QUtil::int_to_string(offsets.at(i)));
    }
    writeString("\n");
}

void
QPDFWriter::writeObjectStream(QPDFObjectHandle object)
{
    // Note: object might be null if this is a place-holder for an
    // object stream that we are generating from scratch.

    QPDFObjGen old_og = object.getObjGen();
    assert(old_og.getGen() == 0);
    int old_id = old_og.getObj();
    int new_id = this->m->obj_renumber[old_og];

    std::vector<qpdf_offset_t> offsets;
    qpdf_offset_t first = 0;

    // Generate stream itself.  We have to do this in two passes so we
    // can calculate offsets in the first pass.
    PointerHolder<Buffer> stream_buffer;
    int first_obj = -1;
    bool compressed = false;
    for (int pass = 1; pass <= 2; ++pass)
    {
	// stream_buffer will be initialized only for pass 2
        PipelinePopper pp_ostream(this, &stream_buffer);
	if (pass == 1)
	{
	    pushDiscardFilter(pp_ostream);
	}
	else
	{
	    // Adjust offsets to skip over comment before first object

	    first = offsets.at(0);
	    for (std::vector<qpdf_offset_t>::iterator iter = offsets.begin();
		 iter != offsets.end(); ++iter)
	    {
		*iter -= first;
	    }

	    // Take one pass at writing pairs of numbers so we can get
	    // their size information
            {
                PipelinePopper pp_discard(this);
                pushDiscardFilter(pp_discard);
                writeObjectStreamOffsets(offsets, first_obj);
                first += this->m->pipeline->getCount();
            }

	    // Set up a stream to write the stream data into a buffer.
	    Pipeline* next = pushPipeline(new Pl_Buffer("object stream"));
            if ((this->m->compress_streams ||
                 (this->m->stream_decode_level == qpdf_dl_none)) &&
                (! this->m->qdf_mode))
	    {
		compressed = true;
		next = pushPipeline(
		    new Pl_Flate("compress object stream", next,
				 Pl_Flate::a_deflate));
	    }
	    activatePipelineStack(pp_ostream);
	    writeObjectStreamOffsets(offsets, first_obj);
	}

	int count = 0;
	for (std::set<QPDFObjGen>::iterator iter =
		 this->m->object_stream_to_objects[old_id].begin();
	     iter != this->m->object_stream_to_objects[old_id].end();
	     ++iter, ++count)
	{
	    QPDFObjGen obj = *iter;
	    int new_obj = this->m->obj_renumber[obj];
	    if (first_obj == -1)
	    {
		first_obj = new_obj;
	    }
	    if (this->m->qdf_mode)
	    {
		writeString("%% Object stream: object " +
			    QUtil::int_to_string(new_obj) + ", index " +
			    QUtil::int_to_string(count));
		if (! this->m->suppress_original_object_ids)
		{
		    writeString("; original object ID: " +
				QUtil::int_to_string(obj.getObj()));
                    // For compatibility, only write the generation if
                    // non-zero.  While object streams only allow
                    // objects with generation 0, if we are generating
                    // object streams, the old object could have a
                    // non-zero generation.
                    if (obj.getGen() != 0)
                    {
                        QTC::TC("qpdf", "QPDFWriter original obj non-zero gen");
                        writeString(" " + QUtil::int_to_string(obj.getGen()));
                    }
		}
		writeString("\n");
	    }
	    if (pass == 1)
	    {
		offsets.push_back(this->m->pipeline->getCount());
                // To avoid double-counting objects being written in
                // object streams for progress reporting, decrement in
                // pass 1.
                indicateProgress(true, false);
	    }
            QPDFObjectHandle obj_to_write =
                this->m->pdf.getObjectByObjGen(obj);
            if (obj_to_write.isStream())
            {
                // This condition occurred in a fuzz input. Ideally we
                // should block it at at parse time, but it's not
                // clear to me how to construct a case for this.
                QTC::TC("qpdf", "QPDFWriter stream in ostream");
                obj_to_write.warnIfPossible(
                    "stream found inside object stream; treating as null");
                obj_to_write = QPDFObjectHandle::newNull();
            }
	    writeObject(obj_to_write, count);

	    this->m->xref[new_obj] = QPDFXRefEntry(2, new_id, count);
	}
    }

    // Write the object
    openObject(new_id);
    setDataKey(new_id);
    writeString("<<");
    writeStringQDF("\n ");
    writeString(" /Type /ObjStm");
    writeStringQDF("\n ");
    size_t length = stream_buffer->getSize();
    adjustAESStreamLength(length);
    writeString(" /Length " + QUtil::uint_to_string(length));
    writeStringQDF("\n ");
    if (compressed)
    {
	writeString(" /Filter /FlateDecode");
    }
    writeString(" /N " + QUtil::uint_to_string(offsets.size()));
    writeStringQDF("\n ");
    writeString(" /First " + QUtil::int_to_string(first));
    if (! object.isNull())
    {
	// If the original object has an /Extends key, preserve it.
	QPDFObjectHandle dict = object.getDict();
	QPDFObjectHandle extends = dict.getKey("/Extends");
	if (extends.isIndirect())
	{
	    QTC::TC("qpdf", "QPDFWriter copy Extends");
	    writeStringQDF("\n ");
	    writeString(" /Extends ");
	    unparseChild(extends, 1, f_in_ostream);
	}
    }
    writeStringQDF("\n");
    writeStringNoQDF(" ");
    writeString(">>\nstream\n");
    if (this->m->encrypted)
    {
	QTC::TC("qpdf", "QPDFWriter encrypt object stream");
    }
    {
        PipelinePopper pp_enc(this);
        pushEncryptionFilter(pp_enc);
        writeBuffer(stream_buffer);
    }
    if (this->m->newline_before_endstream)
    {
        writeString("\n");
    }
    writeString("endstream");
    this->m->cur_data_key.clear();
    closeObject(new_id);
}

void
QPDFWriter::writeObject(QPDFObjectHandle object, int object_stream_index)
{
    QPDFObjGen old_og = object.getObjGen();

    if ((object_stream_index == -1) &&
        (old_og.getGen() == 0) &&
	(this->m->object_stream_to_objects.count(old_og.getObj())))
    {
	writeObjectStream(object);
	return;
    }

    indicateProgress(false, false);
    int new_id = this->m->obj_renumber[old_og];
    if (this->m->qdf_mode)
    {
	if (this->m->page_object_to_seq.count(old_og))
	{
	    writeString("%% Page ");
	    writeString(
		QUtil::int_to_string(
		    this->m->page_object_to_seq[old_og]));
	    writeString("\n");
	}
	if (this->m->contents_to_page_seq.count(old_og))
	{
	    writeString("%% Contents for page ");
	    writeString(
		QUtil::int_to_string(
		    this->m->contents_to_page_seq[old_og]));
	    writeString("\n");
	}
    }
    if (object_stream_index == -1)
    {
	if (this->m->qdf_mode && (! this->m->suppress_original_object_ids))
	{
	    writeString("%% Original object ID: " +
			QUtil::int_to_string(object.getObjectID()) + " " +
			QUtil::int_to_string(object.getGeneration()) + "\n");
	}
	openObject(new_id);
	setDataKey(new_id);
	unparseObject(object, 0, 0);
	this->m->cur_data_key.clear();
	closeObject(new_id);
    }
    else
    {
	unparseObject(object, 0, f_in_ostream);
	writeString("\n");
    }

    if ((! this->m->direct_stream_lengths) && object.isStream())
    {
	if (this->m->qdf_mode)
	{
	    if (this->m->added_newline)
	    {
		writeString("%QDF: ignore_newline\n");
	    }
	}
	openObject(new_id + 1);
	writeString(QUtil::uint_to_string(this->m->cur_stream_length));
	closeObject(new_id + 1);
    }
}

std::string
QPDFWriter::getOriginalID1()
{
    QPDFObjectHandle trailer = this->m->pdf.getTrailer();
    if (trailer.hasKey("/ID"))
    {
        return trailer.getKey("/ID").getArrayItem(0).getStringValue();
    }
    else
    {
        return "";
    }
}

void
QPDFWriter::generateID()
{
    // Generate the ID lazily so that we can handle the user's
    // preference to use static or deterministic ID generation.

    if (! this->m->id2.empty())
    {
	return;
    }

    QPDFObjectHandle trailer = this->m->pdf.getTrailer();

    std::string result;

    if (this->m->static_id)
    {
	// For test suite use only...
	static unsigned char tmp[] = {0x31, 0x41, 0x59, 0x26,
                                      0x53, 0x58, 0x97, 0x93,
                                      0x23, 0x84, 0x62, 0x64,
                                      0x33, 0x83, 0x27, 0x95,
                                      0x00};
	result = reinterpret_cast<char*>(tmp);
    }
    else
    {
	// The PDF specification has guidelines for creating IDs, but
	// it states clearly that the only thing that's really
	// important is that it is very likely to be unique.  We can't
	// really follow the guidelines in the spec exactly because we
	// haven't written the file yet.  This scheme should be fine
	// though.  The deterministic ID case uses a digest of a
	// sufficient portion of the file's contents such no two
	// non-matching files would match in the subsets used for this
	// computation.  Note that we explicitly omit the filename from
	// the digest calculation for deterministic ID so that the same
	// file converted with qpdf, in that case, would have the same
	// ID regardless of the output file's name.

	std::string seed;
        if (this->m->deterministic_id)
        {
            if (this->m->deterministic_id_data.empty())
            {
                QTC::TC("qpdf", "QPDFWriter deterministic with no data");
                throw std::logic_error(
                    "INTERNAL ERROR: QPDFWriter::generateID has no"
                    " data for deterministic ID.  This may happen if"
                    " deterministic ID and file encryption are requested"
                    " together.");
            }
            seed += this->m->deterministic_id_data;
        }
        else
        {
            seed += QUtil::int_to_string(QUtil::get_current_time());
            seed += this->m->filename;
            seed += " ";
        }
	seed += " QPDF ";
	if (trailer.hasKey("/Info"))
	{
            QPDFObjectHandle info = trailer.getKey("/Info");
	    std::set<std::string> keys = info.getKeys();
	    for (std::set<std::string>::iterator iter = keys.begin();
		 iter != keys.end(); ++iter)
	    {
		QPDFObjectHandle obj = info.getKey(*iter);
		if (obj.isString())
		{
		    seed += " ";
		    seed += obj.getStringValue();
		}
	    }
	}

	MD5 m;
	m.encodeString(seed.c_str());
	MD5::Digest digest;
	m.digest(digest);
	result = std::string(reinterpret_cast<char*>(digest),
                             sizeof(MD5::Digest));
    }

    // If /ID already exists, follow the spec: use the original first
    // word and generate a new second word.  Otherwise, we'll use the
    // generated ID for both.

    this->m->id2 = result;
    // Note: keep /ID from old file even if --static-id was given.
    this->m->id1 = getOriginalID1();
    if (this->m->id1.empty())
    {
	this->m->id1 = this->m->id2;
    }
}

void
QPDFWriter::initializeSpecialStreams()
{
    // Mark all page content streams in case we are filtering or
    // normalizing.
    std::vector<QPDFObjectHandle> pages = this->m->pdf.getAllPages();
    int num = 0;
    for (std::vector<QPDFObjectHandle>::iterator iter = pages.begin();
	 iter != pages.end(); ++iter)
    {
	QPDFObjectHandle& page = *iter;
	this->m->page_object_to_seq[page.getObjGen()] = ++num;
	QPDFObjectHandle contents = page.getKey("/Contents");
	std::vector<QPDFObjGen> contents_objects;
	if (contents.isArray())
	{
	    int n = contents.getArrayNItems();
	    for (int i = 0; i < n; ++i)
	    {
		contents_objects.push_back(
		    contents.getArrayItem(i).getObjGen());
	    }
	}
	else if (contents.isStream())
	{
	    contents_objects.push_back(contents.getObjGen());
	}

	for (std::vector<QPDFObjGen>::iterator iter = contents_objects.begin();
	     iter != contents_objects.end(); ++iter)
	{
	    this->m->contents_to_page_seq[*iter] = num;
	    this->m->normalized_streams.insert(*iter);
	}
    }
}

void
QPDFWriter::preserveObjectStreams()
{
    // Our object_to_object_stream map has to map ObjGen -> ObjGen
    // since we may be generating object streams out of old objects
    // that have generation numbers greater than zero.  However in an
    // existing PDF, all object stream objects and all objects in them
    // must have generation 0 because the PDF spec does not provide
    // any way to do otherwise.
    std::map<int, int> omap;
    QPDF::Writer::getObjectStreamData(this->m->pdf, omap);
    for (std::map<int, int>::iterator iter = omap.begin();
         iter != omap.end(); ++iter)
    {
        this->m->object_to_object_stream[QPDFObjGen((*iter).first, 0)] =
            (*iter).second;
    }
}

void
QPDFWriter::generateObjectStreams()
{
    // Basic strategy: make a list of objects that can go into an
    // object stream.  Then figure out how many object streams are
    // needed so that we can distribute objects approximately evenly
    // without having any object stream exceed 100 members.  We don't
    // have to worry about linearized files here -- if the file is
    // linearized, we take care of excluding things that aren't
    // allowed here later.

    // This code doesn't do anything with /Extends.

    std::vector<QPDFObjGen> const& eligible =
        QPDF::Writer::getCompressibleObjGens(this->m->pdf);
    size_t n_object_streams = (eligible.size() + 99U) / 100U;
    if (n_object_streams == 0)
    {
        return;
    }
    size_t n_per = eligible.size() / n_object_streams;
    if (n_per * n_object_streams < eligible.size())
    {
	++n_per;
    }
    unsigned int n = 0;
    int cur_ostream = 0;
    for (std::vector<QPDFObjGen>::const_iterator iter = eligible.begin();
	 iter != eligible.end(); ++iter)
    {
	if ((n % n_per) == 0)
	{
	    if (n > 0)
	    {
		QTC::TC("qpdf", "QPDFWriter generate >1 ostream");
	    }
	    n = 0;
	}
	if (n == 0)
	{
	    // Construct a new null object as the "original" object
	    // stream.  The rest of the code knows that this means
	    // we're creating the object stream from scratch.
	    cur_ostream = this->m->pdf.makeIndirectObject(
		QPDFObjectHandle::newNull()).getObjectID();
	}
	this->m->object_to_object_stream[*iter] = cur_ostream;
	++n;
    }
}

QPDFObjectHandle
QPDFWriter::getTrimmedTrailer()
{
    // Remove keys from the trailer that necessarily have to be
    // replaced when writing the file.

    QPDFObjectHandle trailer = this->m->pdf.getTrailer().shallowCopy();

    // Remove encryption keys
    trailer.removeKey("/ID");
    trailer.removeKey("/Encrypt");

    // Remove modification information
    trailer.removeKey("/Prev");

    // Remove all trailer keys that potentially come from a
    // cross-reference stream
    trailer.removeKey("/Index");
    trailer.removeKey("/W");
    trailer.removeKey("/Length");
    trailer.removeKey("/Filter");
    trailer.removeKey("/DecodeParms");
    trailer.removeKey("/Type");
    trailer.removeKey("/XRefStm");

    return trailer;
}

void
QPDFWriter::prepareFileForWrite()
{
    // Do a traversal of the entire PDF file structure replacing all
    // indirect objects that QPDFWriter wants to be direct.  This
    // includes stream lengths, stream filtering parameters, and
    // document extension level information.

    this->m->pdf.fixDanglingReferences(true);
    std::list<QPDFObjectHandle> queue;
    queue.push_back(getTrimmedTrailer());
    std::set<int> visited;

    while (! queue.empty())
    {
	QPDFObjectHandle node = queue.front();
	queue.pop_front();
	if (node.isIndirect())
	{
	    if (visited.count(node.getObjectID()) > 0)
	    {
		continue;
	    }
            indicateProgress(false, false);
	    visited.insert(node.getObjectID());
	}

	if (node.isArray())
	{
	    int nitems = node.getArrayNItems();
	    for (int i = 0; i < nitems; ++i)
	    {
		QPDFObjectHandle oh = node.getArrayItem(i);
                if (! oh.isScalar())
		{
		    queue.push_back(oh);
		}
	    }
	}
	else if (node.isDictionary() || node.isStream())
	{
            bool is_stream = false;
            bool is_root = false;
            bool filterable = false;
	    QPDFObjectHandle dict = node;
	    if (node.isStream())
	    {
                is_stream = true;
		dict = node.getDict();
                // See whether we are able to filter this stream.
                filterable = node.pipeStreamData(
                    0, 0, this->m->stream_decode_level, true);
	    }
            else if (this->m->pdf.getRoot().getObjectID() == node.getObjectID())
            {
                is_root = true;
            }

	    std::set<std::string> keys = dict.getKeys();
	    for (std::set<std::string>::iterator iter = keys.begin();
		 iter != keys.end(); ++iter)
	    {
		std::string const& key = *iter;
		QPDFObjectHandle oh = dict.getKey(key);
                bool add_to_queue = true;
                if (is_stream)
                {
                    if (oh.isIndirect() &&
                        ((key == "/Length") ||
                         (filterable &&
                          ((key == "/Filter") ||
                           (key == "/DecodeParms")))))
                    {
                        QTC::TC("qpdf", "QPDFWriter make stream key direct");
                        add_to_queue = false;
                        oh.makeDirect();
                        dict.replaceKey(key, oh);
                    }
                }
                else if (is_root)
                {
                    if ((key == "/Extensions") && (oh.isDictionary()))
                    {
                        bool extensions_indirect = false;
                        if (oh.isIndirect())
                        {
                            QTC::TC("qpdf", "QPDFWriter make Extensions direct");
                            extensions_indirect = true;
                            add_to_queue = false;
                            oh = oh.shallowCopy();
                            dict.replaceKey(key, oh);
                        }
                        if (oh.hasKey("/ADBE"))
                        {
                            QPDFObjectHandle adbe = oh.getKey("/ADBE");
                            if (adbe.isIndirect())
                            {
                                QTC::TC("qpdf", "QPDFWriter make ADBE direct",
                                        extensions_indirect ? 0 : 1);
                                adbe.makeDirect();
                                oh.replaceKey("/ADBE", adbe);
                            }
                        }
                    }
                }

                if (add_to_queue)
                {
                    queue.push_back(oh);
		}
	    }
	}
    }
}

void
QPDFWriter::doWriteSetup()
{
    if (this->m->did_write_setup)
    {
        return;
    }
    this->m->did_write_setup = true;

    // Do preliminary setup

    if (this->m->linearized)
    {
	this->m->qdf_mode = false;
    }

    if (this->m->pclm)
    {
        this->m->stream_decode_level = qpdf_dl_none;
        this->m->compress_streams = false;
        this->m->encrypted = false;
    }

    if (this->m->qdf_mode)
    {
	if (! this->m->normalize_content_set)
	{
	    this->m->normalize_content = true;
	}
	if (! this->m->compress_streams_set)
	{
	    this->m->compress_streams = false;
	}
        if (! this->m->stream_decode_level_set)
        {
            this->m->stream_decode_level = qpdf_dl_generalized;
        }
    }

    if (this->m->encrypted)
    {
	// Encryption has been explicitly set
	this->m->preserve_encryption = false;
    }
    else if (this->m->normalize_content ||
	     this->m->stream_decode_level ||
             this->m->pclm ||
	     this->m->qdf_mode)
    {
	// Encryption makes looking at contents pretty useless.  If
	// the user explicitly encrypted though, we still obey that.
	this->m->preserve_encryption = false;
    }

    if (this->m->preserve_encryption)
    {
	copyEncryptionParameters(this->m->pdf);
    }

    if (! this->m->forced_pdf_version.empty())
    {
	int major = 0;
	int minor = 0;
	parseVersion(this->m->forced_pdf_version, major, minor);
	disableIncompatibleEncryption(major, minor,
                                      this->m->forced_extension_level);
	if (compareVersions(major, minor, 1, 5) < 0)
	{
	    QTC::TC("qpdf", "QPDFWriter forcing object stream disable");
	    this->m->object_stream_mode = qpdf_o_disable;
	}
    }

    if (this->m->qdf_mode || this->m->normalize_content ||
        this->m->stream_decode_level)
    {
	initializeSpecialStreams();
    }

    if (this->m->qdf_mode)
    {
	// Generate indirect stream lengths for qdf mode since fix-qdf
	// uses them for storing recomputed stream length data.
	// Certain streams such as object streams, xref streams, and
	// hint streams always get direct stream lengths.
	this->m->direct_stream_lengths = false;
    }

    switch (this->m->object_stream_mode)
    {
      case qpdf_o_disable:
	// no action required
	break;

      case qpdf_o_preserve:
	preserveObjectStreams();
	break;

      case qpdf_o_generate:
	generateObjectStreams();
	break;

	// no default so gcc will warn for missing case tag
    }

    if (this->m->linearized)
    {
	// Page dictionaries are not allowed to be compressed objects.
	std::vector<QPDFObjectHandle> pages = this->m->pdf.getAllPages();
	for (std::vector<QPDFObjectHandle>::iterator iter = pages.begin();
	     iter != pages.end(); ++iter)
	{
	    QPDFObjectHandle& page = *iter;
	    QPDFObjGen og = page.getObjGen();
	    if (this->m->object_to_object_stream.count(og))
	    {
		QTC::TC("qpdf", "QPDFWriter uncompressing page dictionary");
		this->m->object_to_object_stream.erase(og);
	    }
	}
    }

    if (this->m->linearized || this->m->encrypted)
    {
    	// The document catalog is not allowed to be compressed in
    	// linearized files either.  It also appears that Adobe Reader
    	// 8.0.0 has a bug that prevents it from being able to handle
    	// encrypted files with compressed document catalogs, so we
    	// disable them in that case as well.
	QPDFObjGen og = this->m->pdf.getRoot().getObjGen();
	if (this->m->object_to_object_stream.count(og))
	{
	    QTC::TC("qpdf", "QPDFWriter uncompressing root");
	    this->m->object_to_object_stream.erase(og);
	}
    }

    // Generate reverse mapping from object stream to objects
    for (std::map<QPDFObjGen, int>::iterator iter =
	     this->m->object_to_object_stream.begin();
	 iter != this->m->object_to_object_stream.end(); ++iter)
    {
	QPDFObjGen obj = (*iter).first;
	int stream = (*iter).second;
	this->m->object_stream_to_objects[stream].insert(obj);
	this->m->max_ostream_index =
	    std::max(this->m->max_ostream_index,
		     QIntC::to_int(
                         this->m->object_stream_to_objects[stream].size()) - 1);
    }

    if (! this->m->object_stream_to_objects.empty())
    {
	setMinimumPDFVersion("1.5");
    }

    setMinimumPDFVersion(this->m->pdf.getPDFVersion(),
                         this->m->pdf.getExtensionLevel());
    this->m->final_pdf_version = this->m->min_pdf_version;
    this->m->final_extension_level = this->m->min_extension_level;
    if (! this->m->forced_pdf_version.empty())
    {
	QTC::TC("qpdf", "QPDFWriter using forced PDF version");
	this->m->final_pdf_version = this->m->forced_pdf_version;
        this->m->final_extension_level = this->m->forced_extension_level;
    }
}

void
QPDFWriter::write()
{
    doWriteSetup();

    // Set up progress reporting. We spent about equal amounts of time
    // preparing and writing one pass. To get a rough estimate of
    // progress, we track handling of indirect objects. For linearized
    // files, we write two passes. events_expected is an
    // approximation, but it's good enough for progress reporting,
    // which is mostly a guess anyway.
    this->m->events_expected = QIntC::to_int(
        this->m->pdf.getObjectCount() * (this->m->linearized ? 3 : 2));

    prepareFileForWrite();

    if (this->m->linearized)
    {
	writeLinearized();
    }
    else
    {
	writeStandard();
    }

    this->m->pipeline->finish();
    if (this->m->close_file)
    {
	fclose(this->m->file);
    }
    this->m->file = 0;
    if (this->m->buffer_pipeline)
    {
	this->m->output_buffer = this->m->buffer_pipeline->getBuffer();
	this->m->buffer_pipeline = 0;
    }
    indicateProgress(false, true);
}

void
QPDFWriter::enqueuePart(std::vector<QPDFObjectHandle>& part)
{
    for (std::vector<QPDFObjectHandle>::iterator iter = part.begin();
	 iter != part.end(); ++iter)
    {
	enqueueObject(*iter);
    }
}

void
QPDFWriter::writeEncryptionDictionary()
{
    this->m->encryption_dict_objid = openObject(this->m->encryption_dict_objid);
    writeString("<<");
    for (std::map<std::string, std::string>::iterator iter =
	     this->m->encryption_dictionary.begin();
	 iter != this->m->encryption_dictionary.end(); ++iter)
    {
	writeString(" ");
	writeString((*iter).first);
	writeString(" ");
	writeString((*iter).second);
    }
    writeString(" >>");
    closeObject(this->m->encryption_dict_objid);
}

std::string
QPDFWriter::getFinalVersion()
{
    doWriteSetup();
    return this->m->final_pdf_version;
}

void
QPDFWriter::writeHeader()
{
    writeString("%PDF-");
    writeString(this->m->final_pdf_version);
    if (this->m->pclm)
    {
        // PCLm version
        writeString("\n%PCLm 1.0\n");
    }
    else
    {
        // This string of binary characters would not be valid UTF-8, so
        // it really should be treated as binary.
        writeString("\n%\xbf\xf7\xa2\xfe\n");
    }
    writeStringQDF("%QDF-1.0\n\n");

    // Note: do not write extra header text here.  Linearized PDFs
    // must include the entire linearization parameter dictionary
    // within the first 1024 characters of the PDF file, so for
    // linearized files, we have to write extra header text after the
    // linearization parameter dictionary.
}

void
QPDFWriter::writeHintStream(int hint_id)
{
    PointerHolder<Buffer> hint_buffer;
    int S = 0;
    int O = 0;
    QPDF::Writer::generateHintStream(
        this->m->pdf, this->m->xref, this->m->lengths,
        this->m->obj_renumber_no_gen,
        hint_buffer, S, O);

    openObject(hint_id);
    setDataKey(hint_id);

    size_t hlen = hint_buffer->getSize();

    writeString("<< /Filter /FlateDecode /S ");
    writeString(QUtil::int_to_string(S));
    if (O)
    {
	writeString(" /O ");
	writeString(QUtil::int_to_string(O));
    }
    writeString(" /Length ");
    adjustAESStreamLength(hlen);
    writeString(QUtil::uint_to_string(hlen));
    writeString(" >>\nstream\n");

    if (this->m->encrypted)
    {
	QTC::TC("qpdf", "QPDFWriter encrypted hint stream");
    }
    unsigned char last_char = '\0';
    {
        PipelinePopper pp_enc(this);
        pushEncryptionFilter(pp_enc);
        writeBuffer(hint_buffer);
        last_char = this->m->pipeline->getLastChar();
    }

    if (last_char != '\n')
    {
	writeString("\n");
    }
    writeString("endstream");
    closeObject(hint_id);
}

qpdf_offset_t
QPDFWriter::writeXRefTable(trailer_e which, int first, int last, int size)
{
    // There are too many extra arguments to replace overloaded
    // function with defaults in the header file...too much risk of
    // leaving something off.
    return writeXRefTable(which, first, last, size, 0, false, 0, 0, 0, 0);
}

qpdf_offset_t
QPDFWriter::writeXRefTable(trailer_e which, int first, int last, int size,
			   qpdf_offset_t prev, bool suppress_offsets,
			   int hint_id, qpdf_offset_t hint_offset,
                           qpdf_offset_t hint_length, int linearization_pass)
{
    writeString("xref\n");
    writeString(QUtil::int_to_string(first));
    writeString(" ");
    writeString(QUtil::int_to_string(last - first + 1));
    qpdf_offset_t space_before_zero = this->m->pipeline->getCount();
    writeString("\n");
    for (int i = first; i <= last; ++i)
    {
	if (i == 0)
	{
	    writeString("0000000000 65535 f \n");
	}
	else
	{
	    qpdf_offset_t offset = 0;
	    if (! suppress_offsets)
	    {
		offset = this->m->xref[i].getOffset();
		if ((hint_id != 0) &&
		    (i != hint_id) &&
		    (offset >= hint_offset))
		{
		    offset += hint_length;
		}
	    }
	    writeString(QUtil::int_to_string(offset, 10));
	    writeString(" 00000 n \n");
	}
    }
    writeTrailer(which, size, false, prev, linearization_pass);
    writeString("\n");
    return space_before_zero;
}

qpdf_offset_t
QPDFWriter::writeXRefStream(int objid, int max_id, qpdf_offset_t max_offset,
			    trailer_e which, int first, int last, int size)
{
    // There are too many extra arguments to replace overloaded
    // function with defaults in the header file...too much risk of
    // leaving something off.
    return writeXRefStream(objid, max_id, max_offset,
			   which, first, last, size, 0, 0, 0, 0, false, 0);
}

qpdf_offset_t
QPDFWriter::writeXRefStream(int xref_id, int max_id, qpdf_offset_t max_offset,
			    trailer_e which, int first, int last, int size,
			    qpdf_offset_t prev, int hint_id,
			    qpdf_offset_t hint_offset,
                            qpdf_offset_t hint_length,
			    bool skip_compression,
                            int linearization_pass)
{
    qpdf_offset_t xref_offset = this->m->pipeline->getCount();
    qpdf_offset_t space_before_zero = xref_offset - 1;

    // field 1 contains offsets and object stream identifiers
    unsigned int f1_size = std::max(bytesNeeded(max_offset + hint_length),
                                    bytesNeeded(max_id));

    // field 2 contains object stream indices
    unsigned int f2_size = bytesNeeded(this->m->max_ostream_index);

    unsigned int esize = 1 + f1_size + f2_size;

    // Must store in xref table in advance of writing the actual data
    // rather than waiting for openObject to do it.
    this->m->xref[xref_id] = QPDFXRefEntry(1, this->m->pipeline->getCount(), 0);

    Pipeline* p = pushPipeline(new Pl_Buffer("xref stream"));
    bool compressed = false;
    if ((this->m->compress_streams ||
         (this->m->stream_decode_level == qpdf_dl_none)) &&
        (! this->m->qdf_mode))
    {
	compressed = true;
	if (! skip_compression)
	{
	    // Write the stream dictionary for compression but don't
	    // actually compress.  This helps us with computation of
	    // padding for pass 1 of linearization.
	    p = pushPipeline(
		new Pl_Flate("compress xref", p, Pl_Flate::a_deflate));
	}
	p = pushPipeline(
	    new Pl_PNGFilter(
		"pngify xref", p, Pl_PNGFilter::a_encode, esize));
    }
    PointerHolder<Buffer> xref_data;
    {
        PipelinePopper pp_xref(this, &xref_data);
        activatePipelineStack(pp_xref);
        for (int i = first; i <= last; ++i)
        {
            QPDFXRefEntry& e = this->m->xref[i];
            switch (e.getType())
            {
              case 0:
                writeBinary(0, 1);
                writeBinary(0, f1_size);
                writeBinary(0, f2_size);
                break;

              case 1:
                {
                    qpdf_offset_t offset = e.getOffset();
                    if ((hint_id != 0) &&
                        (i != hint_id) &&
                        (offset >= hint_offset))
                    {
                        offset += hint_length;
                    }
                    writeBinary(1, 1);
                    writeBinary(QIntC::to_ulonglong(offset), f1_size);
                    writeBinary(0, f2_size);
                }
                break;

              case 2:
                writeBinary(2, 1);
                writeBinary(QIntC::to_ulonglong(e.getObjStreamNumber()), f1_size);
                writeBinary(QIntC::to_ulonglong(e.getObjStreamIndex()), f2_size);
                break;

              default:
                throw std::logic_error("invalid type writing xref stream");
                break;
            }
        }
    }

    openObject(xref_id);
    writeString("<<");
    writeStringQDF("\n ");
    writeString(" /Type /XRef");
    writeStringQDF("\n ");
    writeString(" /Length " + QUtil::uint_to_string(xref_data->getSize()));
    if (compressed)
    {
	writeStringQDF("\n ");
	writeString(" /Filter /FlateDecode");
	writeStringQDF("\n ");
	writeString(" /DecodeParms << /Columns " +
		    QUtil::int_to_string(esize) + " /Predictor 12 >>");
    }
    writeStringQDF("\n ");
    writeString(" /W [ 1 " +
		QUtil::int_to_string(f1_size) + " " +
		QUtil::int_to_string(f2_size) + " ]");
    if (! ((first == 0) && (last == size - 1)))
    {
	writeString(" /Index [ " +
		    QUtil::int_to_string(first) + " " +
		    QUtil::int_to_string(last - first + 1) + " ]");
    }
    writeTrailer(which, size, true, prev, linearization_pass);
    writeString("\nstream\n");
    writeBuffer(xref_data);
    writeString("\nendstream");
    closeObject(xref_id);
    return space_before_zero;
}

int
QPDFWriter::calculateXrefStreamPadding(qpdf_offset_t xref_bytes)
{
    // This routine is called right after a linearization first pass
    // xref stream has been written without compression.  Calculate
    // the amount of padding that would be required in the worst case,
    // assuming the number of uncompressed bytes remains the same.
    // The worst case for zlib is that the output is larger than the
    // input by 6 bytes plus 5 bytes per 16K, and then we'll add 10
    // extra bytes for number length increases.

    return QIntC::to_int(16 + (5 * ((xref_bytes + 16383) / 16384)));
}

void
QPDFWriter::discardGeneration(std::map<QPDFObjGen, int> const& in,
                              std::map<int, int>& out)
{
    // There are deep assumptions in the linearization code in QPDF
    // that there is only one object with each object number; i.e.,
    // you can't have two objects with the same object number and
    // different generations.  This is a pretty safe assumption
    // because Adobe Reader and Acrobat can't actually handle this
    // case.  There is not much if any code in QPDF outside
    // linearization that assumes this, but the linearization code as
    // currently implemented would do weird things if we found such a
    // case.  In order to avoid breaking ABI changes in QPDF, we will
    // first assert that this condition holds.  Then we can create new
    // maps for QPDF that throw away generation numbers.

    out.clear();
    for (std::map<QPDFObjGen, int>::const_iterator iter = in.begin();
         iter != in.end(); ++iter)
    {
        if (out.count((*iter).first.getObj()))
        {
            throw std::runtime_error(
                "QPDF cannot currently linearize files that contain"
                " multiple objects with the same object ID and different"
                " generations.  If you see this error message, please file"
                " a bug report and attach the file if possible.  As a"
                " workaround, first convert the file with qpdf without"
                " linearizing, and then linearize the result of that"
                " conversion.");
        }
        out[(*iter).first.getObj()] = (*iter).second;
    }
}

void
QPDFWriter::writeLinearized()
{
    // Optimize file and enqueue objects in order

    discardGeneration(this->m->object_to_object_stream,
                      this->m->object_to_object_stream_no_gen);

    bool need_xref_stream = (! this->m->object_to_object_stream.empty());
    this->m->pdf.optimize(this->m->object_to_object_stream_no_gen);

    std::vector<QPDFObjectHandle> part4;
    std::vector<QPDFObjectHandle> part6;
    std::vector<QPDFObjectHandle> part7;
    std::vector<QPDFObjectHandle> part8;
    std::vector<QPDFObjectHandle> part9;
    QPDF::Writer::getLinearizedParts(
        this->m->pdf, this->m->object_to_object_stream_no_gen,
        part4, part6, part7, part8, part9);

    // Object number sequence:
    //
    //  second half
    //    second half uncompressed objects
    //    second half xref stream, if any
    //    second half compressed objects
    //  first half
    //    linearization dictionary
    //    first half xref stream, if any
    //    part 4 uncompresesd objects
    //    encryption dictionary, if any
    //    hint stream
    //    part 6 uncompressed objects
    //    first half compressed objects
    //

    // Second half objects
    int second_half_uncompressed =
        QIntC::to_int(part7.size() + part8.size() + part9.size());
    int second_half_first_obj = 1;
    int after_second_half = 1 + second_half_uncompressed;
    this->m->next_objid = after_second_half;
    int second_half_xref = 0;
    if (need_xref_stream)
    {
	second_half_xref = this->m->next_objid++;
    }
    // Assign numbers to all compressed objects in the second half.
    std::vector<QPDFObjectHandle>* vecs2[] = {&part7, &part8, &part9};
    for (int i = 0; i < 3; ++i)
    {
	for (std::vector<QPDFObjectHandle>::iterator iter = (*vecs2[i]).begin();
	     iter != (*vecs2[i]).end(); ++iter)
	{
	    assignCompressedObjectNumbers((*iter).getObjGen());
	}
    }
    int second_half_end = this->m->next_objid - 1;
    int second_trailer_size = this->m->next_objid;

    // First half objects
    int first_half_start = this->m->next_objid;
    int lindict_id = this->m->next_objid++;
    int first_half_xref = 0;
    if (need_xref_stream)
    {
	first_half_xref = this->m->next_objid++;
    }
    int part4_first_obj = this->m->next_objid;
    this->m->next_objid += QIntC::to_int(part4.size());
    int after_part4 = this->m->next_objid;
    if (this->m->encrypted)
    {
	this->m->encryption_dict_objid = this->m->next_objid++;
    }
    int hint_id = this->m->next_objid++;
    int part6_first_obj = this->m->next_objid;
    this->m->next_objid += QIntC::to_int(part6.size());
    int after_part6 = this->m->next_objid;
    // Assign numbers to all compressed objects in the first half
    std::vector<QPDFObjectHandle>* vecs1[] = {&part4, &part6};
    for (int i = 0; i < 2; ++i)
    {
	for (std::vector<QPDFObjectHandle>::iterator iter = (*vecs1[i]).begin();
	     iter != (*vecs1[i]).end(); ++iter)
	{
	    assignCompressedObjectNumbers((*iter).getObjGen());
	}
    }
    int first_half_end = this->m->next_objid - 1;
    int first_trailer_size = this->m->next_objid;

    int part4_end_marker = part4.back().getObjectID();
    int part6_end_marker = part6.back().getObjectID();
    qpdf_offset_t space_before_zero = 0;
    qpdf_offset_t file_size = 0;
    qpdf_offset_t part6_end_offset = 0;
    qpdf_offset_t first_half_max_obj_offset = 0;
    qpdf_offset_t second_xref_offset = 0;
    qpdf_offset_t first_xref_end = 0;
    qpdf_offset_t second_xref_end = 0;

    this->m->next_objid = part4_first_obj;
    enqueuePart(part4);
    if (this->m->next_objid != after_part4)
    {
        // This can happen with very botched files as in the fuzzer
        // test. There are likely some faulty assumptions in
        // calculateLinearizationData
        throw std::runtime_error(
            "error encountered after"
            " writing part 4 of linearized data");
    }
    this->m->next_objid = part6_first_obj;
    enqueuePart(part6);
    if (this->m->next_objid != after_part6)
    {
        throw std::runtime_error(
            "error encountered after"
            " writing part 6 of linearized data");
    }
    this->m->next_objid = second_half_first_obj;
    enqueuePart(part7);
    enqueuePart(part8);
    enqueuePart(part9);
    if (this->m->next_objid != after_second_half)
    {
        throw std::runtime_error(
            "error encountered after"
            " writing part 9 of linearized data");
    }

    qpdf_offset_t hint_length = 0;
    PointerHolder<Buffer> hint_buffer;

    // Write file in two passes.  Part numbers refer to PDF spec 1.4.

    FILE* lin_pass1_file = 0;
    PointerHolder<PipelinePopper> pp_pass1 = new PipelinePopper(this);
    PointerHolder<PipelinePopper> pp_md5 = new PipelinePopper(this);
    for (int pass = 1; pass <= 2; ++pass)
    {
	if (pass == 1)
	{
            if (! this->m->lin_pass1_filename.empty())
            {
                lin_pass1_file =
                    QUtil::safe_fopen(
                        this->m->lin_pass1_filename.c_str(), "wb");
                pushPipeline(
                    new Pl_StdioFile("linearization pass1", lin_pass1_file));
                activatePipelineStack(*pp_pass1);
            }
            else
            {
                pushDiscardFilter(*pp_pass1);
            }
            if (this->m->deterministic_id)
            {
                pushMD5Pipeline(*pp_md5);
            }
	}

	// Part 1: header

	writeHeader();

	// Part 2: linearization parameter dictionary.  Save enough
	// space to write real dictionary.  200 characters is enough
	// space if all numerical values in the parameter dictionary
	// that contain offsets are 20 digits long plus a few extra
	// characters for safety.  The entire linearization parameter
	// dictionary must appear within the first 1024 characters of
	// the file.

	qpdf_offset_t pos = this->m->pipeline->getCount();
	openObject(lindict_id);
	writeString("<<");
	if (pass == 2)
	{
	    std::vector<QPDFObjectHandle> const& pages =
                this->m->pdf.getAllPages();
	    int first_page_object =
                this->m->obj_renumber[pages.at(0).getObjGen()];
	    int npages = QIntC::to_int(pages.size());

	    writeString(" /Linearized 1 /L ");
	    writeString(QUtil::int_to_string(file_size + hint_length));
	    // Implementation note 121 states that a space is
	    // mandatory after this open bracket.
	    writeString(" /H [ ");
	    writeString(QUtil::int_to_string(
                            this->m->xref[hint_id].getOffset()));
	    writeString(" ");
	    writeString(QUtil::int_to_string(hint_length));
	    writeString(" ] /O ");
	    writeString(QUtil::int_to_string(first_page_object));
	    writeString(" /E ");
	    writeString(QUtil::int_to_string(part6_end_offset + hint_length));
	    writeString(" /N ");
	    writeString(QUtil::int_to_string(npages));
	    writeString(" /T ");
	    writeString(QUtil::int_to_string(space_before_zero + hint_length));
	}
	writeString(" >>");
	closeObject(lindict_id);
	static int const pad = 200;
	int spaces = QIntC::to_int(pos - this->m->pipeline->getCount() + pad);
	assert(spaces >= 0);
	writePad(spaces);
	writeString("\n");

        // If the user supplied any additional header text, write it
        // here after the linearization parameter dictionary.
        writeString(this->m->extra_header_text);

	// Part 3: first page cross reference table and trailer.

	qpdf_offset_t first_xref_offset = this->m->pipeline->getCount();
	qpdf_offset_t hint_offset = 0;
	if (pass == 2)
	{
	    hint_offset = this->m->xref[hint_id].getOffset();
	}
	if (need_xref_stream)
	{
	    // Must pad here too.
	    if (pass == 1)
	    {
		// Set first_half_max_obj_offset to a value large
		// enough to force four bytes to be reserved for each
		// file offset.  This would provide adequate space for
		// the xref stream as long as the last object in page
		// 1 starts with in the first 4 GB of the file, which
		// is extremely likely.  In the second pass, we will
		// know the actual value for this, but it's okay if
		// it's smaller.
		first_half_max_obj_offset = 1 << 25;
	    }
	    pos = this->m->pipeline->getCount();
	    writeXRefStream(first_half_xref, first_half_end,
			    first_half_max_obj_offset,
			    t_lin_first, first_half_start, first_half_end,
			    first_trailer_size,
			    hint_length + second_xref_offset,
			    hint_id, hint_offset, hint_length,
			    (pass == 1), pass);
	    qpdf_offset_t endpos = this->m->pipeline->getCount();
	    if (pass == 1)
	    {
		// Pad so we have enough room for the real xref
		// stream.
		writePad(calculateXrefStreamPadding(endpos - pos));
		first_xref_end = this->m->pipeline->getCount();
	    }
	    else
	    {
		// Pad so that the next object starts at the same
		// place as in pass 1.
		writePad(QIntC::to_int(first_xref_end - endpos));

		if (this->m->pipeline->getCount() != first_xref_end)
                {
                    throw std::logic_error(
                        "insufficient padding for first pass xref stream");
                }
	    }
	    writeString("\n");
	}
	else
	{
	    writeXRefTable(t_lin_first, first_half_start, first_half_end,
			   first_trailer_size, hint_length + second_xref_offset,
			   (pass == 1), hint_id, hint_offset, hint_length,
                           pass);
	    writeString("startxref\n0\n%%EOF\n");
	}

	// Parts 4 through 9

	for (std::list<QPDFObjectHandle>::iterator iter =
		 this->m->object_queue.begin();
	     iter != this->m->object_queue.end(); ++iter)
	{
	    QPDFObjectHandle cur_object = (*iter);
	    if (cur_object.getObjectID() == part6_end_marker)
	    {
		first_half_max_obj_offset = this->m->pipeline->getCount();
	    }
	    writeObject(cur_object);
	    if (cur_object.getObjectID() == part4_end_marker)
	    {
		if (this->m->encrypted)
		{
		    writeEncryptionDictionary();
		}
		if (pass == 1)
		{
		    this->m->xref[hint_id] =
			QPDFXRefEntry(1, this->m->pipeline->getCount(), 0);
		}
		else
		{
		    // Part 5: hint stream
		    writeBuffer(hint_buffer);
		}
	    }
	    if (cur_object.getObjectID() == part6_end_marker)
	    {
		part6_end_offset = this->m->pipeline->getCount();
	    }
	}

	// Part 10: overflow hint stream -- not used

	// Part 11: main cross reference table and trailer

	second_xref_offset = this->m->pipeline->getCount();
	if (need_xref_stream)
	{
	    pos = this->m->pipeline->getCount();
	    space_before_zero =
		writeXRefStream(second_half_xref,
				second_half_end, second_xref_offset,
				t_lin_second, 0, second_half_end,
				second_trailer_size,
				0, 0, 0, 0, (pass == 1), pass);
	    qpdf_offset_t endpos = this->m->pipeline->getCount();

	    if (pass == 1)
	    {
		// Pad so we have enough room for the real xref
		// stream.  See comments for previous xref stream on
		// how we calculate the padding.
		writePad(calculateXrefStreamPadding(endpos - pos));
		writeString("\n");
		second_xref_end = this->m->pipeline->getCount();
	    }
	    else
	    {
		// Make the file size the same.
		qpdf_offset_t pos = this->m->pipeline->getCount();
		writePad(
                    QIntC::to_int(second_xref_end + hint_length - 1 - pos));
		writeString("\n");

		// If this assertion fails, maybe we didn't have
		// enough padding above.
		if (this->m->pipeline->getCount() !=
                    second_xref_end + hint_length)
                {
                    throw std::logic_error(
                        "count mismatch after xref stream;"
                        " possible insufficient padding?");
                }
	    }
	}
	else
	{
	    space_before_zero =
		writeXRefTable(t_lin_second, 0, second_half_end,
			       second_trailer_size, 0, false, 0, 0, 0, pass);
	}
	writeString("startxref\n");
	writeString(QUtil::int_to_string(first_xref_offset));
	writeString("\n%%EOF\n");

        discardGeneration(this->m->obj_renumber, this->m->obj_renumber_no_gen);

	if (pass == 1)
	{
            if (this->m->deterministic_id)
            {
                QTC::TC("qpdf", "QPDFWriter linearized deterministic ID",
                        need_xref_stream ? 0 : 1);
                computeDeterministicIDData();
                pp_md5 = 0;
                assert(this->m->md5_pipeline == 0);
            }

	    // Close first pass pipeline
	    file_size = this->m->pipeline->getCount();
	    pp_pass1 = 0;

	    // Save hint offset since it will be set to zero by
	    // calling openObject.
	    qpdf_offset_t hint_offset = this->m->xref[hint_id].getOffset();

	    // Write hint stream to a buffer
            {
                pushPipeline(new Pl_Buffer("hint buffer"));
                PipelinePopper pp_hint(this, &hint_buffer);
                activatePipelineStack(pp_hint);
                writeHintStream(hint_id);
            }
	    hint_length = QIntC::to_offset(hint_buffer->getSize());

	    // Restore hint offset
	    this->m->xref[hint_id] = QPDFXRefEntry(1, hint_offset, 0);
            if (lin_pass1_file)
            {
                // Write some debugging information
                fprintf(lin_pass1_file, "%% hint_offset=%s\n",
                        QUtil::int_to_string(hint_offset).c_str());
                fprintf(lin_pass1_file, "%% hint_length=%s\n",
                        QUtil::int_to_string(hint_length).c_str());
                fprintf(lin_pass1_file, "%% second_xref_offset=%s\n",
                        QUtil::int_to_string(second_xref_offset).c_str());
                fprintf(lin_pass1_file, "%% second_xref_end=%s\n",
                        QUtil::int_to_string(second_xref_end).c_str());
                fclose(lin_pass1_file);
                lin_pass1_file = 0;
            }
	}
    }
}

void
QPDFWriter::enqueueObjectsStandard()
{
    if (this->m->preserve_unreferenced_objects)
    {
        QTC::TC("qpdf", "QPDFWriter preserve unreferenced standard");
        std::vector<QPDFObjectHandle> all = this->m->pdf.getAllObjects();
        for (std::vector<QPDFObjectHandle>::iterator iter = all.begin();
             iter != all.end(); ++iter)
        {
            enqueueObject(*iter);
        }
    }

    // Put root first on queue.
    QPDFObjectHandle trailer = getTrimmedTrailer();
    enqueueObject(trailer.getKey("/Root"));

    // Next place any other objects referenced from the trailer
    // dictionary into the queue, handling direct objects recursively.
    // Root is already there, so enqueuing it a second time is a
    // no-op.
    std::set<std::string> keys = trailer.getKeys();
    for (std::set<std::string>::iterator iter = keys.begin();
	 iter != keys.end(); ++iter)
    {
	enqueueObject(trailer.getKey(*iter));
    }
}

void
QPDFWriter::enqueueObjectsPCLm()
{
    // Image transform stream content for page strip images.
    // Each of this new stream has to come after every page image
    // strip written in the pclm file.
    std::string image_transform_content = "q /image Do Q\n";

    // enqueue all pages first
    std::vector<QPDFObjectHandle> all = this->m->pdf.getAllPages();
    for (std::vector<QPDFObjectHandle>::iterator iter = all.begin();
         iter != all.end(); ++iter)
    {
        // enqueue page
        enqueueObject(*iter);

        // enqueue page contents stream
        enqueueObject((*iter).getKey("/Contents"));

        // enqueue all the strips for each page
        QPDFObjectHandle strips =
            (*iter).getKey("/Resources").getKey("/XObject");
        std::set<std::string> keys = strips.getKeys();
        for (std::set<std::string>::iterator image = keys.begin();
             image != keys.end(); ++image)
        {
            enqueueObject(strips.getKey(*image));
            enqueueObject(QPDFObjectHandle::newStream(
                              &this->m->pdf, image_transform_content));
        }
    }

    // Put root in queue.
    QPDFObjectHandle trailer = getTrimmedTrailer();
    enqueueObject(trailer.getKey("/Root"));
}

void
QPDFWriter::indicateProgress(bool decrement, bool finished)
{
    if (decrement)
    {
        --this->m->events_seen;
        return;
    }

    ++this->m->events_seen;

    if (! this->m->progress_reporter.getPointer())
    {
        return;
    }

    if (finished || (this->m->events_seen >= this->m->next_progress_report))
    {
        int percentage = (
            finished
            ? 100
            : this->m->next_progress_report == 0
            ? 0
            : std::min(99, 1 + ((100 * this->m->events_seen) /
                                this->m->events_expected)));
        this->m->progress_reporter->reportProgress(percentage);
    }
    int increment = std::max(1, (this->m->events_expected / 100));
    while (this->m->events_seen >= this->m->next_progress_report)
    {
        this->m->next_progress_report += increment;
    }
}

void
QPDFWriter::registerProgressReporter(PointerHolder<ProgressReporter> pr)
{
    this->m->progress_reporter = pr;
}

void
QPDFWriter::writeStandard()
{
    PointerHolder<PipelinePopper> pp_md5 = new PipelinePopper(this);
    if (this->m->deterministic_id)
    {
        pushMD5Pipeline(*pp_md5);
    }

    // Start writing

    writeHeader();
    writeString(this->m->extra_header_text);

    if (this->m->pclm)
    {
        enqueueObjectsPCLm();
    }
    else
    {
        enqueueObjectsStandard();
    }

    // Now start walking queue, outputting each object.
    while (this->m->object_queue.size())
    {
	QPDFObjectHandle cur_object = this->m->object_queue.front();
	this->m->object_queue.pop_front();
	writeObject(cur_object);
    }

    // Write out the encryption dictionary, if any
    if (this->m->encrypted)
    {
	writeEncryptionDictionary();
    }

    // Now write out xref.  next_objid is now the number of objects.
    qpdf_offset_t xref_offset = this->m->pipeline->getCount();
    if (this->m->object_stream_to_objects.empty())
    {
	// Write regular cross-reference table
	writeXRefTable(t_normal, 0, this->m->next_objid - 1,
                       this->m->next_objid);
    }
    else
    {
	// Write cross-reference stream.
	int xref_id = this->m->next_objid++;
	writeXRefStream(xref_id, xref_id, xref_offset, t_normal,
			0, this->m->next_objid - 1, this->m->next_objid);
    }
    writeString("startxref\n");
    writeString(QUtil::int_to_string(xref_offset));
    writeString("\n%%EOF\n");

    if (this->m->deterministic_id)
    {
	QTC::TC("qpdf", "QPDFWriter standard deterministic ID",
                this->m->object_stream_to_objects.empty() ? 0 : 1);
        pp_md5 = 0;
        assert(this->m->md5_pipeline == 0);
    }
}
