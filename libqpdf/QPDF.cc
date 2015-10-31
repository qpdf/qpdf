#include <qpdf/qpdf-config.h>  // include first for large file support
#include <qpdf/QPDF.hh>

#include <vector>
#include <map>
#include <algorithm>
#include <string.h>
#include <memory.h>

#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/PCRE.hh>
#include <qpdf/Pipeline.hh>
#include <qpdf/Pl_Discard.hh>
#include <qpdf/FileInputSource.hh>
#include <qpdf/BufferInputSource.hh>
#include <qpdf/OffsetInputSource.hh>

#include <qpdf/QPDFExc.hh>
#include <qpdf/QPDF_Null.hh>
#include <qpdf/QPDF_Dictionary.hh>

std::string QPDF::qpdf_version = "5.2.0";

static char const* EMPTY_PDF =
    "%PDF-1.3\n"
    "1 0 obj\n"
    "<< /Type /Catalog /Pages 2 0 R >>\n"
    "endobj\n"
    "2 0 obj\n"
    "<< /Type /Pages /Kids [] /Count 0 >>\n"
    "endobj\n"
    "xref\n"
    "0 3\n"
    "0000000000 65535 f \n"
    "0000000009 00000 n \n"
    "0000000058 00000 n \n"
    "trailer << /Size 3 /Root 1 0 R >>\n"
    "startxref\n"
    "110\n"
    "%%EOF\n";

void
QPDF::CopiedStreamDataProvider::provideStreamData(
    int objid, int generation, Pipeline* pipeline)
{
    QPDFObjectHandle foreign_stream =
        this->foreign_streams[QPDFObjGen(objid, generation)];
    foreign_stream.pipeStreamData(pipeline, false, false, false);
}

void
QPDF::CopiedStreamDataProvider::registerForeignStream(
    QPDFObjGen const& local_og, QPDFObjectHandle foreign_stream)
{
    this->foreign_streams[local_og] = foreign_stream;
}

QPDF::StringDecrypter::StringDecrypter(QPDF* qpdf, int objid, int gen) :
    qpdf(qpdf),
    objid(objid),
    gen(gen)
{
}

void
QPDF::StringDecrypter::decryptString(std::string& val)
{
    qpdf->decryptString(val, objid, gen);
}

std::string const&
QPDF::QPDFVersion()
{
    return QPDF::qpdf_version;
}

QPDF::QPDF() :
    encrypted(false),
    encryption_initialized(false),
    ignore_xref_streams(false),
    suppress_warnings(false),
    out_stream(&std::cout),
    err_stream(&std::cerr),
    attempt_recovery(true),
    encryption_V(0),
    encryption_R(0),
    encrypt_metadata(true),
    cf_stream(e_none),
    cf_string(e_none),
    cf_file(e_none),
    cached_key_objid(0),
    cached_key_generation(0),
    pushed_inherited_attributes_to_pages(false),
    copied_stream_data_provider(0),
    first_xref_item_offset(0),
    uncompressed_after_compressed(false)
{
}

QPDF::~QPDF()
{
    // If two objects are mutually referential (through each object
    // having an array or dictionary that contains an indirect
    // reference to the other), the circular references in the
    // PointerHolder objects will prevent the objects from being
    // deleted.  Walk through all objects in the object cache, which
    // is those objects that we read from the file, and break all
    // resolved references.  At this point, obviously no one is still
    // using the QPDF object, but we'll explicitly clear the xref
    // table anyway just to prevent any possibility of resolve()
    // succeeding.  Note that we can't break references like this at
    // any time when the QPDF object is active.  If we do, the next
    // reference will reread the object from the file, which would
    // have the effect of undoing any modifications that may have been
    // made to any of the objects.
    this->xref_table.clear();
    for (std::map<QPDFObjGen, ObjCache>::iterator iter =
             this->obj_cache.begin();
	 iter != obj_cache.end(); ++iter)
    {
	QPDFObject::ObjAccessor::releaseResolved(
	    (*iter).second.object.getPointer());
    }
}

void
QPDF::processFile(char const* filename, char const* password)
{
    FileInputSource* fi = new FileInputSource();
    fi->setFilename(filename);
    processInputSource(fi, password);
}

void
QPDF::processFile(char const* description, FILE* filep,
                  bool close_file, char const* password)
{
    FileInputSource* fi = new FileInputSource();
    fi->setFile(description, filep, close_file);
    processInputSource(fi, password);
}

void
QPDF::processMemoryFile(char const* description,
			char const* buf, size_t length,
			char const* password)
{
    processInputSource(
	new BufferInputSource(
            description,
            new Buffer(QUtil::unsigned_char_pointer(buf), length),
            true),
        password);
}

void
QPDF::processInputSource(PointerHolder<InputSource> source,
                         char const* password)
{
    this->file = source;
    parse(password);
}

void
QPDF::emptyPDF()
{
    processMemoryFile("empty PDF", EMPTY_PDF, strlen(EMPTY_PDF));
}

void
QPDF::setIgnoreXRefStreams(bool val)
{
    this->ignore_xref_streams = val;
}

void
QPDF::setOutputStreams(std::ostream* out, std::ostream* err)
{
    this->out_stream = out ? out : &std::cout;
    this->err_stream = err ? err : &std::cerr;
}

void
QPDF::setSuppressWarnings(bool val)
{
    this->suppress_warnings = val;
}

void
QPDF::setAttemptRecovery(bool val)
{
    this->attempt_recovery = val;
}

std::vector<QPDFExc>
QPDF::getWarnings()
{
    std::vector<QPDFExc> result = this->warnings;
    this->warnings.clear();
    return result;
}

void
QPDF::parse(char const* password)
{
    PCRE header_re("\\A((?s).*?)%PDF-(1.\\d+)\\b");
    PCRE eof_re("(?s:startxref\\s+(\\d+)\\s+%%EOF\\b)");

    if (password)
    {
	this->provided_password = password;
    }

    // Find the header anywhere in the first 1024 bytes of the file,
    // plus add a little extra space for the header itself.
    char buffer[1045];
    memset(buffer, '\0', sizeof(buffer));
    this->file->read(buffer, sizeof(buffer) - 1);
    std::string line(buffer);
    PCRE::Match m1 = header_re.match(line.c_str());
    if (m1)
    {
        size_t global_offset = m1.getMatch(1).length();
        if (global_offset != 0)
        {
            // Empirical evidence strongly suggests that when there is
            // leading material prior to the PDF header, all explicit
            // offsets in the file are such that 0 points to the
            // beginning of the header.
            QTC::TC("qpdf", "QPDF global offset");
            this->file = new OffsetInputSource(this->file, global_offset);
        }
	this->pdf_version = m1.getMatch(2);
	if (atof(this->pdf_version.c_str()) < 1.2)
	{
	    this->tokenizer.allowPoundAnywhereInName();
	}
    }
    else
    {
	QTC::TC("qpdf", "QPDF not a pdf file");
	throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
		      "", 0, "not a PDF file");
    }

    // PDF spec says %%EOF must be found within the last 1024 bytes of
    // the file.  We add an extra 30 characters to leave room for the
    // startxref stuff.
    static int const tbuf_size = 1054;
    this->file->seek(0, SEEK_END);
    if (this->file->tell() > tbuf_size)
    {
	this->file->seek(-tbuf_size, SEEK_END);
    }
    else
    {
	this->file->rewind();
    }
    char* buf = new char[tbuf_size + 1];
    // Put buf in an array-style PointerHolder to guarantee deletion
    // of buf.
    PointerHolder<char> b(true, buf);
    memset(buf, '\0', tbuf_size + 1);
    this->file->read(buf, tbuf_size);

    // Since buf may contain null characters, we can't do a regexp
    // search on buf directly.  Find the last occurrence within buf
    // where the regexp matches.
    char* p = buf;
    char const* candidate = "";
    while ((p = static_cast<char*>(memchr(p, 's', tbuf_size - (p - buf)))) != 0)
    {
	if (eof_re.match(p))
	{
	    candidate = p;
	}
	++p;
    }

    try
    {
	PCRE::Match m2 = eof_re.match(candidate);
	if (! m2)
	{
	    QTC::TC("qpdf", "QPDF can't find startxref");
	    throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(), "", 0,
			  "can't find startxref");
	}
	qpdf_offset_t xref_offset = QUtil::string_to_ll(m2.getMatch(1).c_str());
	read_xref(xref_offset);
    }
    catch (QPDFExc& e)
    {
	if (this->attempt_recovery)
	{
	    reconstruct_xref(e);
	    QTC::TC("qpdf", "QPDF reconstructed xref table");
	}
	else
	{
	    throw e;
	}
    }

    initializeEncryption();
    findAttachmentStreams();
}

