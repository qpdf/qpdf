#include <qpdf/QPDFWriter.hh>

#include <assert.h>
#include <qpdf/Pl_StdioFile.hh>
#include <qpdf/Pl_Count.hh>
#include <qpdf/Pl_Discard.hh>
#include <qpdf/Pl_Buffer.hh>
#include <qpdf/Pl_RC4.hh>
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

QPDFWriter::QPDFWriter(QPDF& pdf, char const* filename) :
    pdf(pdf),
    filename(filename),
    file(0),
    close_file(false),
    normalize_content_set(false),
    normalize_content(false),
    stream_data_mode_set(false),
    stream_data_mode(s_compress),
    qdf_mode(false),
    static_id(false),
    suppress_original_object_ids(false),
    direct_stream_lengths(true),
    encrypted(false),
    preserve_encryption(true),
    linearized(false),
    object_stream_mode(o_preserve),
    encryption_dict_objid(0),
    next_objid(1),
    cur_stream_length_id(0),
    cur_stream_length(0),
    added_newline(false),
    max_ostream_index(0)
{
    if (filename == 0)
    {
	this->filename = "standard output";
	QTC::TC("qpdf", "QPDFWriter write to stdout");
	file = stdout;
	QUtil::binary_stdout();
    }
    else
    {
	QTC::TC("qpdf", "QPDFWriter write to file");
	file = QUtil::fopen_wrapper(std::string("open ") + filename,
				    fopen(filename, "wb+"));
	close_file = true;
    }
    Pipeline* p = new Pl_StdioFile("qdf output", file);
    to_delete.push_back(p);
    pipeline = new Pl_Count("qdf count", p);
    to_delete.push_back(pipeline);
    pipeline_stack.push_back(pipeline);
}

QPDFWriter::~QPDFWriter()
{
    if (file)
    {
	fclose(file);
    }
}

void
QPDFWriter::setObjectStreamMode(object_stream_e mode)
{
    this->object_stream_mode = mode;
}

void
QPDFWriter::setStreamDataMode(stream_data_e mode)
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
    bool set_version = false;
    if (this->min_pdf_version.empty())
    {
	set_version = true;
    }
    else
    {
	float v = atof(version.c_str());
	float mv = atof(this->min_pdf_version.c_str());
	if (v > mv)
	{
	    QTC::TC("qpdf", "QPDFWriter increasing minimum version");
	    set_version = true;
	}
    }

    if (set_version)
    {
	this->min_pdf_version = version;
    }
}

void
QPDFWriter::forcePDFVersion(std::string const& version)
{
    this->forced_pdf_version = version;
}

void
QPDFWriter::setStaticID(bool val)
{
    this->static_id = val;
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

    setMinimumPDFVersion("1.3");
    setEncryptionParameters(user_password, owner_password, 1, 2, 5, clear);
}

void
QPDFWriter::setR3EncryptionParameters(
    char const* user_password, char const* owner_password,
    bool allow_accessibility, bool allow_extract,
    r3_print_e print, r3_modify_e modify)
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

    std::set<int> clear;
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
      case r3p_none:
	clear.insert(3);	// any printing

      case r3p_low:
	clear.insert(12);	// high resolution printing

      case r3p_full:
	break;

	// no default so gcc warns for missing cases
    }

    switch (modify)
    {
      case r3m_none:
	clear.insert(11);	// document assembly

      case r3m_assembly:
	clear.insert(9);	// filling in form fields

      case r3m_form:
	clear.insert(6);	// modify annotations, fill in form fields

      case r3m_annotate:
	clear.insert(4);	// other modifications

      case r3m_all:
	break;

	// no default so gcc warns for missing cases
    }

    setMinimumPDFVersion("1.4");
    setEncryptionParameters(user_password, owner_password, 2, 3, 16, clear);
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
	/*XXX encrypt_metadata*/true, this->id1, O, U);
    setEncryptionParametersInternal(
	V, R, key_len, P, O, U, this->id1, user_password);
}

