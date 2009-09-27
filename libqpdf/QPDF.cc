#include <qpdf/QPDF.hh>

#include <vector>
#include <map>
#include <string.h>
#include <memory.h>

#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/PCRE.hh>
#include <qpdf/Pipeline.hh>
#include <qpdf/Pl_Discard.hh>

#include <qpdf/QPDFExc.hh>
#include <qpdf/QPDF_Null.hh>
#include <qpdf/QPDF_Dictionary.hh>

void
QPDF::InputSource::setLastOffset(off_t offset)
{
    this->last_offset = offset;
}

off_t
QPDF::InputSource::getLastOffset() const
{
    return this->last_offset;
}

std::string
QPDF::InputSource::readLine()
{
    // Read a line terminated by one or more \r or \n characters
    // without caring what the exact terminator is.  Consume the
    // trailing newline characters but don't return them.

    off_t offset = this->tell();
    std::string buf;
    enum { st_before_nl, st_at_nl } state = st_before_nl;
    char ch;
    while (1)
    {
	size_t len = this->read(&ch, 1);
	if (len == 0)
	{
	    break;
	}

	if (state == st_before_nl)
	{
	    if ((ch == '\012') || (ch == '\015'))
	    {
		state = st_at_nl;
	    }
	    else
	    {
		buf += ch;
	    }
	}
	else if (state == st_at_nl)
	{
	    if ((ch == '\012') || (ch == '\015'))
	    {
		// do nothing
	    }
	    else
	    {
		// unread this character
		this->unreadCh(ch);
		break;
	    }
	}
    }
    // Override last offset to be where we started this line rather
    // than before the last character read
    this->last_offset = offset;
    return buf;
}

QPDF::FileInputSource::FileInputSource() :
    file(0)
{
}

void
QPDF::FileInputSource::setFilename(char const* filename)
{
    destroy();
    this->filename = filename;
    this->file = QUtil::fopen_wrapper(std::string("open ") + this->filename,
				      fopen(this->filename.c_str(), "rb"));
}

QPDF::FileInputSource::~FileInputSource()
{
    destroy();
}

void
QPDF::FileInputSource::destroy()
{
    if (this->file)
    {
	fclose(this->file);
	this->file = 0;
    }
}

std::string const&
QPDF::FileInputSource::getName() const
{
    return this->filename;
}

off_t
QPDF::FileInputSource::tell()
{
    return ftell(this->file);
}

void
QPDF::FileInputSource::seek(off_t offset, int whence)
{
    QUtil::os_wrapper(std::string("seek to ") + this->filename + ", offset " +
		      QUtil::int_to_string(offset) + " (" +
		      QUtil::int_to_string(whence) + ")",
		      fseek(this->file, offset, whence));
}

void
QPDF::FileInputSource::rewind()
{
    ::rewind(this->file);
}

size_t
QPDF::FileInputSource::read(char* buffer, int length)
{
    this->last_offset = ftell(this->file);
    size_t len = fread(buffer, 1, length, this->file);
    if ((len == 0) && ferror(this->file))
    {
	throw QPDFExc(this->filename, this->last_offset,
		      std::string("read ") +
		      QUtil::int_to_string(length) + " bytes");
    }
    return len;
}

void
QPDF::FileInputSource::unreadCh(char ch)
{
    QUtil::os_wrapper(this->filename + ": unread character",
		      ungetc((unsigned char)ch, this->file));
}

QPDF::BufferInputSource::BufferInputSource(std::string const& description,
					   Buffer* buf) :
    description(description),
    buf(buf),
    cur_offset(0)
{
}

QPDF::BufferInputSource::~BufferInputSource()
{
}

std::string const&
QPDF::BufferInputSource::getName() const
{
    return this->description;
}

off_t
QPDF::BufferInputSource::tell()
{
    return this->cur_offset;
}

void
QPDF::BufferInputSource::seek(off_t offset, int whence)
{
    switch (whence)
    {
      case SEEK_SET:
	this->cur_offset = offset;
	break;

      case SEEK_END:
	this->cur_offset = this->buf->getSize() - offset;
	break;

      case SEEK_CUR:
	this->cur_offset += offset;
	break;

      default:
	throw std::logic_error(
	    "INTERNAL ERROR: invalid argument to BufferInputSource::seek");
	break;
    }
}

void
QPDF::BufferInputSource::rewind()
{
    this->cur_offset = 0;
}

size_t
QPDF::BufferInputSource::read(char* buffer, int length)
{
    off_t end_pos = this->buf->getSize();
    if (this->cur_offset >= end_pos)
    {
	this->last_offset = end_pos;
	return 0;
    }

    this->last_offset = this->cur_offset;
    size_t len = std::min((int)(end_pos - this->cur_offset), length);
    memcpy(buffer, buf->getBuffer() + this->cur_offset, len);
    this->cur_offset += len;
    return len;
}

void
QPDF::BufferInputSource::unreadCh(char ch)
{
    if (this->cur_offset > 0)
    {
	--this->cur_offset;
    }
}

QPDF::ObjGen::ObjGen(int o = 0, int g = 0) :
    obj(o),
    gen(g)
{
}

bool
QPDF::ObjGen::operator<(ObjGen const& rhs) const
{
    return ((this->obj < rhs.obj) ||
	    ((this->obj == rhs.obj) && (this->gen < rhs.gen)));
}