void
QPDF::warn(QPDFExc const& e)
{
    this->warnings.push_back(e);
    if (! this->suppress_warnings)
    {
	*err_stream << "WARNING: "
		    << this->warnings.back().what() << std::endl;
    }
}

void
QPDF::setTrailer(QPDFObjectHandle obj)
{
    if (this->trailer.isInitialized())
    {
	return;
    }
    this->trailer = obj;
}

void
QPDF::reconstruct_xref(QPDFExc& e)
{
    PCRE obj_re("^\\s*(\\d+)\\s+(\\d+)\\s+obj\\b");
    PCRE endobj_re("^\\s*endobj\\b");
    PCRE trailer_re("^\\s*trailer\\b");

    warn(QPDFExc(qpdf_e_damaged_pdf, this->file->getName(), "", 0,
		 "file is damaged"));
    warn(e);
    warn(QPDFExc(qpdf_e_damaged_pdf, this->file->getName(), "", 0,
		 "Attempting to reconstruct cross-reference table"));

    // Delete all references to type 1 (uncompressed) objects
    std::set<QPDFObjGen> to_delete;
    for (std::map<QPDFObjGen, QPDFXRefEntry>::iterator iter =
	     this->xref_table.begin();
	 iter != this->xref_table.end(); ++iter)
    {
	if (((*iter).second).getType() == 1)
	{
	    to_delete.insert((*iter).first);
	}
    }
    for (std::set<QPDFObjGen>::iterator iter = to_delete.begin();
	 iter != to_delete.end(); ++iter)
    {
	this->xref_table.erase(*iter);
    }

    this->file->seek(0, SEEK_END);
    qpdf_offset_t eof = this->file->tell();
    this->file->seek(0, SEEK_SET);
    bool in_obj = false;
    while (this->file->tell() < eof)
    {
	std::string line = this->file->readLine(50);
	if (in_obj)
	{
	    if (endobj_re.match(line.c_str()))
	    {
		in_obj = false;
	    }
	}
	else
	{
	    PCRE::Match m = obj_re.match(line.c_str());
	    if (m)
	    {
		in_obj = true;
		int obj = atoi(m.getMatch(1).c_str());
		int gen = atoi(m.getMatch(2).c_str());
		qpdf_offset_t offset = this->file->getLastOffset();
		insertXrefEntry(obj, 1, offset, gen, true);
	    }
	    else if ((! this->trailer.isInitialized()) &&
		     trailer_re.match(line.c_str()))
	    {
		// read "trailer"
		this->file->seek(this->file->getLastOffset(), SEEK_SET);
		readToken(this->file);
		QPDFObjectHandle t =
		    readObject(this->file, "trailer", 0, 0, false);
		if (! t.isDictionary())
		{
		    // Oh well.  It was worth a try.
		}
		else
		{
		    setTrailer(t);
		}
	    }
	}
    }

    if (! this->trailer.isInitialized())
    {
	// We could check the last encountered object to see if it was
	// an xref stream.  If so, we could try to get the trailer
	// from there.  This may make it possible to recover files
	// with bad startxref pointers even when they have object
	// streams.

	throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(), "", 0,
		      "unable to find trailer "
		      "dictionary while recovering damaged file");
    }

    // We could iterate through the objects looking for streams and
    // try to find objects inside of them, but it's probably not worth
    // the trouble.  Acrobat can't recover files with any errors in an
    // xref stream, and this would be a real long shot anyway.  If we
    // wanted to do anything that involved looking at stream contents,
    // we'd also have to call initializeEncryption() here.  It's safe
    // to call it more than once.
}

void
QPDF::read_xref(qpdf_offset_t xref_offset)
{
    std::map<int, int> free_table;
    while (xref_offset)
    {
        char buf[7];
        memset(buf, 0, sizeof(buf));
	this->file->seek(xref_offset, SEEK_SET);
	this->file->read(buf, sizeof(buf) - 1);
        // The PDF spec says xref must be followed by a line
        // terminator, but files exist in the wild where it is
        // terminated by arbitrary whitespace.
        PCRE xref_re("^xref\\s+");
	PCRE::Match m = xref_re.match(buf);
	if (m)
	{
            QTC::TC("qpdf", "QPDF xref space",
                    ((buf[4] == '\n') ? 0 :
                     (buf[4] == '\r') ? 1 :
                     (buf[4] == ' ') ? 2 : 9999));
	    xref_offset = read_xrefTable(xref_offset + m.getMatch(0).length());
	}
	else
	{
	    xref_offset = read_xrefStream(xref_offset);
	}
    }

    if (! this->trailer.isInitialized())
    {
        throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(), "", 0,
                      "unable to find trailer while reading xref");
    }
    int size = this->trailer.getKey("/Size").getIntValue();
    int max_obj = 0;
    if (! xref_table.empty())
    {
	max_obj = (*(xref_table.rbegin())).first.getObj();
    }
    if (! this->deleted_objects.empty())
    {
	max_obj = std::max(max_obj, *(this->deleted_objects.rbegin()));
    }
    if (size != max_obj + 1)
    {
	QTC::TC("qpdf", "QPDF xref size mismatch");
	warn(QPDFExc(qpdf_e_damaged_pdf, this->file->getName(), "", 0,
		     std::string("reported number of objects (") +
		     QUtil::int_to_string(size) +
		     ") inconsistent with actual number of objects (" +
		     QUtil::int_to_string(max_obj + 1) + ")"));
    }

    // We no longer need the deleted_objects table, so go ahead and
    // clear it out to make sure we never depend on its being set.
    this->deleted_objects.clear();
}

