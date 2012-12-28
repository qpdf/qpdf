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
#include <qpdf/QUtil.hh>
#include <qpdf/MD5.hh>
#include <qpdf/RC4.hh>
#include <qpdf/QTC.hh>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDF_Name.hh>
#include <qpdf/QPDF_String.hh>

#include <stdlib.h>

QPDFWriter::QPDFWriter(QPDF& pdf) :
    pdf(pdf)
{
    init();
}

QPDFWriter::QPDFWriter(QPDF& pdf, char const* filename) :
    pdf(pdf)
{
    init();
    setOutputFilename(filename);
}

QPDFWriter::QPDFWriter(QPDF& pdf, char const* description,
                       FILE *file, bool close_file) :
    pdf(pdf)
{
    init();
    setOutputFile(description, file, close_file);
}

void
QPDFWriter::init()
{
    filename = 0;
    file = 0;
    close_file = false;
    buffer_pipeline = 0;
    output_buffer = 0;
    normalize_content_set = false;
    normalize_content = false;
    stream_data_mode_set = false;
    stream_data_mode = qpdf_s_compress;
    qdf_mode = false;
    static_id = false;
    suppress_original_object_ids = false;
    direct_stream_lengths = true;
    encrypted = false;
    preserve_encryption = true;
    linearized = false;
    object_stream_mode = qpdf_o_preserve;
    encrypt_metadata = true;
    encrypt_use_aes = false;
    min_extension_level = 0;
    final_extension_level = 0;
    forced_extension_level = 0;
    encryption_dict_objid = 0;
    next_objid = 1;
    cur_stream_length_id = 0;
    cur_stream_length = 0;
    added_newline = false;
    max_ostream_index = 0;
}

QPDFWriter::~QPDFWriter()
{
    if (file && close_file)
    {
	fclose(file);
    }
    if (output_buffer)
    {
	delete output_buffer;
    }
}

void
QPDFWriter::setOutputFilename(char const* filename)
{
    char const* description = filename;
    FILE* f = 0;
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
	f = QUtil::fopen_wrapper(std::string("open ") + filename,
                                 fopen(filename, "wb+"));
	close_file = true;
    }
    setOutputFile(description, f, close_file);
}

void
QPDFWriter::setOutputFile(char const* description, FILE* file, bool close_file)
{
    this->filename = description;
    this->file = file;
    this->close_file = close_file;
    Pipeline* p = new Pl_StdioFile("qpdf output", file);
    to_delete.push_back(p);
    initializePipelineStack(p);
}

void
QPDFWriter::setOutputMemory()
{
    this->filename = "memory buffer";
    this->buffer_pipeline = new Pl_Buffer("qpdf output");
    to_delete.push_back(this->buffer_pipeline);
    initializePipelineStack(this->buffer_pipeline);
}

Buffer*
QPDFWriter::getBuffer()
{
    Buffer* result = this->output_buffer;
    this->output_buffer = 0;
    return result;
}

void
QPDFWriter::setOutputPipeline(Pipeline* p)
{
    this->filename = "custom pipeline";
    initializePipelineStack(p);
}

void
QPDFWriter::setObjectStreamMode(qpdf_object_stream_e mode)
{
    this->object_stream_mode = mode;
}

void
QPDFWriter::setStreamDataMode(qpdf_stream_data_e mode)
{
    this->stream_data_mode_set = true;
    this->stream_data_mode = mode;
}

void
QPDFWriter::setContentNormalization(bool val)
{
    this->normalize_content_set = true;
    this->normalize_content = val;
}

void
QPDFWriter::setQDFMode(bool val)
{
    this->qdf_mode = val;
}

void
QPDFWriter::setMinimumPDFVersion(std::string const& version)
{
    setMinimumPDFVersion(version, 0);
}

void
QPDFWriter::setMinimumPDFVersion(std::string const& version,
                                 int extension_level)
{
    bool set_version = false;
    bool set_extension_level = false;
    if (this->min_pdf_version.empty())
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
	parseVersion(this->min_pdf_version, min_major, min_minor);
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
            if (extension_level > this->min_extension_level)
            {
                QTC::TC("qpdf", "QPDFWriter increasing extension level");
                set_extension_level = true;
            }
	}
    }

    if (set_version)
    {
	this->min_pdf_version = version;
    }
    if (set_extension_level)
    {
        this->min_extension_level = extension_level;
    }
}

void
QPDFWriter::forcePDFVersion(std::string const& version)
{
    forcePDFVersion(version, 0);
}

void
QPDFWriter::forcePDFVersion(std::string const& version,
                            int extension_level)
{
    this->forced_pdf_version = version;
    this->forced_extension_level = extension_level;
}

void
QPDFWriter::setExtraHeaderText(std::string const& text)
{
    this->extra_header_text = text;
    if ((this->extra_header_text.length() > 0) &&
        (*(this->extra_header_text.rbegin()) != '\n'))
    {
        QTC::TC("qpdf", "QPDFWriter extra header text add newline");
        this->extra_header_text += "\n";
    }
    else
    {
        QTC::TC("qpdf", "QPDFWriter extra header text no newline");
    }
}

void
QPDFWriter::setStaticID(bool val)
{
    this->static_id = val;
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
    this->suppress_original_object_ids = val;
}

void
QPDFWriter::setPreserveEncryption(bool val)
{
    this->preserve_encryption = val;
}

void
QPDFWriter::setLinearization(bool val)
{
    this->linearized = val;
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
	allow_accessibility, allow_extract, print, modify);
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
	allow_accessibility, allow_extract, print, modify);
    this->encrypt_use_aes = use_aes;
    this->encrypt_metadata = encrypt_metadata;
    setEncryptionParameters(user_password, owner_password, 4, 4, 16, clear);
}

void
QPDFWriter::interpretR3EncryptionParameters(
    std::set<int>& clear,
    char const* user_password, char const* owner_password,
    bool allow_accessibility, bool allow_extract,
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

    if (! allow_accessibility)
    {
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
    QPDF::compute_encryption_O_U(
	user_password, owner_password, V, R, key_len, P,
	this->encrypt_metadata, this->id1, O, U);
    setEncryptionParametersInternal(
	V, R, key_len, P, O, U, "", "", "", this->id1, user_password);
}

void
QPDFWriter::copyEncryptionParameters(QPDF& qpdf)
{
    this->preserve_encryption = false;
    generateID();
    QPDFObjectHandle trailer = qpdf.getTrailer();
    if (trailer.hasKey("/Encrypt"))
    {
        this->id1 =
            trailer.getKey("/ID").getArrayItem(0).getStringValue();
	QPDFObjectHandle encrypt = trailer.getKey("/Encrypt");
	int V = encrypt.getKey("/V").getIntValue();
	int key_len = 5;
	if (V > 1)
	{
	    key_len = encrypt.getKey("/Length").getIntValue() / 8;
	}
	if (encrypt.hasKey("/EncryptMetadata") &&
	    encrypt.getKey("/EncryptMetadata").isBool())
	{
	    this->encrypt_metadata =
		encrypt.getKey("/EncryptMetadata").getBoolValue();
	}
        if (V >= 4)
        {
            if (encrypt.hasKey("/CF") &&
                encrypt.getKey("/CF").isDictionary() &&
                encrypt.hasKey("/StmF") &&
                encrypt.getKey("/StmF").isName())
            {
                // Determine whether to use AES from StmF.  QPDFWriter
                // can't write files with different StrF and StmF.
                QPDFObjectHandle CF = encrypt.getKey("/CF");
                QPDFObjectHandle StmF = encrypt.getKey("/StmF");
                if (CF.hasKey(StmF.getName()) &&
                    CF.getKey(StmF.getName()).isDictionary())
                {
                    QPDFObjectHandle StmF_data = CF.getKey(StmF.getName());
                    if (StmF_data.hasKey("/CFM") &&
                        StmF_data.getKey("/CFM").isName() &&
                        StmF_data.getKey("/CFM").getName() == "/AESV2")
                    {
                        this->encrypt_use_aes = true;
                    }
                }
            }
        }
	QTC::TC("qpdf", "QPDFWriter copy encrypt metadata",
		this->encrypt_metadata ? 0 : 1);
        QTC::TC("qpdf", "QPDFWriter copy use_aes",
                this->encrypt_use_aes ? 0 : 1);
	setEncryptionParametersInternal(
	    V,
	    encrypt.getKey("/R").getIntValue(),
    	    key_len,
	    encrypt.getKey("/P").getIntValue(),
	    encrypt.getKey("/O").getStringValue(),
	    encrypt.getKey("/U").getStringValue(),
            "",                 // XXXX OE
            "",                 // XXXX UE
            "",                 // XXXX Perms
	    this->id1,		// this->id1 == the other file's id1
	    qpdf.getPaddedUserPassword());
    }
}

void
QPDFWriter::disableIncompatibleEncryption(int major, int minor,
                                          int extension_level)
{
    if (! this->encrypted)
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
	int V = atoi(encryption_dictionary["/V"].c_str());
	int R = atoi(encryption_dictionary["/R"].c_str());
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
	    if (this->encrypt_use_aes)
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
	this->encrypted = false;
    }
}