DLL_EXPORT
QPDF::QPDF() :
    encrypted(false),
    encryption_initialized(false),
    ignore_xref_streams(false),
    suppress_warnings(false),
    attempt_recovery(true),
    cached_key_objid(0),
    cached_key_generation(0),
    first_xref_item_offset(0),
    uncompressed_after_compressed(false)
{
}

DLL_EXPORT
QPDF::~QPDF()
{
}

DLL_EXPORT
void
QPDF::processFile(char const* filename, char const* password)
{
    this->file.setFilename(filename);
    if (password)
    {
	this->provided_password = password;
    }
    parse();
}

DLL_EXPORT
void
QPDF::setIgnoreXRefStreams(bool val)
{
    this->ignore_xref_streams = val;
}

DLL_EXPORT
void
QPDF::setSuppressWarnings(bool val)
{
    this->suppress_warnings = val;
}

DLL_EXPORT
void
QPDF::setAttemptRecovery(bool val)
{
    this->attempt_recovery = val;
}

DLL_EXPORT
std::vector<std::string>
QPDF::getWarnings()
{
    std::vector<std::string> result = this->warnings;
    this->warnings.clear();
    return result;
}

void
QPDF::parse()
{
    static PCRE header_re("^%PDF-(1.\\d+)\\b");
    static PCRE eof_re("(?s:startxref\\s+(\\d+)\\s+%%EOF\\b)");

    std::string line = this->file.readLine();
    PCRE::Match m1 = header_re.match(line.c_str());
    if (m1)
    {
	this->pdf_version = m1.getMatch(1);
	if (atof(this->pdf_version.c_str()) < 1.2)
	{
	    this->tokenizer.allowPoundAnywhereInName();
	}
    }
    else
    {
	QTC::TC("qpdf", "QPDF not a pdf file");
	throw QPDFExc(this->file.getName(), 0, "not a PDF file");
    }

    // PDF spec says %%EOF must be found within the last 1024 bytes of
    // the file.  We add an extra 30 characters to leave room for the
    // startxref stuff.
    static int const tbuf_size = 1054;
    this->file.seek(0, SEEK_END);
    if (this->file.tell() > tbuf_size)
    {
	this->file.seek(-tbuf_size, SEEK_END);
    }
    else
    {
	this->file.rewind();
    }
    char* buf = new char[tbuf_size + 1];
    // Put buf in a PointerHolder to guarantee deletion of buf.  This
    // calls delete rather than delete [], but it's okay since buf is
    // an array of fundamental types.
    PointerHolder<char> b(buf);
    memset(buf, '\0', tbuf_size + 1);
    this->file.read(buf, tbuf_size);

    // Since buf may contain null characters, we can't do a regexp
    // search on buf directly.  Find the last occurrence within buf
    // where the regexp matches.
    char* p = buf;
    char const* candidate = "";
    while ((p = (char*)memchr(p, 's', tbuf_size - (p - buf))) != 0)
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
	    throw QPDFExc(this->file.getName() + ": can't find startxref");
	}
	off_t xref_offset = atoi(m2.getMatch(1).c_str());
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
}