qpdf_offset_t
QPDF::read_xrefTable(qpdf_offset_t xref_offset)
{
    PCRE xref_first_re("^\\s*(\\d+)\\s+(\\d+)\\s*");
    PCRE xref_entry_re("(?s:(^\\d{10}) (\\d{5}) ([fn])\\s*$)");

    std::vector<QPDFObjGen> deleted_items;

    this->file->seek(xref_offset, SEEK_SET);
    bool done = false;
    while (! done)
    {
        char linebuf[51];
        memset(linebuf, 0, sizeof(linebuf));
        this->file->read(linebuf, sizeof(linebuf) - 1);
	std::string line = linebuf;
	PCRE::Match m1 = xref_first_re.match(line.c_str());
	if (! m1)
	{
	    QTC::TC("qpdf", "QPDF invalid xref");
	    throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
			  "xref table", this->file->getLastOffset(),
			  "xref syntax invalid");
	}
        file->seek(this->file->getLastOffset() + m1.getMatch(0).length(),
                   SEEK_SET);
	int obj = atoi(m1.getMatch(1).c_str());
	int num = atoi(m1.getMatch(2).c_str());
	for (int i = obj; i < obj + num; ++i)
	{
	    if (i == 0)
	    {
		// This is needed by checkLinearization()
		this->first_xref_item_offset = this->file->tell();
	    }
	    std::string xref_entry = this->file->readLine(30);
	    PCRE::Match m2 = xref_entry_re.match(xref_entry.c_str());
	    if (! m2)
	    {
		QTC::TC("qpdf", "QPDF invalid xref entry");
		throw QPDFExc(
		    qpdf_e_damaged_pdf, this->file->getName(),
		    "xref table", this->file->getLastOffset(),
		    "invalid xref entry (obj=" +
		    QUtil::int_to_string(i) + ")");
	    }

            // For xref_table, these will always be small enough to be ints
	    qpdf_offset_t f1 = QUtil::string_to_ll(m2.getMatch(1).c_str());
	    int f2 = atoi(m2.getMatch(2).c_str());
	    char type = m2.getMatch(3).at(0);
	    if (type == 'f')
	    {
		// Save deleted items until after we've checked the
		// XRefStm, if any.
		deleted_items.push_back(QPDFObjGen(i, f2));
	    }
	    else
	    {
		insertXrefEntry(i, 1, f1, f2);
	    }
	}
	qpdf_offset_t pos = this->file->tell();
	QPDFTokenizer::Token t = readToken(this->file);
	if (t == QPDFTokenizer::Token(QPDFTokenizer::tt_word, "trailer"))
	{
	    done = true;
	}
	else
	{
	    this->file->seek(pos, SEEK_SET);
	}
    }

    // Set offset to previous xref table if any
    QPDFObjectHandle cur_trailer =
	readObject(this->file, "trailer", 0, 0, false);
    if (! cur_trailer.isDictionary())
    {
	QTC::TC("qpdf", "QPDF missing trailer");
	throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
		      "", this->file->getLastOffset(),
		      "expected trailer dictionary");
    }

    if (! this->trailer.isInitialized())
    {
	setTrailer(cur_trailer);

	if (! this->trailer.hasKey("/Size"))
	{
	    QTC::TC("qpdf", "QPDF trailer lacks size");
	    throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
			  "trailer", this->file->getLastOffset(),
			  "trailer dictionary lacks /Size key");
	}
	if (! this->trailer.getKey("/Size").isInteger())
	{
	    QTC::TC("qpdf", "QPDF trailer size not integer");
	    throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
			  "trailer", this->file->getLastOffset(),
			  "/Size key in trailer dictionary is not "
			  "an integer");
	}
    }

    if (cur_trailer.hasKey("/XRefStm"))
    {
	if (this->ignore_xref_streams)
	{
	    QTC::TC("qpdf", "QPDF ignoring XRefStm in trailer");
	}
	else
	{
	    if (cur_trailer.getKey("/XRefStm").isInteger())
	    {
		// Read the xref stream but disregard any return value
		// -- we'll use our trailer's /Prev key instead of the
		// xref stream's.
		(void) read_xrefStream(
		    cur_trailer.getKey("/XRefStm").getIntValue());
	    }
	    else
	    {
		throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
			      "xref stream", xref_offset,
			      "invalid /XRefStm");
	    }
	}
    }

    // Handle any deleted items now that we've read the /XRefStm.
    for (std::vector<QPDFObjGen>::iterator iter = deleted_items.begin();
	 iter != deleted_items.end(); ++iter)
    {
	QPDFObjGen& og = *iter;
	insertXrefEntry(og.getObj(), 0, 0, og.getGen());
    }

    if (cur_trailer.hasKey("/Prev"))
    {
	if (! cur_trailer.getKey("/Prev").isInteger())
	{
	    QTC::TC("qpdf", "QPDF trailer prev not integer");
	    throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
			  "trailer", this->file->getLastOffset(),
			  "/Prev key in trailer dictionary is not "
			  "an integer");
	}
	QTC::TC("qpdf", "QPDF prev key in trailer dictionary");
	xref_offset = cur_trailer.getKey("/Prev").getIntValue();
    }
    else
    {
	xref_offset = 0;
    }

    return xref_offset;
}

qpdf_offset_t
QPDF::read_xrefStream(qpdf_offset_t xref_offset)
{
    bool found = false;
    if (! this->ignore_xref_streams)
    {
	int xobj;
	int xgen;
	QPDFObjectHandle xref_obj;
	try
	{
	    xref_obj = readObjectAtOffset(
		false, xref_offset, "xref stream", -1, 0, xobj, xgen);
	}
	catch (QPDFExc&)
	{
	    // ignore -- report error below
	}
	if (xref_obj.isInitialized() &&
	    xref_obj.isStream() &&
	    xref_obj.getDict().getKey("/Type").isName() &&
	    xref_obj.getDict().getKey("/Type").getName() == "/XRef")
	{
	    QTC::TC("qpdf", "QPDF found xref stream");
	    found = true;
	    xref_offset = processXRefStream(xref_offset, xref_obj);
	}
    }

    if (! found)
    {
	QTC::TC("qpdf", "QPDF can't find xref");
	throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
		      "", xref_offset, "xref not found");
    }

    return xref_offset;
}

qpdf_offset_t
QPDF::processXRefStream(qpdf_offset_t xref_offset, QPDFObjectHandle& xref_obj)
{
    QPDFObjectHandle dict = xref_obj.getDict();
    QPDFObjectHandle W_obj = dict.getKey("/W");
    QPDFObjectHandle Index_obj = dict.getKey("/Index");
    if (! (W_obj.isArray() &&
	   (W_obj.getArrayNItems() >= 3) &&
	   W_obj.getArrayItem(0).isInteger() &&
	   W_obj.getArrayItem(1).isInteger() &&
	   W_obj.getArrayItem(2).isInteger() &&
	   dict.getKey("/Size").isInteger() &&
	   (Index_obj.isArray() || Index_obj.isNull())))
    {
	throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
		      "xref stream", xref_offset,
		      "Cross-reference stream does not have"
		      " proper /W and /Index keys");
    }

    int W[3];
    size_t entry_size = 0;
    int max_bytes = sizeof(qpdf_offset_t);
    for (int i = 0; i < 3; ++i)
    {
	W[i] = W_obj.getArrayItem(i).getIntValue();
        if (W[i] > max_bytes)
        {
            throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
                          "xref stream", xref_offset,
                          "Cross-reference stream's /W contains"
                          " impossibly large values");
        }
	entry_size += W[i];
    }
    long long max_num_entries =
        static_cast<unsigned long long>(-1) / entry_size;

    std::vector<long long> indx;
    if (Index_obj.isArray())
    {
	int n_index = Index_obj.getArrayNItems();
	if ((n_index % 2) || (n_index < 2))
	{
	    throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
			  "xref stream", xref_offset,
			  "Cross-reference stream's /Index has an"
			  " invalid number of values");
	}
	for (int i = 0; i < n_index; ++i)
	{
	    if (Index_obj.getArrayItem(i).isInteger())
	    {
		indx.push_back(Index_obj.getArrayItem(i).getIntValue());
	    }
	    else
	    {
		throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
			      "xref stream", xref_offset,
			      "Cross-reference stream's /Index's item " +
			      QUtil::int_to_string(i) +
			      " is not an integer");
	    }
	}
	QTC::TC("qpdf", "QPDF xref /Index is array",
		n_index == 2 ? 0 : 1);
    }
    else
    {
	QTC::TC("qpdf", "QPDF xref /Index is null");
	long long size = dict.getKey("/Size").getIntValue();
	indx.push_back(0);
	indx.push_back(size);
    }

    long long num_entries = 0;
    for (unsigned int i = 1; i < indx.size(); i += 2)
    {
        if (indx.at(i) > max_num_entries - num_entries)
        {
            throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
                          "xref stream", xref_offset,
                          "Cross-reference stream claims to contain"
                          " too many entries: " +
                          QUtil::int_to_string(indx.at(i)) + " " +
                          QUtil::int_to_string(max_num_entries) + " " +
                          QUtil::int_to_string(num_entries));
        }
	num_entries += indx.at(i);
    }

    // entry_size and num_entries have both been validated to ensure
    // that this multiplication does not cause an overflow.
    size_t expected_size = entry_size * num_entries;

    PointerHolder<Buffer> bp = xref_obj.getStreamData();
    size_t actual_size = bp->getSize();

    if (expected_size != actual_size)
    {
	QPDFExc x(qpdf_e_damaged_pdf, this->file->getName(),
		  "xref stream", xref_offset,
		  "Cross-reference stream data has the wrong size;"
		  " expected = " + QUtil::int_to_string(expected_size) +
		  "; actual = " + QUtil::int_to_string(actual_size));
	if (expected_size > actual_size)
	{
	    throw x;
	}
	else
	{
	    warn(x);
	}
    }

    int cur_chunk = 0;
    int chunk_count = 0;

    bool saw_first_compressed_object = false;

    // Actual size vs. expected size check above ensures that we will
    // not overflow any buffers here.  We know that entry_size *
    // num_entries is equal to the size of the buffer.
    unsigned char const* data = bp->getBuffer();
    for (int i = 0; i < num_entries; ++i)
    {
	// Read this entry
	unsigned char const* entry = data + (entry_size * i);
	qpdf_offset_t fields[3];
	unsigned char const* p = entry;
	for (int j = 0; j < 3; ++j)
	{
	    fields[j] = 0;
	    if ((j == 0) && (W[0] == 0))
	    {
		QTC::TC("qpdf", "QPDF default for xref stream field 0");
		fields[0] = 1;
	    }
	    for (int k = 0; k < W[j]; ++k)
	    {
		fields[j] <<= 8;
		fields[j] += static_cast<int>(*p++);
	    }
	}

	// Get the object and generation number.  The object number is
	// based on /Index.  The generation number is 0 unless this is
	// an uncompressed object record, in which case the generation
	// number appears as the third field.
	int obj = indx.at(cur_chunk) + chunk_count;
	++chunk_count;
	if (chunk_count >= indx.at(cur_chunk + 1))
	{
	    cur_chunk += 2;
	    chunk_count = 0;
	}

	if (saw_first_compressed_object)
	{
	    if (fields[0] != 2)
	    {
		this->uncompressed_after_compressed = true;
	    }
	}
	else if (fields[0] == 2)
	{
	    saw_first_compressed_object = true;
	}
	if (obj == 0)
	{
	    // This is needed by checkLinearization()
	    this->first_xref_item_offset = xref_offset;
	}
	insertXrefEntry(obj, static_cast<int>(fields[0]),
                        fields[1], static_cast<int>(fields[2]));
    }

    if (! this->trailer.isInitialized())
    {
	setTrailer(dict);
    }

    if (dict.hasKey("/Prev"))
    {
	if (! dict.getKey("/Prev").isInteger())
	{
	    throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
			  "xref stream", this->file->getLastOffset(),
			  "/Prev key in xref stream dictionary is not "
			  "an integer");
	}
	QTC::TC("qpdf", "QPDF prev key in xref stream dictionary");
	xref_offset = dict.getKey("/Prev").getIntValue();
    }
    else
    {
	xref_offset = 0;
    }

    return xref_offset;
}