void
QPDFWriter::copyEncryptionParameters()
{
    generateID();
    QPDFObjectHandle trailer = this->pdf.getTrailer();
    if (trailer.hasKey("/Encrypt"))
    {
	QPDFObjectHandle encrypt = trailer.getKey("/Encrypt");
	int V = encrypt.getKey("/V").getIntValue();
	int key_len = 5;
	if (V > 1)
	{
	    key_len = encrypt.getKey("/Length").getIntValue() / 8;
	}
	setEncryptionParametersInternal(
	    V,
	    encrypt.getKey("/R").getIntValue(),
    	    key_len,
	    encrypt.getKey("/P").getIntValue(),
	    encrypt.getKey("/O").getStringValue(),
	    encrypt.getKey("/U").getStringValue(),
	    this->id1,		// this->id1 == the other file's id1
	    pdf.getPaddedUserPassword());
    }
}

void
QPDFWriter::setEncryptionParametersInternal(
    int V, int R, int key_len, long P,
    std::string const& O, std::string const& U,
    std::string const& id1, std::string const& user_password)
{
    encryption_dictionary["/Filter"] = "/Standard";
    encryption_dictionary["/V"] = QUtil::int_to_string(V);
    encryption_dictionary["/Length"] = QUtil::int_to_string(key_len * 8);
    encryption_dictionary["/R"] = QUtil::int_to_string(R);
    encryption_dictionary["/P"] = QUtil::int_to_string(P);
    encryption_dictionary["/O"] = QPDF_String(O).unparse(true);
    encryption_dictionary["/U"] = QPDF_String(U).unparse(true);
    this->encrypted = true;
    QPDF::EncryptionData encryption_data(V, R, key_len, P, O, U, this->id1,
					 /*XXX encrypt_metadata*/true);
    this->encryption_key = QPDF::compute_encryption_key(
	user_password, encryption_data);
}

void
QPDFWriter::setDataKey(int objid)
{
    this->cur_data_key = QPDF::compute_data_key(
	this->encryption_key, objid, 0, /*XXX use_aes */false);
}