void
QPDF::warn(QPDFExc const& e)
{
    this->warnings.push_back(e.what());
    if (! this->suppress_warnings)
    {
	std::cerr << "WARNING: " << this->warnings.back() << std::endl;
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
    static PCRE obj_re("^(\\d+) (\\d+) obj\\b");
    static PCRE endobj_re("^endobj\\b");
    static PCRE trailer_re("^trailer\\b");

    warn(QPDFExc(this->file.getName(), 0, "file is damaged"));
    warn(e);
    warn(QPDFExc("Attempting to reconstruct cross-reference table"));

    this->file.seek(0, SEEK_END);
    off_t eof = this->file.tell();
    this->file.seek(0, SEEK_SET);
    bool in_obj = false;
    while (this->file.tell() < eof)
    {
	std::string line = this->file.readLine();
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
		int offset = this->file.getLastOffset();
		insertXrefEntry(obj, 1, offset, gen, true);
	    }
	    else if ((! this->trailer.isInitialized()) &&
		     trailer_re.match(line.c_str()))
	    {
		// read "trailer"
		this->file.seek(this->file.getLastOffset(), SEEK_SET);
		readToken(&this->file);
		QPDFObjectHandle t = readObject(&this->file, 0, 0, false);
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

	throw QPDFExc(this->file.getName() + ": unable to find trailer "
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
QPDF::read_xref(off_t xref_offset)
{
    std::map<int, int> free_table;
    while (xref_offset)
    {
	this->file.seek(xref_offset, SEEK_SET);
	std::string line = this->file.readLine();
	if (line == "xref")
	{
	    xref_offset = read_xrefTable(this->file.tell());
	}
	else
	{
	    xref_offset = read_xrefStream(xref_offset);
	}
    }

    int size = this->trailer.getKey("/Size").getIntValue();
    int max_obj = (*(xref_table.rbegin())).first.obj;
    if (! this->deleted_objects.empty())
    {
	max_obj = std::max(max_obj, *(this->deleted_objects.rbegin()));
    }
    if (size != max_obj + 1)
    {
	QTC::TC("qpdf", "QPDF xref size mismatch");
	warn(QPDFExc(this->file.getName() +
		     std::string(": reported number of objects (") +
		     QUtil::int_to_string(size) +
		     ") inconsistent with actual number of objects (" +
		     QUtil::int_to_string(max_obj + 1) + ")"));
    }

    // We no longer need the deleted_objects table, so go ahead and
    // clear it out to make sure we never depend on its being set.
    this->deleted_objects.clear();
}

int
QPDF::read_xrefTable(off_t xref_offset)
{
    static PCRE xref_first_re("^(\\d+)\\s+(\\d+)");
    static PCRE xref_entry_re("(?s:(^\\d{10}) (\\d{5}) ([fn])[ \r\n]{2}$)");

    std::vector<ObjGen> deleted_items;

    this->file.seek(xref_offset, SEEK_SET);
    bool done = false;
    while (! done)
    {
	std::string line = this->file.readLine();
	PCRE::Match m1 = xref_first_re.match(line.c_str());
	if (! m1)
	{
	    QTC::TC("qpdf", "QPDF invalid xref");
	    throw QPDFExc(this->file.getName(), this->file.getLastOffset(),
			  "xref syntax invalid");
	}
	int obj = atoi(m1.getMatch(1).c_str());
	int num = atoi(m1.getMatch(2).c_str());
	static int const xref_entry_size = 20;
	char xref_entry[xref_entry_size + 1];
	for (int i = obj; i < obj + num; ++i)
	{
	    if (i == 0)
	    {
		// This is needed by checkLinearization()
		this->first_xref_item_offset = this->file.tell();
	    }
	    memset(xref_entry, 0, sizeof(xref_entry));
	    this->file.read(xref_entry, xref_entry_size);
	    PCRE::Match m2 = xref_entry_re.match(xref_entry);
	    if (! m2)
	    {
		QTC::TC("qpdf", "QPDF invalid xref entry");
		throw QPDFExc(
		    this->file.getName(), this->file.getLastOffset(),
		    "invalid xref entry (obj=" +
		    QUtil::int_to_string(i) + ")");
	    }

	    int f1 = atoi(m2.getMatch(1).c_str());
	    int f2 = atoi(m2.getMatch(2).c_str());
	    char type = m2.getMatch(3)[0];
	    if (type == 'f')
	    {
		// Save deleted items until after we've checked the
		// XRefStm, if any.
		deleted_items.push_back(ObjGen(i, f2));
	    }
	    else
	    {
		insertXrefEntry(i, 1, f1, f2);
	    }
	}
	off_t pos = this->file.tell();
	QPDFTokenizer::Token t = readToken(&this->file);
	if (t == QPDFTokenizer::Token(QPDFTokenizer::tt_word, "trailer"))
	{
	    done = true;
	}
	else
	{
	    this->file.seek(pos, SEEK_SET);
	}
    }

    // Set offset to previous xref table if any
    QPDFObjectHandle cur_trailer = readObject(&this->file, 0, 0, false);
    if (! cur_trailer.isDictionary())
    {
	QTC::TC("qpdf", "QPDF missing trailer");
	throw QPDFExc(this->file.getName(), this->file.getLastOffset(),
		      "expected trailer dictionary");
    }

    if (! this->trailer.isInitialized())
    {
	setTrailer(cur_trailer);

	if (! this->trailer.hasKey("/Size"))
	{
	    QTC::TC("qpdf", "QPDF trailer lacks size");
	    throw QPDFExc(this->file.getName(), this->file.getLastOffset(),
			  "trailer dictionary lacks /Size key");
	}
	if (! this->trailer.getKey("/Size").isInteger())
	{
	    QTC::TC("qpdf", "QPDF trailer size not integer");
	    throw QPDFExc(this->file.getName(), this->file.getLastOffset(),
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
		throw QPDFExc(this->file.getName(), xref_offset,
			      "invalid /XRefStm");
	    }
	}
    }

    // Handle any deleted items now that we've read the /XRefStm.
    for (std::vector<ObjGen>::iterator iter = deleted_items.begin();
	 iter != deleted_items.end(); ++iter)
    {
	ObjGen& og = *iter;
	insertXrefEntry(og.obj, 0, 0, og.gen);
    }

    if (cur_trailer.hasKey("/Prev"))
    {
	if (! cur_trailer.getKey("/Prev").isInteger())
	{
	    QTC::TC("qpdf", "QPDF trailer prev not integer");
	    throw QPDFExc(this->file.getName(), this->file.getLastOffset(),
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

int
QPDF::read_xrefStream(off_t xref_offset)
{
    bool found = false;
    if (! this->ignore_xref_streams)
    {
	int xobj;
	int xgen;
	QPDFObjectHandle xref_obj;
	try
	{
	    xref_obj = readObjectAtOffset(xref_offset, 0, 0, xobj, xgen);
	}
	catch (QPDFExc& e)
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
	throw QPDFExc(this->file.getName(), xref_offset, "xref not found");
    }

    return xref_offset;
}

int
QPDF::processXRefStream(off_t xref_offset, QPDFObjectHandle& xref_obj)
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
	throw QPDFExc(this->file.getName(), xref_offset,
		      "Cross-reference stream does not have"
		      " proper /W and /Index keys");
    }
    std::vector<int> indx;
    if (Index_obj.isArray())
    {
	int n_index = Index_obj.getArrayNItems();
	if ((n_index % 2) || (n_index < 2))
	{
	    throw QPDFExc(this->file.getName(), xref_offset,
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
		throw QPDFExc(this->file.getName(), xref_offset,
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
	int size = dict.getKey("/Size").getIntValue();
	indx.push_back(0);
	indx.push_back(size);
    }

    int num_entries = 0;
    for (unsigned int i = 1; i < indx.size(); i += 2)
    {
	num_entries += indx[i];
    }

    int W[3];
    int entry_size = 0;
    for (int i = 0; i < 3; ++i)
    {
	W[i] = W_obj.getArrayItem(i).getIntValue();
	entry_size += W[i];
    }

    int expected_size = entry_size * num_entries;

    PointerHolder<Buffer> bp = xref_obj.getStreamData();
    int actual_size = bp.getPointer()->getSize();

    if (expected_size != actual_size)
    {
	throw QPDFExc(this->file.getName(), xref_offset,
		      "Cross-reference stream data has the wrong size;"
		      " expected = " + QUtil::int_to_string(expected_size) +
		      "; actual = " + QUtil::int_to_string(actual_size));
    }

    int cur_chunk = 0;
    int chunk_count = 0;

    bool saw_first_compressed_object = false;

    unsigned char const* data = bp.getPointer()->getBuffer();
    for (int i = 0; i < num_entries; ++i)
    {
	// Read this entry
	unsigned char const* entry = data + (entry_size * i);
	int fields[3];
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
		fields[j] += (int)(*p++);
	    }
	}

	// Get the object and generation number.  The object number is
	// based on /Index.  The generation number is 0 unless this is
	// an uncompressed object record, in which case the generation
	// number appears as the third field.
	int obj = indx[cur_chunk] + chunk_count;
	++chunk_count;
	if (chunk_count >= indx[cur_chunk + 1])
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
	insertXrefEntry(obj, fields[0], fields[1], fields[2]);
    }

    if (! this->trailer.isInitialized())
    {
	setTrailer(dict);
    }

    if (dict.hasKey("/Prev"))
    {
	if (! dict.getKey("/Prev").isInteger())
	{
	    throw QPDFExc(this->file.getName(), this->file.getLastOffset(),
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
QPDF::insertXrefEntry(int obj, int f0, int f1, int f2, bool overwrite)
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
	ObjGen og(obj, gen);
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
	this->xref_table[ObjGen(obj, f2)] = QPDFXRefEntry(f0, f1, f2);
	break;

      case 2:
	this->xref_table[ObjGen(obj, 0)] = QPDFXRefEntry(f0, f1, f2);
	break;

      default:
	throw QPDFExc(this->file.getName(), 0,
		      "unknown xref stream entry type " +
		      QUtil::int_to_string(f0));
	break;
    }
}

DLL_EXPORT
void
QPDF::showXRefTable()
{
    for (std::map<ObjGen, QPDFXRefEntry>::iterator iter =
	     this->xref_table.begin();
	 iter != this->xref_table.end(); ++iter)
    {
	ObjGen const& og = (*iter).first;
	QPDFXRefEntry const& entry = (*iter).second;
	std::cout << og.obj << "/" << og.gen << ": ";
	switch (entry.getType())
	{
	  case 1:
	    std::cout << "uncompressed; offset = " << entry.getOffset();
	    break;

	  case 2:
	    std::cout << "compressed; stream = " << entry.getObjStreamNumber()
		      << ", index = " << entry.getObjStreamIndex();
	    break;

	  default:
	    throw std::logic_error("unknown cross-reference table type while"
				   " showing xref_table");
	    break;
	}
	std::cout << std::endl;
    }
}

QPDFObjectHandle
QPDF::readObject(InputSource* input, int objid, int generation,
		 bool in_object_stream)
{
    off_t offset = input->tell();
    QPDFObjectHandle object = readObjectInternal(
	input, objid, generation, in_object_stream, false, false);
    // Override last_offset so that it points to the beginning of the
    // object we just read
    input->setLastOffset(offset);
    return object;
}

QPDFObjectHandle
QPDF::readObjectInternal(InputSource* input,
			 int objid, int generation,
			 bool in_object_stream,
			 bool in_array, bool in_dictionary)
{
    if (in_dictionary && in_array)
    {
	// Although dictionaries and arrays arbitrarily nest, these
	// variables indicate what is at the top of the stack right
	// now, so they can, by definition, never both be true.
	throw std::logic_error(
	    "INTERNAL ERROR: readObjectInternal: in_dict && in_array");
    }

    QPDFObjectHandle object;

    off_t offset = input->tell();
    std::vector<QPDFObjectHandle> olist;
    bool done = false;
    while (! done)
    {
	object = QPDFObjectHandle();

	QPDFTokenizer::Token token = readToken(input);

	switch (token.getType())
	{
	  case QPDFTokenizer::tt_brace_open:
	  case QPDFTokenizer::tt_brace_close:
	    // Don't know what to do with these for now
	    QTC::TC("qpdf", "QPDF bad brace");
	    throw QPDFExc(input->getName(), input->getLastOffset(),
			  "unexpected brace token");
	    break;

	  case QPDFTokenizer::tt_array_close:
	    if (in_array)
	    {
		done = true;
	    }
	    else
	    {
		QTC::TC("qpdf", "QPDF bad array close");
		throw QPDFExc(input->getName(), input->getLastOffset(),
			      "unexpected array close token");
	    }
	    break;

	  case QPDFTokenizer::tt_dict_close:
	    if (in_dictionary)
	    {
		done = true;
	    }
	    else
	    {
		QTC::TC("qpdf", "QPDF bad dictionary close");
		throw QPDFExc(input->getName(), input->getLastOffset(),
			      "unexpected dictionary close token");
	    }
	    break;

	  case QPDFTokenizer::tt_array_open:
	    object = readObjectInternal(
		input, objid, generation, in_object_stream, true, false);
	    break;

	  case QPDFTokenizer::tt_dict_open:
	    object = readObjectInternal(
		input, objid, generation, in_object_stream, false, true);
	    break;

	  case QPDFTokenizer::tt_bool:
	    object = QPDFObjectHandle::newBool(
		(token.getValue() == "true"));
	    break;

	  case QPDFTokenizer::tt_null:
	    object = QPDFObjectHandle::newNull();
	    break;

	  case QPDFTokenizer::tt_integer:
	    object = QPDFObjectHandle::newInteger(
		atoi(token.getValue().c_str()));
	    break;

	  case QPDFTokenizer::tt_real:
	    object = QPDFObjectHandle::newReal(token.getValue());
	    break;

	  case QPDFTokenizer::tt_name:
	    object = QPDFObjectHandle::newName(token.getValue());
	    break;

	  case QPDFTokenizer::tt_word:
	    {
		std::string const& value = token.getValue();
		if ((value == "R") && (in_array || in_dictionary) &&
		    (olist.size() >= 2) &&
		    (olist[olist.size() - 1].isInteger()) &&
		    (olist[olist.size() - 2].isInteger()))
		{
		    // Try to resolve indirect objects
		    object = QPDFObjectHandle::Factory::newIndirect(
			this,
			olist[olist.size() - 2].getIntValue(),
			olist[olist.size() - 1].getIntValue());
		    olist.pop_back();
		    olist.pop_back();
		}
		else
		{
		    throw QPDFExc(input->getName(), input->getLastOffset(),
				  "unknown token while reading object (" +
				  value + ")");
		}
	    }
	    break;

	  case QPDFTokenizer::tt_string:
	    {
		std::string val = token.getValue();
		if (this->encrypted && (! in_object_stream))
		{
		    decryptString(val, objid, generation);
		}
		object = QPDFObjectHandle::newString(val);
	    }
	    break;

	  default:
	    throw QPDFExc(input->getName(), input->getLastOffset(),
			  "unknown token type while reading object");
	    break;
	}

	if (in_dictionary || in_array)
	{
	    if (! done)
	    {
		olist.push_back(object);
	    }
	}
	else if (! object.isInitialized())
	{
	    throw std::logic_error(
		"INTERNAL ERROR: uninitialized object (token = " +
		QUtil::int_to_string(token.getType()) +
		", " + token.getValue() + ")");
	}
	else
	{
	    done = true;
	}
    }

    if (in_array)
    {
	object = QPDFObjectHandle::newArray(olist);
    }
    else if (in_dictionary)
    {
	// Convert list to map.  Alternating elements are keys.
	std::map<std::string, QPDFObjectHandle> dict;
	if (olist.size() % 2)
	{
	    QTC::TC("qpdf", "QPDF dictionary odd number of elements");
	    throw QPDFExc(
		input->getName(), input->getLastOffset(),
		"dictionary ending here has an odd number of elements");
	}
	for (unsigned int i = 0; i < olist.size(); i += 2)
	{
	    QPDFObjectHandle key_obj = olist[i];
	    QPDFObjectHandle val = olist[i + 1];
	    if (! key_obj.isName())
	    {
		throw QPDFExc(
		    input->getName(), offset,
		    std::string("dictionary key not name (") +
		    key_obj.unparse() + ")");
	    }
	    dict[key_obj.getName()] = val;
	}
	object = QPDFObjectHandle::newDictionary(dict);

	if (! in_object_stream)
	{
	    // check for stream
	    off_t cur_offset = input->tell();
	    if (readToken(input) ==
		QPDFTokenizer::Token(QPDFTokenizer::tt_word, "stream"))
	    {
		// Kill to next actual newline.  Do not use readLine()
		// here -- streams are a special case.  The next
		// single newline character marks the end of the
		// stream token.  It is incorrect to strip subsequent
		// carriage returns or newlines as they may be part of
		// the stream.
		{
		    char ch;
		    do
		    {
			if (input->read(&ch, 1) == 0)
			{
			    // A premature EOF here will result in
			    // some other problem that will get
			    // reported at another time.
			    ch = '\n';
			}
		    } while (ch != '\n');
		}

		// Must get offset before accessing any additional
		// objects since resolving a previously unresolved
		// indirect object will change file position.
		off_t stream_offset = input->tell();
		int length = 0;

		try
		{
		    if (dict.count("/Length") == 0)
		    {
			QTC::TC("qpdf", "QPDF stream without length");
			throw QPDFExc(input->getName(), offset,
				      "stream dictionary lacks /Length key");
		    }

		    QPDFObjectHandle length_obj = dict["/Length"];
		    if (! length_obj.isInteger())
		    {
			QTC::TC("qpdf", "QPDF stream length not integer");
			throw QPDFExc(input->getName(), offset,
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
			throw QPDFExc(input->getName(), input->getLastOffset(),
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
    }

    return object;
}

int
QPDF::recoverStreamLength(InputSource* input,
			  int objid, int generation, off_t stream_offset)
{
    static PCRE endobj_re("^endobj\\b");

    // Try to reconstruct stream length by looking for
    // endstream(\r\n?|\n)endobj
    warn(QPDFExc(input->getName(), stream_offset,
		 "attempting to recover stream length"));

    input->seek(0, SEEK_END);
    off_t eof = input->tell();
    input->seek(stream_offset, SEEK_SET);
    std::string last_line;
    off_t last_line_offset = 0;
    int length = 0;
    while (input->tell() < eof)
    {
	std::string line = input->readLine();
	// Can't use regexp last_line since it might contain nulls
	if (endobj_re.match(line.c_str()) &&
	    (last_line.length() >= 9) &&
	    (last_line.substr(last_line.length() - 9, 9) == "endstream"))
	{
	    // Stream probably ends right before "endstream", which
	    // contains 9 characters.
	    length = last_line_offset + last_line.length() - 9 - stream_offset;
	    // Go back to where we would have been if we had just read
	    // the endstream.
	    input->seek(input->getLastOffset(), SEEK_SET);
	    break;
	}
	last_line = line;
	last_line_offset = input->getLastOffset();
    }

    if (length)
    {
	int this_obj_offset = 0;
	ObjGen this_obj(0, 0);

	// Make sure this is inside this object
	for (std::map<ObjGen, QPDFXRefEntry>::iterator iter =
		 this->xref_table.begin();
	     iter != this->xref_table.end(); ++iter)
	{
	    ObjGen const& og = (*iter).first;
	    QPDFXRefEntry const& entry = (*iter).second;
	    if (entry.getType() == 1)
	    {
		int obj_offset = entry.getOffset();
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
	    (this_obj.obj == objid) &&
	    (this_obj.gen == generation))
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
	throw QPDFExc(input->getName(), stream_offset,
		      "unable to recover stream data");
    }

    QTC::TC("qpdf", "QPDF recovered stream length");
    return length;
}

QPDFTokenizer::Token
QPDF::readToken(InputSource* input)
{
    off_t offset = input->tell();
    QPDFTokenizer::Token token;
    bool unread_char;
    char char_to_unread;
    while (! this->tokenizer.getToken(token, unread_char, char_to_unread))
    {
	char ch;
	if (input->read(&ch, 1) == 0)
	{
	    throw QPDFExc(input->getName(), offset, "EOF while reading token");
	}
	else
	{
	    if (isspace((unsigned char)ch) &&
		(input->getLastOffset() == offset))
	    {
		++offset;
	    }
	    this->tokenizer.presentCharacter(ch);
	}
    }

    if (unread_char)
    {
	input->unreadCh(char_to_unread);
    }

    if (token.getType() == QPDFTokenizer::tt_bad)
    {
	throw QPDFExc(input->getName(), offset, token.getErrorMessage());
    }

    input->setLastOffset(offset);

    return token;
}

QPDFObjectHandle
QPDF::readObjectAtOffset(off_t offset, int exp_objid, int exp_generation,
			 int& objid, int& generation)
{
    this->file.seek(offset, SEEK_SET);

    QPDFTokenizer::Token tobjid = readToken(&this->file);
    QPDFTokenizer::Token tgen = readToken(&this->file);
    QPDFTokenizer::Token tobj = readToken(&this->file);

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
	    throw QPDFExc(this->file.getName(), offset, "expected n n obj");
	}
	objid = atoi(tobjid.getValue().c_str());
	generation = atoi(tgen.getValue().c_str());

	if (exp_objid &&
	    (! ((objid == exp_objid) && (generation == exp_generation))))
	{
	    QTC::TC("qpdf", "QPDF err wrong objid/generation");
	    throw QPDFExc(this->file.getName(), offset,
			  std::string("expected ") +
			  QUtil::int_to_string(exp_objid) + " " +
			  QUtil::int_to_string(exp_generation) + " obj");
	}
    }
    catch (QPDFExc& e)
    {
	if (exp_objid && this->attempt_recovery)
	{
	    // Try again after reconstructing xref table
	    reconstruct_xref(e);
	    ObjGen og(exp_objid, exp_generation);
	    if (this->xref_table.count(og) &&
		(this->xref_table[og].getType() == 1))
	    {
		off_t new_offset = this->xref_table[og].getOffset();
		// Call readObjectAtOffset with 0 for exp_objid to
		// avoid an infinite loop.
		QPDFObjectHandle result =
		    readObjectAtOffset(new_offset, 0, 0, objid, generation);
		QTC::TC("qpdf", "QPDF recovered in readObjectAtOffset");
		return result;
	    }
	}
	else
	{
	    throw e;
	}
    }

    QPDFObjectHandle oh = readObject(
	&this->file, objid, generation, false);

    if (! (readToken(&this->file) ==
	   QPDFTokenizer::Token(QPDFTokenizer::tt_word, "endobj")))
    {
	QTC::TC("qpdf", "QPDF err expected endobj");
	warn(QPDFExc(this->file.getName(), this->file.getLastOffset(),
		     "expected endobj"));
    }

    ObjGen og(objid, generation);
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
	off_t end_before_space = this->file.tell();

	// skip over spaces
	while (true)
	{
	    char ch;
	    if (this->file.read(&ch, 1))
	    {
		if (! isspace((unsigned char)ch))
		{
		    this->file.seek(-1, SEEK_CUR);
		    break;
		}
	    }
	    else
	    {
		throw QPDFExc(this->file.getName(), offset,
			      "EOF after endobj");
	    }
	}
	off_t end_after_space = this->file.tell();

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
    ObjGen og(objid, generation);
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
		off_t offset = entry.getOffset();
		// Object stored in cache by readObjectAtOffset
		int aobjid;
		int ageneration;
		QPDFObjectHandle oh =
		    readObjectAtOffset(offset, objid, generation,
				       aobjid, ageneration);
	    }
	    break;

	  case 2:
	    resolveObjectsInStream(entry.getObjStreamNumber());
	    break;

	  default:
	    throw QPDFExc(this->file.getName(), 0,
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
	throw QPDFExc(this->file.getName(), this->file.getLastOffset(),
		      "supposed object stream " +
		      QUtil::int_to_string(obj_stream_number) +
		      " is not a stream");
    }

    // For linearization data in the object, use the data from the
    // object stream for the objects in the stream.
    ObjGen stream_og(obj_stream_number, 0);
    off_t end_before_space = this->obj_cache[stream_og].end_before_space;
    off_t end_after_space = this->obj_cache[stream_og].end_after_space;

    QPDFObjectHandle dict = obj_stream.getDict();
    if (! (dict.getKey("/Type").isName() &&
	   dict.getKey("/Type").getName() == "/ObjStm"))
    {
	throw QPDFExc(this->file.getName(), this->file.getLastOffset(),
		      "supposed object stream " +
		      QUtil::int_to_string(obj_stream_number) +
		      " has wrong type");
    }

    if (! (dict.getKey("/N").isInteger() &&
	   dict.getKey("/First").isInteger()))
    {
	throw QPDFExc(this->file.getName(), this->file.getLastOffset(),
		      "object stream " +
		      QUtil::int_to_string(obj_stream_number) +
		      " has incorrect keys");
    }

    int n = dict.getKey("/N").getIntValue();
    int first = dict.getKey("/First").getIntValue();

    std::map<int, int> offsets;

    PointerHolder<Buffer> bp = obj_stream.getStreamData();
    BufferInputSource input(
	"object stream " + QUtil::int_to_string(obj_stream_number),
	bp.getPointer());

    for (int i = 0; i < n; ++i)
    {
	QPDFTokenizer::Token tnum = readToken(&input);
	QPDFTokenizer::Token toffset = readToken(&input);
	if (! ((tnum.getType() == QPDFTokenizer::tt_integer) &&
	       (toffset.getType() == QPDFTokenizer::tt_integer)))
	{
	    throw QPDFExc(input.getName(), input.getLastOffset(),
			  "expected integer in object stream header");
	}

	int num = atoi(tnum.getValue().c_str());
	int offset = atoi(toffset.getValue().c_str());
	offsets[num] = offset + first;
    }

    for (std::map<int, int>::iterator iter = offsets.begin();
	 iter != offsets.end(); ++iter)
    {
	int obj = (*iter).first;
	int offset = (*iter).second;
	input.seek(offset, SEEK_SET);
	QPDFObjectHandle oh = readObject(&input, obj, 0, true);

	// Store in cache
	ObjGen og(obj, 0);

	this->obj_cache[og] =
	    ObjCache(QPDFObjectHandle::ObjAccessor::getObject(oh),
		     end_before_space, end_after_space);
    }
}

DLL_EXPORT
QPDFObjectHandle
QPDF::makeIndirectObject(QPDFObjectHandle oh)
{
    ObjGen o1 = (*(this->obj_cache.rbegin())).first;
    ObjGen o2 = (*(this->xref_table.rbegin())).first;
    QTC::TC("qpdf", "QPDF indirect last obj from xref",
	    (o2.obj > o1.obj) ? 1 : 0);
    int max_objid = std::max(o1.obj, o2.obj);
    ObjGen next(max_objid + 1, 0);
    this->obj_cache[next] =
	ObjCache(QPDFObjectHandle::ObjAccessor::getObject(oh), -1, -1);
    return QPDFObjectHandle::Factory::newIndirect(this, next.obj, next.gen);
}

DLL_EXPORT
QPDFObjectHandle
QPDF::getObjectByID(int objid, int generation)
{
    return QPDFObjectHandle::Factory::newIndirect(this, objid, generation);
}

DLL_EXPORT
void
QPDF::trimTrailerForWrite()
{
    // Note that removing the encryption dictionary does not interfere
    // with reading encrypted files.  QPDF loads all the information
    // it needs from the encryption dictionary at the beginning and
    // never looks at it again.
    this->trailer.removeKey("/ID");
    this->trailer.removeKey("/Encrypt");
    this->trailer.removeKey("/Prev");

    // Remove all trailer keys that potentially come from a
    // cross-reference stream
    this->trailer.removeKey("/Index");
    this->trailer.removeKey("/W");
    this->trailer.removeKey("/Length");
    this->trailer.removeKey("/Filter");
    this->trailer.removeKey("/DecodeParms");
    this->trailer.removeKey("/Type");
    this->trailer.removeKey("/XRefStm");
}

DLL_EXPORT
std::string
QPDF::getFilename() const
{
    return this->file.getName();
}

DLL_EXPORT
std::string
QPDF::getPDFVersion() const
{
    return this->pdf_version;
}

DLL_EXPORT
QPDFObjectHandle
QPDF::getTrailer()
{
    return this->trailer;
}

DLL_EXPORT
QPDFObjectHandle
QPDF::getRoot()
{
    return this->trailer.getKey("/Root");
}

DLL_EXPORT
void
QPDF::getObjectStreamData(std::map<int, int>& omap)
{
    for (std::map<ObjGen, QPDFXRefEntry>::iterator iter =
	     this->xref_table.begin();
	 iter != this->xref_table.end(); ++iter)
    {
	ObjGen const& og = (*iter).first;
	QPDFXRefEntry const& entry = (*iter).second;
	if (entry.getType() == 2)
	{
	    omap[og.obj] = entry.getObjStreamNumber();
	}
    }
}

DLL_EXPORT
std::vector<int>
QPDF::getCompressibleObjects()
{
    // Return a set of object numbers of objects that are allowed to
    // be in object streams.  We disregard generation numbers here
    // since this is a helper function for QPDFWriter which is going
    // to renumber objects anyway.  This code will do weird things if
    // we have two objects with the same object number and different
    // generations, but so do virtually all PDF consumers,
    // particularly since this is not a permitted condition.

    // We walk through the objects by traversing the document from the
    // root, including a traversal of the pages tree.  This makes that
    // objects that are on the same page are more likely to be in the
    // same object stream, which is slightly more efficient,
    // particularly with linearized files.  This is better than
    // iterating through the xref table since it avoids preserving
    // orphaned items.

    // Exclude encryption dictionary, if any
    int encryption_dict_id = 0;
    QPDFObjectHandle encryption_dict = trailer.getKey("/Encrypt");
    if (encryption_dict.isIndirect())
    {
	encryption_dict_id = encryption_dict.getObjectID();
    }

    std::set<int> visited;
    std::list<QPDFObjectHandle> queue;
    queue.push_front(this->trailer);
    std::vector<int> result;
    while (! queue.empty())
    {
	QPDFObjectHandle obj = queue.front();
	queue.pop_front();
	if (obj.isIndirect())
	{
	    int objid = obj.getObjectID();
	    if (visited.count(objid))
	    {
		QTC::TC("qpdf", "QPDF loop detected traversing objects");
		continue;
	    }
	    if (objid == encryption_dict_id)
	    {
		QTC::TC("qpdf", "QPDF exclude encryption dictionary");
	    }
	    else if (! obj.isStream())
	    {
		result.push_back(objid);
	    }
	    visited.insert(objid);
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
		     off_t offset, size_t length,
		     QPDFObjectHandle stream_dict,
		     Pipeline* pipeline)
{
    std::vector<PointerHolder<Pipeline> > to_delete;
    if (this->encrypted)
    {
	bool xref_stream = false;
	if (stream_dict.getKey("/Type").isName() &&
	    (stream_dict.getKey("/Type").getName() == "/XRef"))
	{
	    QTC::TC("qpdf", "QPDF piping xref stream from encrypted file");
	    xref_stream = true;
	}
	if (! xref_stream)
	{
	    decryptStream(pipeline, objid, generation, to_delete);
	}
    }

    try
    {
	this->file.seek(offset, SEEK_SET);
	char buf[10240];
	while (length > 0)
	{
	    size_t to_read = (sizeof(buf) < length ? sizeof(buf) : length);
	    size_t len = this->file.read(buf, to_read);
	    if (len == 0)
	    {
		throw QPDFExc(this->file.getName(), this->file.getLastOffset(),
			      "unexpected EOF reading stream data");
	    }
	    length -= len;
	    pipeline->write((unsigned char*)buf, len);
	}
    }
    catch (std::runtime_error& e)
    {
	QTC::TC("qpdf", "QPDF decoding error warning");
	warn(QPDFExc(this->file.getName(), this->file.getLastOffset(),
		     "error decoding stream data for object " +
		     QUtil::int_to_string(objid) + " " +
		     QUtil::int_to_string(generation) + ": " + e.what()));
    }
    pipeline->finish();
}

DLL_EXPORT
void
QPDF::decodeStreams()
{
    for (std::map<ObjGen, QPDFXRefEntry>::iterator iter =
	     this->xref_table.begin();
	 iter != this->xref_table.end(); ++iter)
    {
	ObjGen const& og = (*iter).first;
	QPDFObjectHandle obj = getObjectByID(og.obj, og.gen);
	if (obj.isStream())
	{
	    Pl_Discard pl;
	    obj.pipeStreamData(&pl, true, false, false);
	}
    }
}

DLL_EXPORT
std::vector<QPDFObjectHandle> const&
QPDF::getAllPages()
{
    if (this->all_pages.empty())
    {
	getAllPagesInternal(
	    this->trailer.getKey("/Root").getKey("/Pages"), this->all_pages);
    }
    return this->all_pages;
}

void
QPDF::getAllPagesInternal(QPDFObjectHandle cur_pages,
			  std::vector<QPDFObjectHandle>& result)
{
    std::string type = cur_pages.getKey("/Type").getName();
    if (type == "/Pages")
    {
	QPDFObjectHandle kids = cur_pages.getKey("/Kids");
	int n = kids.getArrayNItems();
	for (int i = 0; i < n; ++i)
	{
	    getAllPagesInternal(kids.getArrayItem(i), result);
	}
    }
    else if (type == "/Page")
    {
	result.push_back(cur_pages);
    }
    else
    {
	throw QPDFExc(this->file.getName() + ": invalid Type in page tree");
    }
}