void
QPDF::insertXrefEntry(int obj, int f0, qpdf_offset_t f1, int f2, bool overwrite)
{
    // Populate the xref table in such a way that the first reference
    // to an object that we see, which is the one in the latest xref
    // table in which it appears, is the one that gets stored.  This
    // works because we are reading more recent appends before older
    // ones.  Exception: if overwrite is true, then replace any
    // existing object.  This is used in xref recovery mode, which
    // reads the file from beginning to end.

    // If there is already an entry for this object and generation in
    // the table, it means that a later xref table has registered this
    // object.  Disregard this one.
    { // private scope
	int gen = (f0 == 2 ? 0 : f2);
	QPDFObjGen og(obj, gen);
	if (this->xref_table.count(og))
	{
	    if (overwrite)
	    {
		QTC::TC("qpdf", "QPDF xref overwrite object");
		this->xref_table.erase(og);
	    }
	    else
	    {
		QTC::TC("qpdf", "QPDF xref reused object");
		return;
	    }
	}
	if (this->deleted_objects.count(obj))
	{
	    QTC::TC("qpdf", "QPDF xref deleted object");
	    return;
	}
    }

    switch (f0)
    {
      case 0:
	this->deleted_objects.insert(obj);
	break;

      case 1:
	// f2 is generation
	QTC::TC("qpdf", "QPDF xref gen > 0", ((f2 > 0) ? 1 : 0));
	this->xref_table[QPDFObjGen(obj, f2)] = QPDFXRefEntry(f0, f1, f2);
	break;

      case 2:
	this->xref_table[QPDFObjGen(obj, 0)] = QPDFXRefEntry(f0, f1, f2);
	break;

      default:
	throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
		      "xref stream", this->file->getLastOffset(),
		      "unknown xref stream entry type " +
		      QUtil::int_to_string(f0));
	break;
    }
}

void
QPDF::showXRefTable()
{
    for (std::map<QPDFObjGen, QPDFXRefEntry>::iterator iter =
	     this->xref_table.begin();
	 iter != this->xref_table.end(); ++iter)
    {
	QPDFObjGen const& og = (*iter).first;
	QPDFXRefEntry const& entry = (*iter).second;
	*out_stream << og.getObj() << "/" << og.getGen() << ": ";
	switch (entry.getType())
	{
	  case 1:
	    *out_stream << "uncompressed; offset = " << entry.getOffset();
	    break;

	  case 2:
	    *out_stream << "compressed; stream = " << entry.getObjStreamNumber()
			<< ", index = " << entry.getObjStreamIndex();
	    break;

	  default:
	    throw std::logic_error("unknown cross-reference table type while"
				   " showing xref_table");
	    break;
	}
	*out_stream << std::endl;
    }
}

void
QPDF::setLastObjectDescription(std::string const& description,
			       int objid, int generation)
{
    this->last_object_description.clear();
    if (! description.empty())
    {
	this->last_object_description += description;
	if (objid > 0)
	{
	    this->last_object_description += ": ";
	}
    }
    if (objid > 0)
    {
	this->last_object_description += "object " +
	    QUtil::int_to_string(objid) + " " +
	    QUtil::int_to_string(generation);
    }
}

QPDFObjectHandle
QPDF::readObject(PointerHolder<InputSource> input,
		 std::string const& description,
		 int objid, int generation, bool in_object_stream)
{
    setLastObjectDescription(description, objid, generation);
    qpdf_offset_t offset = input->tell();

    bool empty = false;
    PointerHolder<StringDecrypter> decrypter_ph;
    StringDecrypter* decrypter = 0;
    if (this->encrypted && (! in_object_stream))
    {
        decrypter_ph = new StringDecrypter(this, objid, generation);
        decrypter = decrypter_ph.getPointer();
    }
    QPDFObjectHandle object = QPDFObjectHandle::parse(
        input, description, this->tokenizer, empty, decrypter, this);
    if (empty)
    {
        // Nothing in the PDF spec appears to allow empty objects, but
        // they have been encountered in actual PDF files and Adobe
        // Reader appears to ignore them.
        warn(QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                     this->last_object_description,
                     input->getLastOffset(),
                     "empty object treated as null"));
    }
    else if (object.isDictionary() && (! in_object_stream))
    {
        // check for stream
        qpdf_offset_t cur_offset = input->tell();
        if (readToken(input) ==
            QPDFTokenizer::Token(QPDFTokenizer::tt_word, "stream"))
        {
            // The PDF specification states that the word "stream"
            // should be followed by either a carriage return and
            // a newline or by a newline alone.  It specifically
            // disallowed following it by a carriage return alone
            // since, in that case, there would be no way to tell
            // whether the NL in a CR NL sequence was part of the
            // stream data.  However, some readers, including
            // Adobe reader, accept a carriage return by itself
            // when followed by a non-newline character, so that's
            // what we do here.
            {
                char ch;
                if (input->read(&ch, 1) == 0)
                {
                    // A premature EOF here will result in some
                    // other problem that will get reported at
                    // another time.
                }
                else if (ch == '\n')
                {
                    // ready to read stream data
                    QTC::TC("qpdf", "QPDF stream with NL only");
                }
                else if (ch == '\r')
                {
                    // Read another character
                    if (input->read(&ch, 1) != 0)
                    {
                        if (ch == '\n')
                        {
                            // Ready to read stream data
                            QTC::TC("qpdf", "QPDF stream with CRNL");
                        }
                        else
                        {
                            // Treat the \r by itself as the
                            // whitespace after endstream and
                            // start reading stream data in spite
                            // of not having seen a newline.
                            QTC::TC("qpdf", "QPDF stream with CR only");
                            input->unreadCh(ch);
                            warn(QPDFExc(
                                     qpdf_e_damaged_pdf,
                                     input->getName(),
                                     this->last_object_description,
                                     input->tell(),
                                     "stream keyword followed"
                                     " by carriage return only"));
                        }
                    }
                }
                else
                {
                    QTC::TC("qpdf", "QPDF stream without newline");
                    warn(QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                                 this->last_object_description,
                                 input->tell(),
                                 "stream keyword not followed"
                                 " by proper line terminator"));
                }
            }

            // Must get offset before accessing any additional
            // objects since resolving a previously unresolved
            // indirect object will change file position.
            qpdf_offset_t stream_offset = input->tell();
            size_t length = 0;

            try
            {
                std::map<std::string, QPDFObjectHandle> dict =
                    object.getDictAsMap();

                if (dict.count("/Length") == 0)
                {
                    QTC::TC("qpdf", "QPDF stream without length");
                    throw QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                                  this->last_object_description, offset,
                                  "stream dictionary lacks /Length key");
                }

                QPDFObjectHandle length_obj = dict["/Length"];
                if (! length_obj.isInteger())
                {
                    QTC::TC("qpdf", "QPDF stream length not integer");
                    throw QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                                  this->last_object_description, offset,
                                  "/Length key in stream dictionary is not "
                                  "an integer");
                }

                length = length_obj.getIntValue();
                input->seek(stream_offset + length, SEEK_SET);
                if (! (readToken(input) ==
                       QPDFTokenizer::Token(
                           QPDFTokenizer::tt_word, "endstream")))
                {
                    QTC::TC("qpdf", "QPDF missing endstream");
                    throw QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                                  this->last_object_description,
                                  input->getLastOffset(),
                                  "expected endstream");
                }
            }
            catch (QPDFExc& e)
            {
                if (this->attempt_recovery)
                {
                    // may throw an exception
                    length = recoverStreamLength(
                        input, objid, generation, stream_offset);
                }
                else
                {
                    throw e;
                }
            }
            object = QPDFObjectHandle::Factory::newStream(
                this, objid, generation, object, stream_offset, length);
        }
        else
        {
            input->seek(cur_offset, SEEK_SET);
        }
    }

    // Override last_offset so that it points to the beginning of the
    // object we just read
    input->setLastOffset(offset);
    return object;
}