void
QPDFWriter::parseVersion(std::string const& version,
			 int& major, int& minor) const
{
    major = atoi(version.c_str());
    minor = 0;
    size_t p = version.find('.');
    if ((p != std::string::npos) && (version.length() > p))
    {
	minor = atoi(version.substr(p + 1).c_str());
    }
    std::string tmp = QUtil::int_to_string(major) + "." +
	QUtil::int_to_string(minor);
    if (tmp != version)
    {
	throw std::logic_error(
	    "INTERNAL ERROR: QPDFWriter::parseVersion"
	    " called with invalid version number " + version);
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
    int V, int R, int key_len, long P,
    std::string const& O, std::string const& U,
    std::string const& OE, std::string const& UE, std::string const& Perms,
    std::string const& id1, std::string const& user_password)
{
    // XXXX OE, UE, Perms, V=5

    encryption_dictionary["/Filter"] = "/Standard";
    encryption_dictionary["/V"] = QUtil::int_to_string(V);
    encryption_dictionary["/Length"] = QUtil::int_to_string(key_len * 8);
    encryption_dictionary["/R"] = QUtil::int_to_string(R);
    encryption_dictionary["/P"] = QUtil::int_to_string(P);
    encryption_dictionary["/O"] = QPDF_String(O).unparse(true);
    encryption_dictionary["/U"] = QPDF_String(U).unparse(true);
    if (V >= 5)
    {
        setMinimumPDFVersion("1.4");
    }
    if (R >= 5)
    {
        setMinimumPDFVersion("1.7", 3);
    }
    else if (R == 4)
    {
        setMinimumPDFVersion(this->encrypt_use_aes ? "1.6" : "1.5");
    }
    else if (R == 3)
    {
        setMinimumPDFVersion("1.4");
    }
    else
    {
        setMinimumPDFVersion("1.3");
    }

    if ((R >= 4) && (! encrypt_metadata))
    {
	encryption_dictionary["/EncryptMetadata"] = "false";
    }
    if (V == 4)
    {
	// The spec says the value for the crypt filter key can be
	// anything, and xpdf seems to agree.  However, Adobe Reader
	// won't open our files unless we use /StdCF.
	encryption_dictionary["/StmF"] = "/StdCF";
	encryption_dictionary["/StrF"] = "/StdCF";
	std::string method = (this->encrypt_use_aes ? "/AESV2" : "/V2");
	encryption_dictionary["/CF"] =
	    "<< /StdCF << /AuthEvent /DocOpen /CFM " + method + " >> >>";
    }

    this->encrypted = true;
    QPDF::EncryptionData encryption_data(
	V, R, key_len, P, O, U, OE, UE, Perms, id1, this->encrypt_metadata);
    this->encryption_key = QPDF::compute_encryption_key(
	user_password, encryption_data);
}

void
QPDFWriter::setDataKey(int objid)
{
    this->cur_data_key = QPDF::compute_data_key(
	this->encryption_key, objid, 0, this->encrypt_use_aes);
}

int
QPDFWriter::bytesNeeded(unsigned long long n)
{
    int bytes = 0;
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
    assert(bytes <= sizeof(unsigned long long));
    unsigned char data[sizeof(unsigned long long)];
    for (unsigned int i = 0; i < bytes; ++i)
    {
	data[bytes - i - 1] = (unsigned char)(val & 0xff);
	val >>= 8;
    }
    this->pipeline->write(data, bytes);
}

void
QPDFWriter::writeString(std::string const& str)
{
    this->pipeline->write((unsigned char*)str.c_str(), str.length());
}

void
QPDFWriter::writeBuffer(PointerHolder<Buffer>& b)
{
    this->pipeline->write(b->getBuffer(), b->getSize());
}

void
QPDFWriter::writeStringQDF(std::string const& str)
{
    if (this->qdf_mode)
    {
	writeString(str);
    }
}

void
QPDFWriter::writeStringNoQDF(std::string const& str)
{
    if (! this->qdf_mode)
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
    this->pipeline_stack.push_back(p);
    return p;
}

void
QPDFWriter::initializePipelineStack(Pipeline *p)
{
    this->pipeline = new Pl_Count("qpdf count", p);
    to_delete.push_back(this->pipeline);
    this->pipeline_stack.push_back(this->pipeline);
}

void
QPDFWriter::activatePipelineStack()
{
    Pl_Count* c = new Pl_Count("count", this->pipeline_stack.back());
    this->pipeline_stack.push_back(c);
    this->pipeline = c;
}

void
QPDFWriter::popPipelineStack(PointerHolder<Buffer>* bp)
{
    assert(this->pipeline_stack.size() >= 2);
    this->pipeline->finish();
    assert(dynamic_cast<Pl_Count*>(this->pipeline_stack.back()) ==
	   this->pipeline);
    delete this->pipeline_stack.back();
    this->pipeline_stack.pop_back();
    while (dynamic_cast<Pl_Count*>(this->pipeline_stack.back()) == 0)
    {
	Pipeline* p = this->pipeline_stack.back();
	this->pipeline_stack.pop_back();
	Pl_Buffer* buf = dynamic_cast<Pl_Buffer*>(p);
	if (bp && buf)
	{
	    *bp = buf->getBuffer();
	}
	delete p;
    }
    this->pipeline = dynamic_cast<Pl_Count*>(this->pipeline_stack.back());
}

void
QPDFWriter::adjustAESStreamLength(size_t& length)
{
    if (this->encrypted && (! this->cur_data_key.empty()) &&
	this->encrypt_use_aes)
    {
	// Stream length will be padded with 1 to 16 bytes to end up
	// as a multiple of 16.  It will also be prepended by 16 bits
	// of random data.
	length += 32 - (length & 0xf);
    }
}

void
QPDFWriter::pushEncryptionFilter()
{
    if (this->encrypted && (! this->cur_data_key.empty()))
    {
	Pipeline* p = 0;
	if (this->encrypt_use_aes)
	{
	    p = new Pl_AES_PDF(
		"aes stream encryption", this->pipeline, true,
		(unsigned char*) this->cur_data_key.c_str(),
                (unsigned int)this->cur_data_key.length());
	}
	else
	{
	    p = new Pl_RC4("rc4 stream encryption", this->pipeline,
			   (unsigned char*) this->cur_data_key.c_str(),
			   (unsigned int)this->cur_data_key.length());
	}
	pushPipeline(p);
    }
    // Must call this unconditionally so we can call popPipelineStack
    // to balance pushEncryptionFilter().
    activatePipelineStack();
}

void
QPDFWriter::pushDiscardFilter()
{
    pushPipeline(new Pl_Discard());
    activatePipelineStack();
}

int
QPDFWriter::openObject(int objid)
{
    if (objid == 0)
    {
	objid = this->next_objid++;
    }
    this->xref[objid] = QPDFXRefEntry(1, pipeline->getCount(), 0);
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
    this->lengths[objid] = pipeline->getCount() - this->xref[objid].getOffset();
}

void
QPDFWriter::assignCompressedObjectNumbers(int objid)
{
    if (this->object_stream_to_objects.count(objid) == 0)
    {
	return;
    }

    // Reserve numbers for the objects that belong to this object
    // stream.
    for (std::set<int>::iterator iter =
	     this->object_stream_to_objects[objid].begin();
	 iter != this->object_stream_to_objects[objid].end();
	 ++iter)
    {
	obj_renumber[*iter] = next_objid++;
    }
}

void
QPDFWriter::enqueueObject(QPDFObjectHandle object)
{
    if (object.isIndirect())
    {
        if (object.getOwningQPDF() != &(this->pdf))
        {
            QTC::TC("qpdf", "QPDFWriter foreign object");
            throw std::logic_error(
                "QPDFObjectHandle from different QPDF found while writing."
                "  Use QPDF::copyForeignObject to add objects from"
                " another file.");
        }

	if (object.isNull())
	{
	    // This is a place-holder object for an object stream
	}
	int objid = object.getObjectID();

	if (obj_renumber.count(objid) == 0)
	{
	    if (this->object_to_object_stream.count(objid))
	    {
		// This is in an object stream.  Don't process it
		// here.  Instead, enqueue the object stream.
		int stream_id = this->object_to_object_stream[objid];
		enqueueObject(this->pdf.getObjectByID(stream_id, 0));
	    }
	    else
	    {
		object_queue.push_back(object);
		obj_renumber[objid] = next_objid++;

		if (this->object_stream_to_objects.count(objid))
		{
		    // For linearized files, uncompressed objects go
		    // at end, and we take care of assigning numbers
		    // to them elsewhere.
		    if (! this->linearized)
		    {
			assignCompressedObjectNumbers(objid);
		    }
		}
		else if ((! this->direct_stream_lengths) && object.isStream())
		{
		    // reserve next object ID for length
		    ++next_objid;
		}
	    }
	}
    }
    else if (object.isArray())
    {
	int n = object.getArrayNItems();
	for (int i = 0; i < n; ++i)
	{
	    if (! this->linearized)
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
	    if (! this->linearized)
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
    if (! this->linearized)
    {
	enqueueObject(child);
    }
    if (child.isIndirect())
    {
	int old_id = child.getObjectID();
	int new_id = obj_renumber[old_id];
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
                         qpdf_offset_t prev)
{
    QPDFObjectHandle trailer = pdf.getTrailer();
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
		    qpdf_offset_t pos = this->pipeline->getCount();
		    writeString(QUtil::int_to_string(prev));
		    int nspaces = (int)(pos - this->pipeline->getCount() + 21);
		    assert(nspaces >= 0);
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
    writeString(QPDF_String(this->id1).unparse(true));
    writeString(QPDF_String(this->id2).unparse(true));
    writeString("]");

    if (which != t_lin_second)
    {
	// Write reference to encryption dictionary
	if (this->encrypted)
	{
	    writeString(" /Encrypt ");
	    writeString(QUtil::int_to_string(this->encryption_dict_objid));
	    writeString(" 0 R");
	}
    }

    writeStringQDF("\n");
    writeStringNoQDF(" ");
    writeString(">>");
}

void
QPDFWriter::unparseObject(QPDFObjectHandle object, int level,
			  unsigned int flags)
{
    unparseObject(object, level, flags, 0, false);
}

void
QPDFWriter::unparseObject(QPDFObjectHandle object, int level,
			  unsigned int flags, size_t stream_length,
                          bool compress)
{
    unsigned int child_flags = flags & ~f_stream & ~f_in_extensions;

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
        // Handle special cases for specific dictionaries.

        // Extensions dictionaries are complicated.  We have one of
        // several cases:
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
        // We may be in the root dictionary, or we may be inside the
        // extensions dictionary itself.  The latter is determined by
        // the presence of the f_in_extensions flag.

        bool is_root = false;
        bool have_extensions_other = false;
        bool have_extensions_adbe = false;

        QPDFObjectHandle extensions;
        if (object.getObjectID() == pdf.getRoot().getObjectID())
        {
            is_root = true;
            if (object.hasKey("/Extensions") &&
                object.getKey("/Extensions").isDictionary())
            {
                extensions = object.getKey("/Extensions");
            }
        }
        else if (flags & f_in_extensions)
        {
            extensions = object;
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

        bool need_extensions_adbe = (this->final_extension_level > 0);

        bool write_new_extensions = false;
        bool write_new_adbe = false;
        bool suppress_existing_extensions = false;
        bool suppress_existing_adbe = false;
        if (is_root)
        {
            if (need_extensions_adbe)
            {
                if (! (have_extensions_other || have_extensions_adbe))
                {
                    // We need Extensions and don't have it.  Create
                    // it here.
                    QTC::TC("qpdf", "QPDFWriter create Extensions",
                            this->qdf_mode ? 0 : 1);
                    write_new_extensions = true;
                    suppress_existing_extensions = true;
                }
                else
                {
                    // Preserve existing Extensions and do the work
                    // in the extensions dictionary.
                }
            }
            else if (! have_extensions_other)
            {
                // We have Extensions dictionary and don't want one.
                suppress_existing_extensions = true;
                if (have_extensions_adbe)
                {
                    QTC::TC("qpdf", "QPDFWriter remove existing Extensions");
                }
            }
        }
        else if (flags & f_in_extensions)
        {
            QTC::TC("qpdf", "QPDFWriter preserve Extensions");
            QPDFObjectHandle adbe = extensions.getKey("/ADBE");
            if (adbe.isDictionary() &&
                adbe.hasKey("/BaseVersion") &&
                adbe.getKey("/BaseVersion").isName() &&
                (adbe.getKey("/BaseVersion").getName() ==
                 "/" + this->final_pdf_version) &&
                adbe.hasKey("/ExtensionLevel") &&
                adbe.getKey("/ExtensionLevel").isInteger() &&
                (adbe.getKey("/ExtensionLevel").getIntValue() ==
                 this->final_extension_level))
            {
                QTC::TC("qpdf", "QPDFWriter preserve ADBE");
            }
            else
            {
                suppress_existing_adbe = true;
                if (need_extensions_adbe)
                {
                    write_new_adbe = true;
                }
            }
        }

	writeString("<<");
	writeStringQDF("\n");

        if (write_new_extensions || write_new_adbe)
        {
            writeStringQDF(indent);
            writeStringQDF("  ");
            writeStringNoQDF(" ");
            if (write_new_extensions)
            {
                writeString("/Extensions << ");
            }
            writeString("/ADBE << /BaseVersion /");
            writeString(this->final_pdf_version);
            writeString(" /ExtensionLevel ");
            writeString(QUtil::int_to_string(this->final_extension_level));
            writeString(" >>");
            if (write_new_extensions)
            {
                writeString(" >>");
            }
            writeStringQDF("\n");
        }

	std::set<std::string> keys = object.getKeys();
	for (std::set<std::string>::iterator iter = keys.begin();
	     iter != keys.end(); ++iter)
	{
	    // I'm not fully clear on /Crypt keys in /DecodeParms.  If
	    // one is found, we refuse to filter, so we should be
	    // safe.
	    std::string const& key = *iter;
	    if ((flags & f_filtered) &&
		((key == "/Filter") ||
		 (key == "/DecodeParms")))
	    {
		continue;
	    }
	    if ((flags & f_stream) && (key == "/Length"))
	    {
		continue;
	    }

            bool is_extensions = (is_root && (key == "/Extensions"));
            if (suppress_existing_extensions && is_extensions)
            {
                QTC::TC("qpdf", "QPDFWriter skip Extensions");
                continue;
            }
            if (suppress_existing_adbe && (key == "/ADBE"))
            {
                QTC::TC("qpdf", "QPDFWriter skip ADBE");
                continue;
            }


	    writeStringQDF(indent);
	    writeStringQDF("  ");
	    writeStringNoQDF(" ");
	    writeString(QPDF_Name::normalizeName(key));
	    writeString(" ");
            unparseChild(object.getKey(key), level + 1,
                         child_flags | (is_extensions ? f_in_extensions : 0));
	    writeStringQDF("\n");
	}

	if (flags & f_stream)
	{
	    writeStringQDF(indent);
	    writeStringQDF(" ");
	    writeString(" /Length ");

	    if (this->direct_stream_lengths)
	    {
		writeString(QUtil::int_to_string(stream_length));
	    }
	    else
	    {
		writeString(
		    QUtil::int_to_string(this->cur_stream_length_id));
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
	int old_id = object.getObjectID();
	int new_id = obj_renumber[old_id];
	if (! this->direct_stream_lengths)
	{
	    this->cur_stream_length_id = new_id + 1;
	}
	QPDFObjectHandle stream_dict = object.getDict();

	bool is_metadata = false;
	if (stream_dict.getKey("/Type").isName() &&
	    (stream_dict.getKey("/Type").getName() == "/Metadata"))
	{
	    is_metadata = true;
	}
	bool filter = (this->stream_data_mode != qpdf_s_preserve);
	if (this->stream_data_mode == qpdf_s_compress)
	{
	    // Don't filter if the stream is already compressed with
	    // FlateDecode.  We don't want to make it worse by getting
	    // rid of a predictor or otherwise messing with it.  We
	    // should also avoid messing with anything that's
	    // compressed with a lossy compression scheme, but we
	    // don't support any of those right now.
	    QPDFObjectHandle filter_obj = stream_dict.getKey("/Filter");
	    if (filter_obj.isName() &&
		((filter_obj.getName() == "/FlateDecode") ||
		 (filter_obj.getName() == "/Fl")))
	    {
		QTC::TC("qpdf", "QPDFWriter not recompressing /FlateDecode");
		filter = false;
	    }
	}
	bool normalize = false;
	bool compress = false;
	if (is_metadata &&
	    ((! this->encrypted) || (this->encrypt_metadata == false)))
	{
	    QTC::TC("qpdf", "QPDFWriter not compressing metadata");
	    filter = true;
	    compress = false;
	}
	else if (this->normalize_content && normalized_streams.count(old_id))
	{
	    normalize = true;
	    filter = true;
	}
	else if (filter && (this->stream_data_mode == qpdf_s_compress))
	{
	    compress = true;
	    QTC::TC("qpdf", "QPDFWriter compressing uncompressed stream");
	}

	flags |= f_stream;

	pushPipeline(new Pl_Buffer("stream data"));
	activatePipelineStack();
	bool filtered =
	    object.pipeStreamData(this->pipeline, filter, normalize, compress);
	PointerHolder<Buffer> stream_data;
	popPipelineStack(&stream_data);
	if (filtered)
	{
	    flags |= f_filtered;
	}
	else
	{
	    compress = false;
	}

	this->cur_stream_length = stream_data->getSize();
	if (is_metadata && this->encrypted && (! this->encrypt_metadata))
	{
	    // Don't encrypt stream data for the metadata stream
	    this->cur_data_key.clear();
	}
	adjustAESStreamLength(this->cur_stream_length);
	unparseObject(stream_dict, 0, flags, this->cur_stream_length, compress);
	writeString("\nstream\n");
	pushEncryptionFilter();
	writeBuffer(stream_data);
	char last_char = this->pipeline->getLastChar();
	popPipelineStack();

	if (this->qdf_mode)
	{
	    if (last_char != '\n')
	    {
		writeString("\n");
		this->added_newline = true;
	    }
	    else
	    {
		this->added_newline = false;
	    }
	}
	writeString("endstream");
    }
    else if (object.isString())
    {
	std::string val;
	if (this->encrypted &&
	    (! (flags & f_in_ostream)) &&
	    (! this->cur_data_key.empty()))
	{
	    val = object.getStringValue();
	    if (this->encrypt_use_aes)
	    {
		Pl_Buffer bufpl("encrypted string");
		Pl_AES_PDF pl("aes encrypt string", &bufpl, true,
			      (unsigned char const*)this->cur_data_key.c_str(),
                              (unsigned int)this->cur_data_key.length());
		pl.write((unsigned char*) val.c_str(), val.length());
		pl.finish();
		Buffer* buf = bufpl.getBuffer();
		val = QPDF_String(
		    std::string((char*)buf->getBuffer(),
				buf->getSize())).unparse(true);
		delete buf;
	    }
	    else
	    {
		char* tmp = QUtil::copy_string(val);
		size_t vlen = val.length();
		RC4 rc4((unsigned char const*)this->cur_data_key.c_str(),
			(int)this->cur_data_key.length());
		rc4.process((unsigned char*)tmp, (int)vlen);
		val = QPDF_String(std::string(tmp, vlen)).unparse();
		delete [] tmp;
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
    for (unsigned int i = 0; i < offsets.size(); ++i)
    {
	if (i != 0)
	{
	    writeStringQDF("\n");
	    writeStringNoQDF(" ");
	}
	writeString(QUtil::int_to_string(i + first_obj));
	writeString(" ");
	writeString(QUtil::int_to_string(offsets[i]));
    }
    writeString("\n");
}

void
QPDFWriter::writeObjectStream(QPDFObjectHandle object)
{
    // Note: object might be null if this is a place-holder for an
    // object stream that we are generating from scratch.

    int old_id = object.getObjectID();
    int new_id = obj_renumber[old_id];

    std::vector<qpdf_offset_t> offsets;
    qpdf_offset_t first = 0;

    // Generate stream itself.  We have to do this in two passes so we
    // can calculate offsets in the first pass.
    PointerHolder<Buffer> stream_buffer;
    int first_obj = -1;
    bool compressed = false;
    for (int pass = 1; pass <= 2; ++pass)
    {
	if (pass == 1)
	{
	    pushDiscardFilter();
	}
	else
	{
	    // Adjust offsets to skip over comment before first object

	    first = offsets[0];
	    for (std::vector<qpdf_offset_t>::iterator iter = offsets.begin();
		 iter != offsets.end(); ++iter)
	    {
		*iter -= first;
	    }

	    // Take one pass at writing pairs of numbers so we can get
	    // their size information
	    pushDiscardFilter();
	    writeObjectStreamOffsets(offsets, first_obj);
	    first += this->pipeline->getCount();
	    popPipelineStack();

	    // Set up a stream to write the stream data into a buffer.
	    Pipeline* next = pushPipeline(new Pl_Buffer("object stream"));
	    if (! ((this->stream_data_mode == qpdf_s_uncompress) ||
		   this->qdf_mode))
	    {
		compressed = true;
		next = pushPipeline(
		    new Pl_Flate("compress object stream", next,
				 Pl_Flate::a_deflate));
	    }
	    activatePipelineStack();
	    writeObjectStreamOffsets(offsets, first_obj);
	}

	int count = 0;
	for (std::set<int>::iterator iter =
		 this->object_stream_to_objects[old_id].begin();
	     iter != this->object_stream_to_objects[old_id].end();
	     ++iter, ++count)
	{
	    int obj = *iter;
	    int new_obj = this->obj_renumber[obj];
	    if (first_obj == -1)
	    {
		first_obj = new_obj;
	    }
	    if (this->qdf_mode)
	    {
		writeString("%% Object stream: object " +
			    QUtil::int_to_string(new_obj) + ", index " +
			    QUtil::int_to_string(count));
		if (! this->suppress_original_object_ids)
		{
		    writeString("; original object ID: " +
				QUtil::int_to_string(obj));
		}
		writeString("\n");
	    }
	    if (pass == 1)
	    {
		offsets.push_back(this->pipeline->getCount());
	    }
	    writeObject(this->pdf.getObjectByID(obj, 0), count);

	    this->xref[new_obj] = QPDFXRefEntry(2, new_id, count);
	}

	// stream_buffer will be initialized only for pass 2
	popPipelineStack(&stream_buffer);
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
    writeString(" /Length " + QUtil::int_to_string(length));
    writeStringQDF("\n ");
    if (compressed)
    {
	writeString(" /Filter /FlateDecode");
    }
    writeString(" /N " + QUtil::int_to_string(offsets.size()));
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
    if (this->encrypted)
    {
	QTC::TC("qpdf", "QPDFWriter encrypt object stream");
    }
    pushEncryptionFilter();
    writeBuffer(stream_buffer);
    popPipelineStack();
    writeString("endstream");
    this->cur_data_key.clear();
    closeObject(new_id);
}

void
QPDFWriter::writeObject(QPDFObjectHandle object, int object_stream_index)
{
    int old_id = object.getObjectID();

    if ((object_stream_index == -1) &&
	(this->object_stream_to_objects.count(old_id)))
    {
	writeObjectStream(object);
	return;
    }

    int new_id = obj_renumber[old_id];
    if (this->qdf_mode)
    {
	if (this->page_object_to_seq.count(old_id))
	{
	    writeString("%% Page ");
	    writeString(
		QUtil::int_to_string(
		    this->page_object_to_seq[old_id]));
	    writeString("\n");
	}
	if (this->contents_to_page_seq.count(old_id))
	{
	    writeString("%% Contents for page ");
	    writeString(
		QUtil::int_to_string(
		    this->contents_to_page_seq[old_id]));
	    writeString("\n");
	}
    }
    if (object_stream_index == -1)
    {
	if (this->qdf_mode && (! this->suppress_original_object_ids))
	{
	    writeString("%% Original object ID: " +
			QUtil::int_to_string(object.getObjectID()) + " " +
			QUtil::int_to_string(object.getGeneration()) + "\n");
	}
	openObject(new_id);
	setDataKey(new_id);
	unparseObject(object, 0, 0);
	this->cur_data_key.clear();
	closeObject(new_id);
    }
    else
    {
	unparseObject(object, 0, f_in_ostream);
	writeString("\n");
    }

    if ((! this->direct_stream_lengths) && object.isStream())
    {
	if (this->qdf_mode)
	{
	    if (this->added_newline)
	    {
		writeString("%QDF: ignore_newline\n");
	    }
	}
	openObject(new_id + 1);
	writeString(QUtil::int_to_string(this->cur_stream_length));
	closeObject(new_id + 1);
    }
}

void
QPDFWriter::generateID()
{
    // Note: we can't call generateID() at the time of construction
    // since the caller hasn't yet had a chance to call setStaticID(),
    // but we need to generate it before computing encryption
    // dictionary parameters.  This is why we call this function both
    // from setEncryptionParameters() and from write() and return
    // immediately if the ID has already been generated.

    if (! this->id2.empty())
    {
	return;
    }

    QPDFObjectHandle trailer = pdf.getTrailer();

    std::string result;

    if (this->static_id)
    {
	// For test suite use only...
	static unsigned char tmp[] = {0x31, 0x41, 0x59, 0x26,
                                      0x53, 0x58, 0x97, 0x93,
                                      0x23, 0x84, 0x62, 0x64,
                                      0x33, 0x83, 0x27, 0x95,
                                      0x00};
	result = (char*)tmp;
    }
    else
    {
	// The PDF specification has guidelines for creating IDs, but it
	// states clearly that the only thing that's really important is
	// that it is very likely to be unique.  We can't really follow
	// the guidelines in the spec exactly because we haven't written
	// the file yet.  This scheme should be fine though.

	std::string seed;
	seed += QUtil::int_to_string((int)QUtil::get_current_time());
	seed += " QPDF ";
	seed += this->filename;
	seed += " ";
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
	result = std::string((char*)digest, sizeof(MD5::Digest));
    }

    // If /ID already exists, follow the spec: use the original first
    // word and generate a new second word.  Otherwise, we'll use the
    // generated ID for both.

    this->id2 = result;
    if (trailer.hasKey("/ID"))
    {
	// Note: keep /ID from old file even if --static-id was given.
	this->id1 = trailer.getKey("/ID").getArrayItem(0).getStringValue();
    }
    else
    {
	this->id1 = this->id2;
    }
}

void
QPDFWriter::initializeSpecialStreams()
{
    // Mark all page content streams in case we are filtering or
    // normalizing.
    std::vector<QPDFObjectHandle> pages = pdf.getAllPages();
    int num = 0;
    for (std::vector<QPDFObjectHandle>::iterator iter = pages.begin();
	 iter != pages.end(); ++iter)
    {
	QPDFObjectHandle& page = *iter;
	this->page_object_to_seq[page.getObjectID()] = ++num;
	QPDFObjectHandle contents = page.getKey("/Contents");
	std::vector<int> contents_objects;
	if (contents.isArray())
	{
	    int n = contents.getArrayNItems();
	    for (int i = 0; i < n; ++i)
	    {
		contents_objects.push_back(
		    contents.getArrayItem(i).getObjectID());
	    }
	}
	else if (contents.isStream())
	{
	    contents_objects.push_back(contents.getObjectID());
	}

	for (std::vector<int>::iterator iter = contents_objects.begin();
	     iter != contents_objects.end(); ++iter)
	{
	    this->contents_to_page_seq[*iter] = num;
	    this->normalized_streams.insert(*iter);
	}
    }
}

void
QPDFWriter::preserveObjectStreams()
{
    this->pdf.getObjectStreamData(this->object_to_object_stream);
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

    std::vector<int> const& eligible = this->pdf.getCompressibleObjects();
    unsigned int n_object_streams =
        (unsigned int)((eligible.size() + 99) / 100);
    unsigned int n_per = (unsigned int)(eligible.size() / n_object_streams);
    if (n_per * n_object_streams < eligible.size())
    {
	++n_per;
    }
    unsigned int n = 0;
    int cur_ostream = 0;
    for (std::vector<int>::const_iterator iter = eligible.begin();
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
	    cur_ostream = this->pdf.makeIndirectObject(
		QPDFObjectHandle::newNull()).getObjectID();
	}
	this->object_to_object_stream[*iter] = cur_ostream;
	++n;
    }
}

void
QPDFWriter::prepareFileForWrite()
{
    // Remove keys from the trailer that necessarily have to be
    // replaced when writing the file.

    QPDFObjectHandle trailer = pdf.getTrailer();

    // Note that removing the encryption dictionary does not interfere
    // with reading encrypted files.  QPDF loads all the information
    // it needs from the encryption dictionary at the beginning and
    // never looks at it again.
    trailer.removeKey("/ID");
    trailer.removeKey("/Encrypt");
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

    // Do a traversal of the entire PDF file structure replacing all
    // indirect objects that QPDFWriter wants to be direct.  This
    // includes stream lengths, stream filtering parameters, and
    // document extension level information.  Also replace all
    // indirect null references with direct nulls.  This way, the only
    // indirect nulls queued for output will be object stream place
    // holders.

    std::list<QPDFObjectHandle> queue;
    queue.push_back(pdf.getTrailer());
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
	    visited.insert(node.getObjectID());
	}

	if (node.isArray())
	{
	    int nitems = node.getArrayNItems();
	    for (int i = 0; i < nitems; ++i)
	    {
		QPDFObjectHandle oh = node.getArrayItem(i);
                if (oh.isIndirect() && oh.isNull())
                {
                    QTC::TC("qpdf", "QPDFWriter flatten array null");
                    oh.makeDirect();
                    node.setArrayItem(i, oh);
                }
		else if (! oh.isScalar())
		{
		    queue.push_back(oh);
		}
	    }
	}
	else if (node.isDictionary() || node.isStream())
	{
            bool is_stream = false;
            bool is_root = false;
	    QPDFObjectHandle dict = node;
	    if (node.isStream())
	    {
                is_stream = true;
		dict = node.getDict();
	    }
            else if (pdf.getRoot().getObjectID() == node.getObjectID())
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
                         (key == "/Filter") ||
                         (key == "/DecodeParms")))
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
QPDFWriter::write()
{
    // Do preliminary setup

    if (this->linearized)
    {
	this->qdf_mode = false;
    }

    if (this->qdf_mode)
    {
	if (! this->normalize_content_set)
	{
	    this->normalize_content = true;
	}
	if (! this->stream_data_mode_set)
	{
	    this->stream_data_mode = qpdf_s_uncompress;
	}
    }

    if (this->encrypted)
    {
	// Encryption has been explicitly set
	this->preserve_encryption = false;
    }
    else if (this->normalize_content ||
	     (this->stream_data_mode == qpdf_s_uncompress) ||
	     this->qdf_mode)
    {
	// Encryption makes looking at contents pretty useless.  If
	// the user explicitly encrypted though, we still obey that.
	this->preserve_encryption = false;
    }

    if (preserve_encryption)
    {
	copyEncryptionParameters(this->pdf);
    }

    if (! this->forced_pdf_version.empty())
    {
	int major = 0;
	int minor = 0;
	parseVersion(this->forced_pdf_version, major, minor);
	disableIncompatibleEncryption(major, minor,
                                      this->forced_extension_level);
	if (compareVersions(major, minor, 1, 5) < 0)
	{
	    QTC::TC("qpdf", "QPDFWriter forcing object stream disable");
	    this->object_stream_mode = qpdf_o_disable;
	}
    }

    if (this->qdf_mode || this->normalize_content ||
	(this->stream_data_mode == qpdf_s_uncompress))
    {
	initializeSpecialStreams();
    }

    if (this->qdf_mode)
    {
	// Generate indirect stream lengths for qdf mode since fix-qdf
	// uses them for storing recomputed stream length data.
	// Certain streams such as object streams, xref streams, and
	// hint streams always get direct stream lengths.
	this->direct_stream_lengths = false;
    }

    switch (this->object_stream_mode)
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

    if (this->linearized)
    {
	// Page dictionaries are not allowed to be compressed objects.
	std::vector<QPDFObjectHandle> pages = pdf.getAllPages();
	for (std::vector<QPDFObjectHandle>::iterator iter = pages.begin();
	     iter != pages.end(); ++iter)
	{
	    QPDFObjectHandle& page = *iter;
	    int objid = page.getObjectID();
	    if (this->object_to_object_stream.count(objid))
	    {
		QTC::TC("qpdf", "QPDFWriter uncompressing page dictionary");
		this->object_to_object_stream.erase(objid);
	    }
	}
    }

    if (this->linearized || this->encrypted)
    {
    	// The document catalog is not allowed to be compressed in
    	// linearized files either.  It also appears that Adobe Reader
    	// 8.0.0 has a bug that prevents it from being able to handle
    	// encrypted files with compressed document catalogs, so we
    	// disable them in that case as well.
	int objid = pdf.getRoot().getObjectID();
	if (this->object_to_object_stream.count(objid))
	{
	    QTC::TC("qpdf", "QPDFWriter uncompressing root");
	    this->object_to_object_stream.erase(objid);
	}
    }

    // Generate reverse mapping from object stream to objects
    for (std::map<int, int>::iterator iter =
	     this->object_to_object_stream.begin();
	 iter != this->object_to_object_stream.end(); ++iter)
    {
	int obj = (*iter).first;
	int stream = (*iter).second;
	this->object_stream_to_objects[stream].insert(obj);
	this->max_ostream_index =
	    std::max(this->max_ostream_index,
		     (int)this->object_stream_to_objects[stream].size() - 1);
    }

    if (! this->object_stream_to_objects.empty())
    {
	setMinimumPDFVersion("1.5");
    }

    generateID();

    prepareFileForWrite();

    if (this->linearized)
    {
	writeLinearized();
    }
    else
    {
	writeStandard();
    }

    this->pipeline->finish();
    if (this->close_file)
    {
	fclose(this->file);
    }
    this->file = 0;
    if (this->buffer_pipeline)
    {
	this->output_buffer = this->buffer_pipeline->getBuffer();
	this->buffer_pipeline = 0;
    }
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
    this->encryption_dict_objid = openObject(this->encryption_dict_objid);
    writeString("<<");
    for (std::map<std::string, std::string>::iterator iter =
	     this->encryption_dictionary.begin();
	 iter != this->encryption_dictionary.end(); ++iter)
    {
	writeString(" ");
	writeString((*iter).first);
	writeString(" ");
	writeString((*iter).second);
    }
    writeString(" >>");
    closeObject(this->encryption_dict_objid);
}

void
QPDFWriter::writeHeader()
{
    setMinimumPDFVersion(pdf.getPDFVersion(), pdf.getExtensionLevel());
    this->final_pdf_version = this->min_pdf_version;
    this->final_extension_level = this->min_extension_level;
    if (! this->forced_pdf_version.empty())
    {
	QTC::TC("qpdf", "QPDFWriter using forced PDF version");
	this->final_pdf_version = this->forced_pdf_version;
        this->final_extension_level = this->forced_extension_level;
    }

    writeString("%PDF-");
    writeString(this->final_pdf_version);
    // This string of binary characters would not be valid UTF-8, so
    // it really should be treated as binary.
    writeString("\n%\xbf\xf7\xa2\xfe\n");
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
    pdf.generateHintStream(
	this->xref, this->lengths, this->obj_renumber, hint_buffer, S, O);

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
    writeString(QUtil::int_to_string(hlen));
    writeString(" >>\nstream\n");

    if (this->encrypted)
    {
	QTC::TC("qpdf", "QPDFWriter encrypted hint stream");
    }
    pushEncryptionFilter();
    writeBuffer(hint_buffer);
    char last_char = this->pipeline->getLastChar();
    popPipelineStack();

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
    return writeXRefTable(which, first, last, size, 0, false, 0, 0, 0);
}

qpdf_offset_t
QPDFWriter::writeXRefTable(trailer_e which, int first, int last, int size,
			   qpdf_offset_t prev, bool suppress_offsets,
			   int hint_id, qpdf_offset_t hint_offset,
                           qpdf_offset_t hint_length)
{
    writeString("xref\n");
    writeString(QUtil::int_to_string(first));
    writeString(" ");
    writeString(QUtil::int_to_string(last - first + 1));
    qpdf_offset_t space_before_zero = this->pipeline->getCount();
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
		offset = this->xref[i].getOffset();
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
    writeTrailer(which, size, false, prev);
    writeString("\n");
    return space_before_zero;
}

qpdf_offset_t
QPDFWriter::writeXRefStream(int objid, int max_id, qpdf_offset_t max_offset,
			    trailer_e which, int first, int last, int size)
{
    return writeXRefStream(objid, max_id, max_offset,
			   which, first, last, size, 0, 0, 0, 0, false);
}

qpdf_offset_t
QPDFWriter::writeXRefStream(int xref_id, int max_id, qpdf_offset_t max_offset,
			    trailer_e which, int first, int last, int size,
			    qpdf_offset_t prev, int hint_id,
			    qpdf_offset_t hint_offset,
                            qpdf_offset_t hint_length,
			    bool skip_compression)
{
    qpdf_offset_t xref_offset = this->pipeline->getCount();
    qpdf_offset_t space_before_zero = xref_offset - 1;

    // field 1 contains offsets and object stream identifiers
    int f1_size = std::max(bytesNeeded(max_offset),
			   bytesNeeded(max_id));

    // field 2 contains object stream indices
    int f2_size = bytesNeeded(this->max_ostream_index);

    unsigned int esize = 1 + f1_size + f2_size;

    // Must store in xref table in advance of writing the actual data
    // rather than waiting for openObject to do it.
    this->xref[xref_id] = QPDFXRefEntry(1, pipeline->getCount(), 0);

    Pipeline* p = pushPipeline(new Pl_Buffer("xref stream"));
    bool compressed = false;
    if (! ((this->stream_data_mode == qpdf_s_uncompress) || this->qdf_mode))
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
		"pngify xref", p, Pl_PNGFilter::a_encode, esize, 0));
    }
    activatePipelineStack();
    for (int i = first; i <= last; ++i)
    {
	QPDFXRefEntry& e = this->xref[i];
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
		writeBinary(offset, f1_size);
		writeBinary(0, f2_size);
	    }
	    break;

	  case 2:
	    writeBinary(2, 1);
	    writeBinary(e.getObjStreamNumber(), f1_size);
	    writeBinary(e.getObjStreamIndex(), f2_size);
	    break;

	  default:
	    throw std::logic_error("invalid type writing xref stream");
	    break;
	}
    }
    PointerHolder<Buffer> xref_data;
    popPipelineStack(&xref_data);

    openObject(xref_id);
    writeString("<<");
    writeStringQDF("\n ");
    writeString(" /Type /XRef");
    writeStringQDF("\n ");
    writeString(" /Length " + QUtil::int_to_string(xref_data->getSize()));
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
    writeTrailer(which, size, true, prev);
    writeString("\nstream\n");
    writeBuffer(xref_data);
    writeString("\nendstream");
    closeObject(xref_id);
    return space_before_zero;
}

int
QPDFWriter::calculateXrefStreamPadding(int xref_bytes)
{
    // This routine is called right after a linearization first pass
    // xref stream has been written without compression.  Calculate
    // the amount of padding that would be required in the worst case,
    // assuming the number of uncompressed bytes remains the same.
    // The worst case for zlib is that the output is larger than the
    // input by 6 bytes plus 5 bytes per 16K, and then we'll add 10
    // extra bytes for number length increases.

    return 16 + (5 * ((xref_bytes + 16383) / 16384));
}

void
QPDFWriter::writeLinearized()
{
    // Optimize file and enqueue objects in order

    bool need_xref_stream = (! this->object_to_object_stream.empty());
    pdf.optimize(this->object_to_object_stream);

    std::vector<QPDFObjectHandle> part4;
    std::vector<QPDFObjectHandle> part6;
    std::vector<QPDFObjectHandle> part7;
    std::vector<QPDFObjectHandle> part8;
    std::vector<QPDFObjectHandle> part9;
    pdf.getLinearizedParts(this->object_to_object_stream,
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
        (int)(part7.size() + part8.size() + part9.size());
    int second_half_first_obj = 1;
    int after_second_half = 1 + second_half_uncompressed;
    this->next_objid = after_second_half;
    int second_half_xref = 0;
    if (need_xref_stream)
    {
	second_half_xref = this->next_objid++;
    }
    // Assign numbers to all compressed objects in the second half.
    std::vector<QPDFObjectHandle>* vecs2[] = {&part7, &part8, &part9};
    for (int i = 0; i < 3; ++i)
    {
	for (std::vector<QPDFObjectHandle>::iterator iter = (*vecs2[i]).begin();
	     iter != (*vecs2[i]).end(); ++iter)
	{
	    assignCompressedObjectNumbers((*iter).getObjectID());
	}
    }
    int second_half_end = this->next_objid - 1;
    int second_trailer_size = this->next_objid;

    // First half objects
    int first_half_start = this->next_objid;
    int lindict_id = this->next_objid++;
    int first_half_xref = 0;
    if (need_xref_stream)
    {
	first_half_xref = this->next_objid++;
    }
    int part4_first_obj = this->next_objid;
    this->next_objid += part4.size();
    int after_part4 = this->next_objid;
    if (this->encrypted)
    {
	this->encryption_dict_objid = this->next_objid++;
    }
    int hint_id = this->next_objid++;
    int part6_first_obj = this->next_objid;
    this->next_objid += part6.size();
    int after_part6 = this->next_objid;
    // Assign numbers to all compressed objects in the first half
    std::vector<QPDFObjectHandle>* vecs1[] = {&part4, &part6};
    for (int i = 0; i < 2; ++i)
    {
	for (std::vector<QPDFObjectHandle>::iterator iter = (*vecs1[i]).begin();
	     iter != (*vecs1[i]).end(); ++iter)
	{
	    assignCompressedObjectNumbers((*iter).getObjectID());
	}
    }
    int first_half_end = this->next_objid - 1;
    int first_trailer_size = this->next_objid;

    int part4_end_marker = part4.back().getObjectID();
    int part6_end_marker = part6.back().getObjectID();
    qpdf_offset_t space_before_zero = 0;
    qpdf_offset_t file_size = 0;
    qpdf_offset_t part6_end_offset = 0;
    qpdf_offset_t first_half_max_obj_offset = 0;
    qpdf_offset_t second_xref_offset = 0;
    qpdf_offset_t first_xref_end = 0;
    qpdf_offset_t second_xref_end = 0;

    this->next_objid = part4_first_obj;
    enqueuePart(part4);
    assert(this->next_objid = after_part4);
    this->next_objid = part6_first_obj;
    enqueuePart(part6);
    assert(this->next_objid == after_part6);
    this->next_objid = second_half_first_obj;
    enqueuePart(part7);
    enqueuePart(part8);
    enqueuePart(part9);
    assert(this->next_objid == after_second_half);

    qpdf_offset_t hint_length = 0;
    PointerHolder<Buffer> hint_buffer;

    // Write file in two passes.  Part numbers refer to PDF spec 1.4.

    for (int pass = 1; pass <= 2; ++pass)
    {
	if (pass == 1)
	{
	    pushDiscardFilter();
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

	qpdf_offset_t pos = this->pipeline->getCount();
	openObject(lindict_id);
	writeString("<<");
	if (pass == 2)
	{
	    std::vector<QPDFObjectHandle> const& pages = pdf.getAllPages();
	    int first_page_object = obj_renumber[pages[0].getObjectID()];
	    int npages = (int)pages.size();

	    writeString(" /Linearized 1 /L ");
	    writeString(QUtil::int_to_string(file_size + hint_length));
	    // Implementation note 121 states that a space is
	    // mandatory after this open bracket.
	    writeString(" /H [ ");
	    writeString(QUtil::int_to_string(this->xref[hint_id].getOffset()));
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
	int spaces = (pos - this->pipeline->getCount() + pad);
	assert(spaces >= 0);
	writePad(spaces);
	writeString("\n");

        // If the user supplied any additional header text, write it
        // here after the linearization parameter dictionary.
        writeString(this->extra_header_text);

	// Part 3: first page cross reference table and trailer.

	qpdf_offset_t first_xref_offset = this->pipeline->getCount();
	qpdf_offset_t hint_offset = 0;
	if (pass == 2)
	{
	    hint_offset = this->xref[hint_id].getOffset();
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
	    pos = this->pipeline->getCount();
	    writeXRefStream(first_half_xref, first_half_end,
			    first_half_max_obj_offset,
			    t_lin_first, first_half_start, first_half_end,
			    first_trailer_size,
			    hint_length + second_xref_offset,
			    hint_id, hint_offset, hint_length,
			    (pass == 1));
	    qpdf_offset_t endpos = this->pipeline->getCount();
	    if (pass == 1)
	    {
		// Pad so we have enough room for the real xref
		// stream.
		writePad(calculateXrefStreamPadding(endpos - pos));
		first_xref_end = this->pipeline->getCount();
	    }
	    else
	    {
		// Pad so that the next object starts at the same
		// place as in pass 1.
		writePad(first_xref_end - endpos);

		// A failure of this insertion means we didn't allow
		// enough padding for the first pass xref stream.
		assert(this->pipeline->getCount() == first_xref_end);
	    }
	    writeString("\n");
	}
	else
	{
	    writeXRefTable(t_lin_first, first_half_start, first_half_end,
			   first_trailer_size, hint_length + second_xref_offset,
			   (pass == 1), hint_id, hint_offset, hint_length);
	    writeString("startxref\n0\n%%EOF\n");
	}

	// Parts 4 through 9

	for (std::list<QPDFObjectHandle>::iterator iter =
		 this->object_queue.begin();
	     iter != this->object_queue.end(); ++iter)
	{
	    QPDFObjectHandle cur_object = (*iter);
	    if (cur_object.getObjectID() == part6_end_marker)
	    {
		first_half_max_obj_offset = this->pipeline->getCount();
	    }
	    writeObject(cur_object);
	    if (cur_object.getObjectID() == part4_end_marker)
	    {
		if (this->encrypted)
		{
		    writeEncryptionDictionary();
		}
		if (pass == 1)
		{
		    this->xref[hint_id] =
			QPDFXRefEntry(1, this->pipeline->getCount(), 0);
		}
		else
		{
		    // Part 5: hint stream
		    writeBuffer(hint_buffer);
		}
	    }
	    if (cur_object.getObjectID() == part6_end_marker)
	    {
		part6_end_offset = this->pipeline->getCount();
	    }
	}

	// Part 10: overflow hint stream -- not used

	// Part 11: main cross reference table and trailer

	second_xref_offset = this->pipeline->getCount();
	if (need_xref_stream)
	{
	    pos = this->pipeline->getCount();
	    space_before_zero =
		writeXRefStream(second_half_xref,
				second_half_end, second_xref_offset,
				t_lin_second, 0, second_half_end,
				second_trailer_size,
				0, 0, 0, 0, (pass == 1));
	    qpdf_offset_t endpos = this->pipeline->getCount();

	    if (pass == 1)
	    {
		// Pad so we have enough room for the real xref
		// stream.  See comments for previous xref stream on
		// how we calculate the padding.
		writePad(calculateXrefStreamPadding(endpos - pos));
		writeString("\n");
		second_xref_end = this->pipeline->getCount();
	    }
	    else
	    {
		// Make the file size the same.
		qpdf_offset_t pos = this->pipeline->getCount();
		writePad(second_xref_end + hint_length - 1 - pos);
		writeString("\n");

		// If this assertion fails, maybe we didn't have
		// enough padding above.
		assert(this->pipeline->getCount() ==
		       (qpdf_offset_t)(second_xref_end + hint_length));
	    }
	}
	else
	{
	    space_before_zero =
		writeXRefTable(t_lin_second, 0, second_half_end,
			       second_trailer_size);
	}
	writeString("startxref\n");
	writeString(QUtil::int_to_string(first_xref_offset));
	writeString("\n%%EOF\n");

	if (pass == 1)
	{
	    // Close first pass pipeline
	    file_size = this->pipeline->getCount();
	    popPipelineStack();

	    // Save hint offset since it will be set to zero by
	    // calling openObject.
	    qpdf_offset_t hint_offset = this->xref[hint_id].getOffset();

	    // Write hint stream to a buffer
	    pushPipeline(new Pl_Buffer("hint buffer"));
	    activatePipelineStack();
	    writeHintStream(hint_id);
	    popPipelineStack(&hint_buffer);
	    hint_length = (qpdf_offset_t)hint_buffer->getSize();

	    // Restore hint offset
	    this->xref[hint_id] = QPDFXRefEntry(1, hint_offset, 0);
	}
    }
}

void
QPDFWriter::writeStandard()
{
    // Start writing

    writeHeader();
    writeString(this->extra_header_text);

    // Put root first on queue.
    QPDFObjectHandle trailer = pdf.getTrailer();
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

    // Now start walking queue, output each object
    while (this->object_queue.size())
    {
	QPDFObjectHandle cur_object = this->object_queue.front();
	this->object_queue.pop_front();
	writeObject(cur_object);
    }

    // Write out the encryption dictionary, if any
    if (this->encrypted)
    {
	writeEncryptionDictionary();
    }

    // Now write out xref.  next_objid is now the number of objects.
    qpdf_offset_t xref_offset = this->pipeline->getCount();
    if (this->object_stream_to_objects.empty())
    {
	// Write regular cross-reference table
	// Write regular cross-reference table
	writeXRefTable(t_normal, 0, this->next_objid - 1, this->next_objid);
    }
    else
    {
	// Write cross-reference stream.
	int xref_id = this->next_objid++;
	writeXRefStream(xref_id, xref_id, xref_offset, t_normal,
			0, this->next_objid - 1, this->next_objid);
    }
    writeString("startxref\n");
    writeString(QUtil::int_to_string(xref_offset));
    writeString("\n%%EOF\n");
}