int
QPDFWriter::bytesNeeded(unsigned long n)
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
QPDFWriter::writeBinary(unsigned long val, unsigned int bytes)
{
    assert(bytes <= sizeof(unsigned long));
    unsigned char data[sizeof(unsigned long)];
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
    this->pipeline->write(b.getPointer()->getBuffer(),
			  b.getPointer()->getSize());
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

Pipeline*
QPDFWriter::pushPipeline(Pipeline* p)
{
    assert(dynamic_cast<Pl_Count*>(p) == 0);
    this->pipeline_stack.push_back(p);
    return p;
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
QPDFWriter::pushEncryptionFilter()
{
    if (this->encrypted && (! this->cur_data_key.empty()))
    {
	Pipeline* p =
	    new Pl_RC4("stream encryption", this->pipeline,
		       (unsigned char*) this->cur_data_key.c_str(),
		       this->cur_data_key.length());
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
	if (object.isNull())
	{
	    // This is a place-holder object for an object stream
	}
	else if (object.isScalar())
	{
	    throw std::logic_error(
		"INTERNAL ERROR: QPDFWriter::enqueueObject: indirect scalar: " +
		std::string(this->filename) + " " +
		QUtil::int_to_string(object.getObjectID()) + " " +
		QUtil::int_to_string(object.getGeneration()));
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
	if (child.isScalar())
	{
	    throw std::logic_error(
		"INTERNAL ERROR: QPDFWriter::unparseChild: indirect scalar: " +
		QUtil::int_to_string(child.getObjectID()) + " " +
		QUtil::int_to_string(child.getGeneration()));
	}
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
QPDFWriter::writeTrailer(trailer_e which, int size, bool xref_stream, int prev)
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
		    int pos = this->pipeline->getCount();
		    writeString(QUtil::int_to_string(prev));
		    int nspaces = pos + 11 - this->pipeline->getCount();
		    assert(nspaces >= 0);
		    for (int i = 0; i < nspaces; ++i)
		    {
			writeString(" ");
		    }
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
			  unsigned int flags, int stream_length, bool compress)
{
    unsigned int child_flags = flags & ~f_stream;

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
	writeString("<<");
	writeStringQDF("\n");
	std::set<std::string> keys = object.getKeys();
	for (std::set<std::string>::iterator iter = keys.begin();
	     iter != keys.end(); ++iter)
	{
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

	bool filter = (this->stream_data_mode != s_preserve);
	if (this->stream_data_mode == s_compress)
	{
	    // Don't filter if the stream is already compressed with
	    // FlateDecode.  We don't want to make it worse by getting
	    // rid of a predictor or otherwise messing with it.  We
	    // should also avoid messing with anything that's
	    // compressed with a lossy compression scheme, but we
	    // don't support any of those right now.
	    QPDFObjectHandle filter_obj = stream_dict.getKey("/Filter");
	    if (filter_obj.isName() && (filter_obj.getName() == "/FlateDecode"))
	    {
		QTC::TC("qpdf", "QPDFWriter not recompressing /FlateDecode");
		filter = false;
	    }
	}
	bool normalize = false;
	bool compress = false;
	if (this->normalize_content && normalized_streams.count(old_id))
	{
	    normalize = true;
	    filter = true;
	}
	else if (filter && (this->stream_data_mode == s_compress))
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

	this->cur_stream_length = stream_data.getPointer()->getSize();
	unparseObject(stream_dict, 0, flags, this->cur_stream_length, compress);
	writeString("\nstream\n");
	pushEncryptionFilter();
	writeBuffer(stream_data);
	popPipelineStack();

	if (this->qdf_mode)
	{
	    if (this->pipeline->getLastChar() != '\n')
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
	    char* tmp = QUtil::copy_string(val);
	    unsigned int vlen = val.length();
	    RC4 rc4((unsigned char const*)this->cur_data_key.c_str(),
		    this->cur_data_key.length());
	    rc4.process((unsigned char*)tmp, vlen);
	    val = QPDF_String(std::string(tmp, vlen)).unparse();
	    delete [] tmp;
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
QPDFWriter::writeObjectStreamOffsets(std::vector<int>& offsets,
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

    std::vector<int> offsets;
    int first = 0;

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
	    for (std::vector<int>::iterator iter = offsets.begin();
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
	    if (! ((this->stream_data_mode == s_uncompress) || this->qdf_mode))
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
    writeString(" /Length " +
		QUtil::int_to_string(stream_buffer.getPointer()->getSize()));
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
	static char tmp[] = {0x31, 0x41, 0x59, 0x26,
			     0x53, 0x58, 0x97, 0x93,
			     0x23, 0x84, 0x62, 0x64,
			     0x33, 0x83, 0x27, 0x95,
			     0x00};
	result = tmp;
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
	seed += filename;
	seed += " ";
	if (trailer.hasKey("/Info"))
	{
	    std::set<std::string> keys = trailer.getKeys();
	    for (std::set<std::string>::iterator iter = keys.begin();
		 iter != keys.end(); ++iter)
	    {
		QPDFObjectHandle obj = trailer.getKey(*iter);
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
    unsigned int n_object_streams = (eligible.size() + 99) / 100;
    unsigned int n_per = eligible.size() / n_object_streams;
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
	    this->stream_data_mode = s_uncompress;
	}
    }

    if (this->encrypted)
    {
	// Encryption has been explicitly set
	this->preserve_encryption = false;
    }
    else if (this->normalize_content ||
	     (this->stream_data_mode == s_uncompress) ||
	     this->qdf_mode)
    {
	// Encryption makes looking at contents pretty useless.  If
	// the user explicitly encrypted though, we still obey that.
	this->preserve_encryption = false;
    }

    if (preserve_encryption)
    {
	copyEncryptionParameters();
    }

    if (this->qdf_mode || this->normalize_content ||
	(this->stream_data_mode == s_uncompress))
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
      case o_disable:
	// no action required
	break;

      case o_preserve:
	preserveObjectStreams();
	break;

      case o_generate:
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

    pdf.trimTrailerForWrite();
    pdf.flattenScalarReferences();

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
    setMinimumPDFVersion(pdf.getPDFVersion());
    std::string version = this->min_pdf_version;
    if (! this->forced_pdf_version.empty())
    {
	QTC::TC("qpdf", "QPDFWriter using forced PDF version");
	version = this->forced_pdf_version;
    }

    writeString("%PDF-");
    writeString(version);
    // This string of binary characters would not be valid UTF-8, so
    // it really should be treated as binary.
    writeString("\n%¿÷¢þ\n");
    writeStringQDF("%QDF-1.0\n\n");
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

    unsigned char* hs = hint_buffer.getPointer()->getBuffer();
    unsigned long hlen = hint_buffer.getPointer()->getSize();

    writeString("<< /Filter /FlateDecode /S ");
    writeString(QUtil::int_to_string(S));
    if (O)
    {
	writeString(" /O ");
	writeString(QUtil::int_to_string(O));
    }
    writeString(" /Length ");
    writeString(QUtil::int_to_string(hlen));
    writeString(" >>\nstream\n");

    if (this->encrypted)
    {
	QTC::TC("qpdf", "QPDFWriter encrypted hint stream");
    }
    pushEncryptionFilter();
    writeBuffer(hint_buffer);
    popPipelineStack();

    if (hs[hlen - 1] != '\n')
    {
	writeString("\n");
    }
    writeString("endstream");
    closeObject(hint_id);
}

int
QPDFWriter::writeXRefTable(trailer_e which, int first, int last, int size)
{
    return writeXRefTable(which, first, last, size, 0, false, 0, 0, 0);
}

int
QPDFWriter::writeXRefTable(trailer_e which, int first, int last, int size,
			   int prev, bool suppress_offsets,
			   int hint_id, int hint_offset, int hint_length)
{
    writeString("xref\n");
    writeString(QUtil::int_to_string(first));
    writeString(" ");
    writeString(QUtil::int_to_string(last - first + 1));
    int space_before_zero = this->pipeline->getCount();
    writeString("\n");
    for (int i = first; i <= last; ++i)
    {
	if (i == 0)
	{
	    writeString("0000000000 65535 f \n");
	}
	else
	{
	    int offset = 0;
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

int
QPDFWriter::writeXRefStream(int objid, int max_id, int max_offset,
			    trailer_e which, int first, int last, int size)
{
    return writeXRefStream(objid, max_id, max_offset,
			   which, first, last, size, 0, 0, 0, 0);
}

int
QPDFWriter::writeXRefStream(int xref_id, int max_id, int max_offset,
			    trailer_e which, int first, int last, int size,
			    int prev, int hint_id,
			    int hint_offset, int hint_length)
{
    int xref_offset = this->pipeline->getCount();
    int space_before_zero = xref_offset - 1;

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
    if (! ((this->stream_data_mode == s_uncompress) || this->qdf_mode))
    {
	compressed = true;
	p = pushPipeline(
	    new Pl_Flate("compress xref", p, Pl_Flate::a_deflate));
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
		int offset = e.getOffset();
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
    writeString(" /Length " +
		QUtil::int_to_string(xref_data.getPointer()->getSize()));
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
    int second_half_uncompressed = part7.size() + part8.size() + part9.size();
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
    int space_before_zero = 0;
    int file_size = 0;
    int part6_end_offset = 0;
    int first_half_max_obj_offset = 0;
    int second_xref_offset = 0;
    int first_xref_end = 0;
    int second_xref_end = 0;

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

    int hint_length = 0;
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
	// space to write real dictionary.  150 characters is enough
	// space if all numerical values in the parameter dictionary
	// are 10 digits long plus a few extra characters for safety.

	int pos = this->pipeline->getCount();
	openObject(lindict_id);
	writeString("<<");
	if (pass == 2)
	{
	    std::vector<QPDFObjectHandle> const& pages = pdf.getAllPages();
	    int first_page_object = obj_renumber[pages[0].getObjectID()];
	    int npages = pages.size();

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
	static int const pad = 150;
	int spaces = (pos + pad - this->pipeline->getCount());
	assert(spaces >= 0);
	for (int i = 0; i < spaces; ++i)
	{
	    writeString(" ");
	}
	writeString("\n");

	// Part 3: first page cross reference table and trailer.

	int first_xref_offset = this->pipeline->getCount();
	int hint_offset = 0;
	if (pass == 2)
	{
	    hint_offset = this->xref[hint_id].getOffset();
	}
	if (need_xref_stream)
	{
	    // Must pad here too.
	    if (pass == 1)
	    {
		// first_half_max_obj_offset is very likely to fall
		// within the first 64K of the document (thus
		// requiring two bytes for offsets) since it is the
		// offset of the last uncompressed object in page 1.
		// We allow for it to do otherwise though.
		first_half_max_obj_offset = 65535;
	    }
	    pos = this->pipeline->getCount();
	    writeXRefStream(first_half_xref, first_half_end,
			    first_half_max_obj_offset,
			    t_lin_first, first_half_start, first_half_end,
			    first_trailer_size,
			    hint_length + second_xref_offset,
			    hint_id, hint_offset, hint_length);
	    int endpos = this->pipeline->getCount();
	    if (pass == 1)
	    {
		// Pad so we have enough room for the real xref
		// stream.  In an extremely unlikely worst case,
		// first_half_max_obj_offset could be enough larger to
		// require two extra bytes beyond what we calculated
		// in pass 1.  This means we need to save two extra
		// bytes for each xref entry.  To that, we'll add 10
		// extra bytes for number length increases.
		int possible_extra =
		    10 + (2 * (first_half_end - first_half_start + 1));
		for (int i = 0; i < possible_extra; ++i)
		{
		    writeString(" ");
		}
		first_xref_end = this->pipeline->getCount();
	    }
	    else
	    {
		// Pad so that the next object starts at the same
		// place as in pass 1.
		for (int i = 0; i < first_xref_end - endpos; ++i)
		{
		    writeString(" ");
		}
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
	    space_before_zero =
		writeXRefStream(second_half_xref,
				second_half_end, second_xref_offset,
				t_lin_second, 0, second_half_end,
				second_trailer_size);
	    if (pass == 1)
	    {
		// Add some padding -- we need an accurate file_size
		// number, and this could change if the pass 2 xref
		// stream compresses differently.  There shouldn't be
		// much difference, so we'll just pad 100 characters.
		// This is unscientific though, and may not always
		// work.  The only way we could really get around this
		// would be to seek back to the beginning of the file
		// and update /L in the linearization dictionary, but
		// that would be the only thing in the design that
		// would require the output file to be seekable.
		for (int i = 0; i < 99; ++i)
		{
		    writeString(" ");
		}
		writeString("\n");
		second_xref_end = this->pipeline->getCount();
	    }
	    else
	    {
		// Make the file size the same.
		int pos = this->pipeline->getCount();
		while (pos < second_xref_end + hint_length - 1)
		{
		    ++pos;
		    writeString(" ");
		}
		writeString("\n");
		// If this assertion fails, maybe we didn't have
		// enough padding above.
		assert(this->pipeline->getCount() ==
		       second_xref_end + hint_length);
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
	    int hint_offset = this->xref[hint_id].getOffset();

	    // Write hint stream to a buffer
	    pushPipeline(new Pl_Buffer("hint buffer"));
	    activatePipelineStack();
	    writeHintStream(hint_id);
	    popPipelineStack(&hint_buffer);
	    hint_length = hint_buffer.getPointer()->getSize();

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
    off_t xref_offset = this->pipeline->getCount();
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