size_t
QPDF::recoverStreamLength(PointerHolder<InputSource> input,
			  int objid, int generation,
                          qpdf_offset_t stream_offset)
{
    PCRE endobj_re("^\\s*endobj\\b");

    // Try to reconstruct stream length by looking for
    // endstream(\r\n?|\n)endobj
    warn(QPDFExc(qpdf_e_damaged_pdf, input->getName(),
		 this->last_object_description, stream_offset,
		 "attempting to recover stream length"));

    input->seek(0, SEEK_END);
    qpdf_offset_t eof = input->tell();
    input->seek(stream_offset, SEEK_SET);
    qpdf_offset_t last_line_offset = 0;
    size_t length = 0;
    static int const line_end_length = 12; // room for endstream\r\n\0
    char last_line_end[line_end_length];
    while (input->tell() < eof)
    {
	std::string line = input->readLine(50);
        qpdf_offset_t line_offset = input->getLastOffset();
	if (endobj_re.match(line.c_str()))
        {
            qpdf_offset_t endstream_offset = 0;
            if (last_line_offset >= line_end_length)
            {
                qpdf_offset_t cur_offset = input->tell();
                // Read from the end of the last line, guaranteeing
                // null termination
                qpdf_offset_t search_offset =
                    line_offset - (line_end_length - 1);
                input->seek(search_offset, SEEK_SET);
                memset(last_line_end, '\0', line_end_length);
                input->read(last_line_end, line_end_length - 1);
                input->seek(cur_offset, SEEK_SET);
                // if endstream[\r\n] will fit in last_line_end, the
                // 'e' has to be in one of the first three spots.
                // Check explicitly rather than using strstr directly
                // in case there are nulls right before endstream.
                char* p = ((last_line_end[0] == 'e') ? last_line_end :
                           (last_line_end[1] == 'e') ? last_line_end + 1 :
                           (last_line_end[2] == 'e') ? last_line_end + 2 :
                           0);
                char* endstream_p = 0;
                if (p)
                {
                    char* p1 = strstr(p, "endstream\n");
                    char* p2 = strstr(p, "endstream\r");
                    endstream_p = (p1 ? p1 : p2);
                }
                if (endstream_p)
                {
                    endstream_offset =
                        search_offset + (endstream_p - last_line_end);
                }
            }
            if (endstream_offset > 0)
            {
                // Stream probably ends right before "endstream"
                length = endstream_offset - stream_offset;
                // Go back to where we would have been if we had just
                // read the endstream.
                input->seek(line_offset, SEEK_SET);
                break;
            }
	}
	last_line_offset = line_offset;
    }

    if (length)
    {
	int this_obj_offset = 0;
	QPDFObjGen this_obj(0, 0);

	// Make sure this is inside this object
	for (std::map<QPDFObjGen, QPDFXRefEntry>::iterator iter =
		 this->xref_table.begin();
	     iter != this->xref_table.end(); ++iter)
	{
	    QPDFObjGen const& og = (*iter).first;
	    QPDFXRefEntry const& entry = (*iter).second;
	    if (entry.getType() == 1)
	    {
		qpdf_offset_t obj_offset = entry.getOffset();
		if ((obj_offset > stream_offset) &&
		    ((this_obj_offset == 0) ||
		     (this_obj_offset > obj_offset)))
		{
		    this_obj_offset = obj_offset;
		    this_obj = og;
		}
	    }
	}
	if (this_obj_offset &&
	    (this_obj.getObj() == objid) &&
	    (this_obj.getGen() == generation))
	{
	    // Well, we found endstream\nendobj within the space
	    // allowed for this object, so we're probably in good
	    // shape.
	}
	else
	{
	    QTC::TC("qpdf", "QPDF found wrong endstream in recovery");
	}
    }

    if (length == 0)
    {
	throw QPDFExc(qpdf_e_damaged_pdf, input->getName(),
		      this->last_object_description, stream_offset,
		      "unable to recover stream data");
    }

    QTC::TC("qpdf", "QPDF recovered stream length");
    return length;
}

QPDFTokenizer::Token
QPDF::readToken(PointerHolder<InputSource> input)
{
    return this->tokenizer.readToken(input, this->last_object_description);
}

QPDFObjectHandle
QPDF::readObjectAtOffset(bool try_recovery,
			 qpdf_offset_t offset, std::string const& description,
			 int exp_objid, int exp_generation,
			 int& objid, int& generation)
{
    setLastObjectDescription(description, exp_objid, exp_generation);

    // Special case: if offset is 0, just return null.  Some PDF
    // writers, in particular "Mac OS X 10.7.5 Quartz PDFContext", may
    // store deleted objects in the xref table as "0000000000 00000
    // n", which is not correct, but it won't hurt anything for to
    // ignore these.
    if (offset == 0)
    {
        QTC::TC("qpdf", "QPDF bogus 0 offset", 0);
	warn(QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
		     this->last_object_description, 0,
		     "object has offset 0"));
        return QPDFObjectHandle::newNull();
    }

    this->file->seek(offset, SEEK_SET);

    QPDFTokenizer::Token tobjid = readToken(this->file);
    QPDFTokenizer::Token tgen = readToken(this->file);
    QPDFTokenizer::Token tobj = readToken(this->file);

    bool objidok = (tobjid.getType() == QPDFTokenizer::tt_integer);
    int genok = (tgen.getType() == QPDFTokenizer::tt_integer);
    int objok = (tobj == QPDFTokenizer::Token(QPDFTokenizer::tt_word, "obj"));

    QTC::TC("qpdf", "QPDF check objid", objidok ? 1 : 0);
    QTC::TC("qpdf", "QPDF check generation", genok ? 1 : 0);
    QTC::TC("qpdf", "QPDF check obj", objok ? 1 : 0);

    try
    {
	if (! (objidok && genok && objok))
	{
	    QTC::TC("qpdf", "QPDF expected n n obj");
	    throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
			  this->last_object_description, offset,
			  "expected n n obj");
	}
	objid = atoi(tobjid.getValue().c_str());
	generation = atoi(tgen.getValue().c_str());

	if ((exp_objid >= 0) &&
	    (! ((objid == exp_objid) && (generation == exp_generation))))
	{
	    QTC::TC("qpdf", "QPDF err wrong objid/generation");
	    throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
			  this->last_object_description, offset,
			  std::string("expected ") +
			  QUtil::int_to_string(exp_objid) + " " +
			  QUtil::int_to_string(exp_generation) + " obj");
	}
    }
    catch (QPDFExc& e)
    {
	if ((exp_objid >= 0) && try_recovery && this->attempt_recovery)
	{
	    // Try again after reconstructing xref table
	    reconstruct_xref(e);
	    QPDFObjGen og(exp_objid, exp_generation);
	    if (this->xref_table.count(og) &&
		(this->xref_table[og].getType() == 1))
	    {
		qpdf_offset_t new_offset = this->xref_table[og].getOffset();
		QPDFObjectHandle result = readObjectAtOffset(
		    false, new_offset, description,
		    exp_objid, exp_generation, objid, generation);
		QTC::TC("qpdf", "QPDF recovered in readObjectAtOffset");
		return result;
	    }
	    else
	    {
		QTC::TC("qpdf", "QPDF object gone after xref reconstruction");
		warn(QPDFExc(
			 qpdf_e_damaged_pdf, this->file->getName(),
			 "", 0,
			 std::string(
			     "object " +
			     QUtil::int_to_string(exp_objid) +
			     " " +
			     QUtil::int_to_string(exp_generation) +
			     " not found in file after regenerating"
			     " cross reference table")));
		return QPDFObjectHandle::newNull();
	    }
	}
	else
	{
	    throw e;
	}
    }

    QPDFObjectHandle oh = readObject(
	this->file, description, objid, generation, false);

    if (! (readToken(this->file) ==
	   QPDFTokenizer::Token(QPDFTokenizer::tt_word, "endobj")))
    {
	QTC::TC("qpdf", "QPDF err expected endobj");
	warn(QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
		     this->last_object_description, this->file->getLastOffset(),
		     "expected endobj"));
    }

    QPDFObjGen og(objid, generation);
    if (! this->obj_cache.count(og))
    {
	// Store the object in the cache here so it gets cached
	// whether we first know the offset or whether we first know
	// the object ID and generation (in which we case we would get
	// here through resolve).

	// Determine the end offset of this object before and after
	// white space.  We use these numbers to validate
	// linearization hint tables.  Offsets and lengths of objects
	// may imply the end of an object to be anywhere between these
	// values.
	qpdf_offset_t end_before_space = this->file->tell();

	// skip over spaces
	while (true)
	{
	    char ch;
	    if (this->file->read(&ch, 1))
	    {
		if (! isspace(static_cast<unsigned char>(ch)))
		{
		    this->file->seek(-1, SEEK_CUR);
		    break;
		}
	    }
	    else
	    {
		throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
			      this->last_object_description, offset,
			      "EOF after endobj");
	    }
	}
	qpdf_offset_t end_after_space = this->file->tell();

	this->obj_cache[og] =
	    ObjCache(QPDFObjectHandle::ObjAccessor::getObject(oh),
		     end_before_space, end_after_space);
    }

    return oh;
}

PointerHolder<QPDFObject>
QPDF::resolve(int objid, int generation)
{
    // Check object cache before checking xref table.  This allows us
    // to insert things into the object cache that don't actually
    // exist in the file.
    QPDFObjGen og(objid, generation);
    if (! this->obj_cache.count(og))
    {
	if (! this->xref_table.count(og))
	{
	    // PDF spec says unknown objects resolve to the null object.
	    return new QPDF_Null;
	}

	QPDFXRefEntry const& entry = this->xref_table[og];
	switch (entry.getType())
	{
	  case 1:
	    {
		qpdf_offset_t offset = entry.getOffset();
		// Object stored in cache by readObjectAtOffset
		int aobjid;
		int ageneration;
		QPDFObjectHandle oh =
		    readObjectAtOffset(true, offset, "", objid, generation,
				       aobjid, ageneration);
	    }
	    break;

	  case 2:
	    resolveObjectsInStream(entry.getObjStreamNumber());
	    break;

	  default:
	    throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(), "", 0,
			  "object " +
			  QUtil::int_to_string(objid) + "/" +
			  QUtil::int_to_string(generation) +
			  " has unexpected xref entry type");
	}
    }

    return this->obj_cache[og].object;
}

void
QPDF::resolveObjectsInStream(int obj_stream_number)
{
    // Force resolution of object stream
    QPDFObjectHandle obj_stream = getObjectByID(obj_stream_number, 0);
    if (! obj_stream.isStream())
    {
	throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
		      this->last_object_description,
		      this->file->getLastOffset(),
		      "supposed object stream " +
		      QUtil::int_to_string(obj_stream_number) +
		      " is not a stream");
    }

    // For linearization data in the object, use the data from the
    // object stream for the objects in the stream.
    QPDFObjGen stream_og(obj_stream_number, 0);
    qpdf_offset_t end_before_space =
        this->obj_cache[stream_og].end_before_space;
    qpdf_offset_t end_after_space =
        this->obj_cache[stream_og].end_after_space;

    QPDFObjectHandle dict = obj_stream.getDict();
    if (! (dict.getKey("/Type").isName() &&
	   dict.getKey("/Type").getName() == "/ObjStm"))
    {
	QTC::TC("qpdf", "QPDF ERR object stream with wrong type");
	throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
		      this->last_object_description,
		      this->file->getLastOffset(),
		      "supposed object stream " +
		      QUtil::int_to_string(obj_stream_number) +
		      " has wrong type");
    }

    if (! (dict.getKey("/N").isInteger() &&
	   dict.getKey("/First").isInteger()))
    {
	throw QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
		      this->last_object_description,
		      this->file->getLastOffset(),
		      "object stream " +
		      QUtil::int_to_string(obj_stream_number) +
		      " has incorrect keys");
    }

    int n = dict.getKey("/N").getIntValue();
    int first = dict.getKey("/First").getIntValue();

    std::map<int, int> offsets;

    PointerHolder<Buffer> bp = obj_stream.getStreamData();
    PointerHolder<InputSource> input = new BufferInputSource(
	"object stream " + QUtil::int_to_string(obj_stream_number),
	bp.getPointer());

    for (int i = 0; i < n; ++i)
    {
	QPDFTokenizer::Token tnum = readToken(input);
	QPDFTokenizer::Token toffset = readToken(input);
	if (! ((tnum.getType() == QPDFTokenizer::tt_integer) &&
	       (toffset.getType() == QPDFTokenizer::tt_integer)))
	{
	    throw QPDFExc(qpdf_e_damaged_pdf, input->getName(),
			  this->last_object_description, input->getLastOffset(),
			  "expected integer in object stream header");
	}

	int num = atoi(tnum.getValue().c_str());
	int offset = QUtil::string_to_ll(toffset.getValue().c_str());
	offsets[num] = offset + first;
    }

    // To avoid having to read the object stream multiple times, store
    // all objects that would be found here in the cache.  Remember
    // that some objects stored here might have been overridden by new
    // objects appended to the file, so it is necessary to recheck the
    // xref table and only cache what would actually be resolved here.
    for (std::map<int, int>::iterator iter = offsets.begin();
	 iter != offsets.end(); ++iter)
    {
	int obj = (*iter).first;
	QPDFObjGen og(obj, 0);
        QPDFXRefEntry const& entry = this->xref_table[og];
        if ((entry.getType() == 2) &&
            (entry.getObjStreamNumber() == obj_stream_number))
        {
            int offset = (*iter).second;
            input->seek(offset, SEEK_SET);
            QPDFObjectHandle oh = readObject(input, "", obj, 0, true);
            this->obj_cache[og] =
                ObjCache(QPDFObjectHandle::ObjAccessor::getObject(oh),
                         end_before_space, end_after_space);
        }
        else
        {
            QTC::TC("qpdf", "QPDF not caching overridden objstm object");
        }
    }
}

QPDFObjectHandle
QPDF::makeIndirectObject(QPDFObjectHandle oh)
{
    QPDFObjGen o1(0, 0);
    if (! this->obj_cache.empty())
    {
	o1 = (*(this->obj_cache.rbegin())).first;
    }
    QPDFObjGen o2 = (*(this->xref_table.rbegin())).first;
    QTC::TC("qpdf", "QPDF indirect last obj from xref",
	    (o2.getObj() > o1.getObj()) ? 1 : 0);
    int max_objid = std::max(o1.getObj(), o2.getObj());
    QPDFObjGen next(max_objid + 1, 0);
    this->obj_cache[next] =
	ObjCache(QPDFObjectHandle::ObjAccessor::getObject(oh), -1, -1);
    return QPDFObjectHandle::Factory::newIndirect(
        this, next.getObj(), next.getGen());
}

QPDFObjectHandle
QPDF::getObjectByObjGen(QPDFObjGen const& og)
{
    return getObjectByID(og.getObj(), og.getGen());
}

QPDFObjectHandle
QPDF::getObjectByID(int objid, int generation)
{
    return QPDFObjectHandle::Factory::newIndirect(this, objid, generation);
}

void
QPDF::replaceObject(QPDFObjGen const& og, QPDFObjectHandle oh)
{
    replaceObject(og.getObj(), og.getGen(), oh);
}

void
QPDF::replaceObject(int objid, int generation, QPDFObjectHandle oh)
{
    if (oh.isIndirect())
    {
	QTC::TC("qpdf", "QPDF replaceObject called with indirect object");
	throw std::logic_error(
	    "QPDF::replaceObject called with indirect object handle");
    }

    // Force new object to appear in the cache
    resolve(objid, generation);

    // Replace the object in the object cache
    QPDFObjGen og(objid, generation);
    this->obj_cache[og] =
	ObjCache(QPDFObjectHandle::ObjAccessor::getObject(oh), -1, -1);
}

void
QPDF::replaceReserved(QPDFObjectHandle reserved,
                      QPDFObjectHandle replacement)
{
    QTC::TC("qpdf", "QPDF replaceReserved");
    reserved.assertReserved();
    replaceObject(reserved.getObjGen(), replacement);
}

QPDFObjectHandle
QPDF::copyForeignObject(QPDFObjectHandle foreign)
{
    return copyForeignObject(foreign, false);
}

QPDFObjectHandle
QPDF::copyForeignObject(QPDFObjectHandle foreign, bool allow_page)
{
    if (! foreign.isIndirect())
    {
        QTC::TC("qpdf", "QPDF copyForeign direct");
	throw std::logic_error(
	    "QPDF::copyForeign called with direct object handle");
    }
    QPDF* other = foreign.getOwningQPDF();
    if (other == this)
    {
        QTC::TC("qpdf", "QPDF copyForeign not foreign");
        throw std::logic_error(
            "QPDF::copyForeign called with object from this QPDF");
    }

    ObjCopier& obj_copier = this->object_copiers[other];
    if (! obj_copier.visiting.empty())
    {
        throw std::logic_error("obj_copier.visiting is not empty"
                               " at the beginning of copyForeignObject");
    }

    // Make sure we have an object in this file for every referenced
    // object in the old file.  obj_copier.object_map maps foreign
    // QPDFObjGen to local objects.  For everything new that we have
    // to copy, the local object will be a reservation, unless it is a
    // stream, in which case the local object will already be a
    // stream.
    reserveObjects(foreign, obj_copier, true);

    if (! obj_copier.visiting.empty())
    {
        throw std::logic_error("obj_copier.visiting is not empty"
                               " after reserving objects");
    }

    // Copy any new objects and replace the reservations.
    for (std::vector<QPDFObjectHandle>::iterator iter =
             obj_copier.to_copy.begin();
         iter != obj_copier.to_copy.end(); ++iter)
    {
        QPDFObjectHandle& to_copy = *iter;
        QPDFObjectHandle copy =
            replaceForeignIndirectObjects(to_copy, obj_copier, true);
        if (! to_copy.isStream())
        {
            QPDFObjGen og(to_copy.getObjGen());
            replaceReserved(obj_copier.object_map[og], copy);
        }
    }
    obj_copier.to_copy.clear();

    return obj_copier.object_map[foreign.getObjGen()];
}

void
QPDF::reserveObjects(QPDFObjectHandle foreign, ObjCopier& obj_copier,
                     bool top)
{
    if (foreign.isReserved())
    {
        throw std::logic_error(
            "QPDF: attempting to copy a foreign reserved object");
    }

    if (foreign.isPagesObject())
    {
        QTC::TC("qpdf", "QPDF not copying pages object");
        return;
    }

    if ((! top) && foreign.isPageObject())
    {
        QTC::TC("qpdf", "QPDF not crossing page boundary");
        return;
    }

    if (foreign.isIndirect())
    {
        QPDFObjGen foreign_og(foreign.getObjGen());
        if (obj_copier.visiting.find(foreign_og) != obj_copier.visiting.end())
        {
            QTC::TC("qpdf", "QPDF loop reserving objects");
            return;
        }
        if (obj_copier.object_map.find(foreign_og) != obj_copier.object_map.end())
        {
            QTC::TC("qpdf", "QPDF already reserved object");
            return;
        }
        QTC::TC("qpdf", "QPDF copy indirect");
        obj_copier.visiting.insert(foreign_og);
        std::map<QPDFObjGen, QPDFObjectHandle>::iterator mapping =
            obj_copier.object_map.find(foreign_og);
        if (mapping == obj_copier.object_map.end())
        {
            obj_copier.to_copy.push_back(foreign);
            QPDFObjectHandle reservation;
            if (foreign.isStream())
            {
                reservation = QPDFObjectHandle::newStream(this);
            }
            else
            {
                reservation = QPDFObjectHandle::newReserved(this);
            }
            obj_copier.object_map[foreign_og] = reservation;
        }
    }

    if (foreign.isArray())
    {
        QTC::TC("qpdf", "QPDF reserve array");
	int n = foreign.getArrayNItems();
	for (int i = 0; i < n; ++i)
	{
            reserveObjects(foreign.getArrayItem(i), obj_copier, false);
	}
    }
    else if (foreign.isDictionary())
    {
        QTC::TC("qpdf", "QPDF reserve dictionary");
	std::set<std::string> keys = foreign.getKeys();
	for (std::set<std::string>::iterator iter = keys.begin();
	     iter != keys.end(); ++iter)
	{
            reserveObjects(foreign.getKey(*iter), obj_copier, false);
	}
    }
    else if (foreign.isStream())
    {
        QTC::TC("qpdf", "QPDF reserve stream");
        reserveObjects(foreign.getDict(), obj_copier, false);
    }

    if (foreign.isIndirect())
    {
        QPDFObjGen foreign_og(foreign.getObjGen());
        obj_copier.visiting.erase(foreign_og);
    }
}

QPDFObjectHandle
QPDF::replaceForeignIndirectObjects(
    QPDFObjectHandle foreign, ObjCopier& obj_copier, bool top)
{
    QPDFObjectHandle result;
    if ((! top) && foreign.isIndirect())
    {
        QTC::TC("qpdf", "QPDF replace indirect");
        QPDFObjGen foreign_og(foreign.getObjGen());
        std::map<QPDFObjGen, QPDFObjectHandle>::iterator mapping =
            obj_copier.object_map.find(foreign_og);
        if (mapping == obj_copier.object_map.end())
        {
            // This case would occur if this is a reference to a Page
            // or Pages object that we didn't traverse into.
            QTC::TC("qpdf", "QPDF replace foreign indirect with null");
            result = QPDFObjectHandle::newNull();
        }
        else
        {
            result = obj_copier.object_map[foreign_og];
        }
    }
    else if (foreign.isArray())
    {
        QTC::TC("qpdf", "QPDF replace array");
        result = QPDFObjectHandle::newArray();
	int n = foreign.getArrayNItems();
	for (int i = 0; i < n; ++i)
	{
            result.appendItem(
                replaceForeignIndirectObjects(
                    foreign.getArrayItem(i), obj_copier, false));
	}
    }
    else if (foreign.isDictionary())
    {
        QTC::TC("qpdf", "QPDF replace dictionary");
        result = QPDFObjectHandle::newDictionary();
	std::set<std::string> keys = foreign.getKeys();
	for (std::set<std::string>::iterator iter = keys.begin();
	     iter != keys.end(); ++iter)
	{
            result.replaceKey(
                *iter,
                replaceForeignIndirectObjects(
                    foreign.getKey(*iter), obj_copier, false));
	}
    }
    else if (foreign.isStream())
    {
        QTC::TC("qpdf", "QPDF replace stream");
        QPDFObjGen foreign_og(foreign.getObjGen());
        result = obj_copier.object_map[foreign_og];
        result.assertStream();
        QPDFObjectHandle dict = result.getDict();
        QPDFObjectHandle old_dict = foreign.getDict();
        std::set<std::string> keys = old_dict.getKeys();
        for (std::set<std::string>::iterator iter = keys.begin();
	     iter != keys.end(); ++iter)
	{
            dict.replaceKey(
                *iter,
                replaceForeignIndirectObjects(
                    old_dict.getKey(*iter), obj_copier, false));
	}
        if (this->copied_stream_data_provider == 0)
        {
            this->copied_stream_data_provider = new CopiedStreamDataProvider();
            this->copied_streams = this->copied_stream_data_provider;
        }
        QPDFObjGen local_og(result.getObjGen());
        this->copied_stream_data_provider->registerForeignStream(
            local_og, foreign);
        result.replaceStreamData(this->copied_streams,
                                 dict.getKey("/Filter"),
                                 dict.getKey("/DecodeParms"));
    }
    else
    {
        foreign.assertScalar();
        result = foreign;
        result.makeDirect();
    }

    if (top && (! result.isStream()) && result.isIndirect())
    {
        throw std::logic_error("replacement for foreign object is indirect");
    }

    return result;
}

void
QPDF::swapObjects(QPDFObjGen const& og1, QPDFObjGen const& og2)
{
    swapObjects(og1.getObj(), og1.getGen(), og2.getObj(), og2.getGen());
}

void
QPDF::swapObjects(int objid1, int generation1, int objid2, int generation2)
{
    // Force objects to be loaded into cache; then swap them in the
    // cache.
    resolve(objid1, generation1);
    resolve(objid2, generation2);
    QPDFObjGen og1(objid1, generation1);
    QPDFObjGen og2(objid2, generation2);
    ObjCache t = this->obj_cache[og1];
    this->obj_cache[og1] = this->obj_cache[og2];
    this->obj_cache[og2] = t;
}

std::string
QPDF::getFilename() const
{
    return this->file->getName();
}

std::string
QPDF::getPDFVersion() const
{
    return this->pdf_version;
}

int
QPDF::getExtensionLevel()
{
    int result = 0;
    QPDFObjectHandle obj = getRoot();
    if (obj.hasKey("/Extensions"))
    {
        obj = obj.getKey("/Extensions");
        if (obj.isDictionary() && obj.hasKey("/ADBE"))
        {
            obj = obj.getKey("/ADBE");
            if (obj.isDictionary() && obj.hasKey("/ExtensionLevel"))
            {
                obj = obj.getKey("/ExtensionLevel");
                if (obj.isInteger())
                {
                    result = obj.getIntValue();
                }
            }
        }
    }
    return result;
}

QPDFObjectHandle
QPDF::getTrailer()
{
    return this->trailer;
}

QPDFObjectHandle
QPDF::getRoot()
{
    return this->trailer.getKey("/Root");
}

void
QPDF::getObjectStreamData(std::map<int, int>& omap)
{
    for (std::map<QPDFObjGen, QPDFXRefEntry>::iterator iter =
	     this->xref_table.begin();
	 iter != this->xref_table.end(); ++iter)
    {
	QPDFObjGen const& og = (*iter).first;
	QPDFXRefEntry const& entry = (*iter).second;
	if (entry.getType() == 2)
	{
	    omap[og.getObj()] = entry.getObjStreamNumber();
	}
    }
}

std::vector<QPDFObjGen>
QPDF::getCompressibleObjGens()
{
    // Return a list of objects that are allowed to be in object
    // streams.  Walk through the objects by traversing the document
    // from the root, including a traversal of the pages tree.  This
    // makes that objects that are on the same page are more likely to
    // be in the same object stream, which is slightly more efficient,
    // particularly with linearized files.  This is better than
    // iterating through the xref table since it avoids preserving
    // orphaned items.

    // Exclude encryption dictionary, if any
    QPDFObjectHandle encryption_dict = trailer.getKey("/Encrypt");
    QPDFObjGen encryption_dict_og = encryption_dict.getObjGen();

    std::set<QPDFObjGen> visited;
    std::list<QPDFObjectHandle> queue;
    queue.push_front(this->trailer);
    std::vector<QPDFObjGen> result;
    while (! queue.empty())
    {
	QPDFObjectHandle obj = queue.front();
	queue.pop_front();
	if (obj.isIndirect())
	{
	    QPDFObjGen og = obj.getObjGen();
	    if (visited.count(og))
	    {
		QTC::TC("qpdf", "QPDF loop detected traversing objects");
		continue;
	    }
	    if (og == encryption_dict_og)
	    {
		QTC::TC("qpdf", "QPDF exclude encryption dictionary");
	    }
	    else if (! obj.isStream())
	    {
		result.push_back(og);
	    }
	    visited.insert(og);
	}
	if (obj.isStream())
	{
	    QPDFObjectHandle dict = obj.getDict();
	    std::set<std::string> keys = dict.getKeys();
	    for (std::set<std::string>::reverse_iterator iter = keys.rbegin();
		 iter != keys.rend(); ++iter)
	    {
		std::string const& key = *iter;
		QPDFObjectHandle value = dict.getKey(key);
		if (key == "/Length")
		{
		    // omit stream lengths
		    if (value.isIndirect())
		    {
			QTC::TC("qpdf", "QPDF exclude indirect length");
		    }
		}
		else
		{
		    queue.push_front(value);
		}
	    }
	}
	else if (obj.isDictionary())
	{
	    std::set<std::string> keys = obj.getKeys();
	    for (std::set<std::string>::reverse_iterator iter = keys.rbegin();
		 iter != keys.rend(); ++iter)
	    {
		queue.push_front(obj.getKey(*iter));
	    }
	}
	else if (obj.isArray())
	{
	    int n = obj.getArrayNItems();
	    for (int i = 1; i <= n; ++i)
	    {
		queue.push_front(obj.getArrayItem(n - i));
	    }
	}
    }

    return result;
}

void
QPDF::pipeStreamData(int objid, int generation,
		     qpdf_offset_t offset, size_t length,
		     QPDFObjectHandle stream_dict,
		     Pipeline* pipeline)
{
    std::vector<PointerHolder<Pipeline> > to_delete;
    if (this->encrypted)
    {
	decryptStream(pipeline, objid, generation, stream_dict, to_delete);
    }

    try
    {
	this->file->seek(offset, SEEK_SET);
	char buf[10240];
	while (length > 0)
	{
	    size_t to_read = (sizeof(buf) < length ? sizeof(buf) : length);
	    size_t len = this->file->read(buf, to_read);
	    if (len == 0)
	    {
		throw QPDFExc(qpdf_e_damaged_pdf,
			      this->file->getName(),
			      this->last_object_description,
			      this->file->getLastOffset(),
			      "unexpected EOF reading stream data");
	    }
	    length -= len;
	    pipeline->write(QUtil::unsigned_char_pointer(buf), len);
	}
    }
    catch (QPDFExc& e)
    {
	warn(e);
    }
    catch (std::runtime_error& e)
    {
	QTC::TC("qpdf", "QPDF decoding error warning");
	warn(QPDFExc(qpdf_e_damaged_pdf, this->file->getName(),
		     "", this->file->getLastOffset(),
		     "error decoding stream data for object " +
		     QUtil::int_to_string(objid) + " " +
		     QUtil::int_to_string(generation) + ": " + e.what()));
    }
    pipeline->finish();
}

void
QPDF::findAttachmentStreams()
{
    QPDFObjectHandle root = getRoot();
    QPDFObjectHandle names = root.getKey("/Names");
    if (! names.isDictionary())
    {
        return;
    }
    QPDFObjectHandle embeddedFiles = names.getKey("/EmbeddedFiles");
    if (! embeddedFiles.isDictionary())
    {
        return;
    }
    names = embeddedFiles.getKey("/Names");
    if (! names.isArray())
    {
        return;
    }
    for (int i = 0; i < names.getArrayNItems(); ++i)
    {
        QPDFObjectHandle item = names.getArrayItem(i);
        if (item.isDictionary() &&
            item.getKey("/Type").isName() &&
            (item.getKey("/Type").getName() == "/Filespec") &&
            item.getKey("/EF").isDictionary() &&
            item.getKey("/EF").getKey("/F").isStream())
        {
            QPDFObjectHandle stream = item.getKey("/EF").getKey("/F");
            this->attachment_streams.insert(stream.getObjGen());
        }
    }
}
