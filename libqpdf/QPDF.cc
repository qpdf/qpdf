#include <qpdf/qpdf-config.h>  // include first for large file support
#include <qpdf/QPDF.hh>

#include <atomic>
#include <vector>
#include <map>
#include <algorithm>
#include <limits>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <memory.h>

#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/Pipeline.hh>
#include <qpdf/Pl_Discard.hh>
#include <qpdf/FileInputSource.hh>
#include <qpdf/BufferInputSource.hh>
#include <qpdf/OffsetInputSource.hh>

#include <qpdf/QPDFExc.hh>
#include <qpdf/QPDF_Null.hh>
#include <qpdf/QPDF_Dictionary.hh>
#include <qpdf/QPDF_Stream.hh>
#include <qpdf/QPDF_Array.hh>

std::string QPDF::qpdf_version = "10.0.1";

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

QPDF::ForeignStreamData::ForeignStreamData(
    PointerHolder<EncryptionParameters> encp,
    PointerHolder<InputSource> file,
    int foreign_objid,
    int foreign_generation,
    qpdf_offset_t offset,
    size_t length,
    bool is_attachment_stream,
    QPDFObjectHandle local_dict)
    :
    encp(encp),
    file(file),
    foreign_objid(foreign_objid),
    foreign_generation(foreign_generation),
    offset(offset),
    length(length),
    is_attachment_stream(is_attachment_stream),
    local_dict(local_dict)
{
}

QPDF::CopiedStreamDataProvider::CopiedStreamDataProvider(
    QPDF& destination_qpdf) :
    QPDFObjectHandle::StreamDataProvider(true),
    destination_qpdf(destination_qpdf)
{
}

bool
QPDF::CopiedStreamDataProvider::provideStreamData(
    int objid, int generation, Pipeline* pipeline,
    bool suppress_warnings, bool will_retry)
{
    PointerHolder<ForeignStreamData> foreign_data =
        this->foreign_stream_data[QPDFObjGen(objid, generation)];
    bool result = false;
    if (foreign_data.getPointer())
    {
        result = destination_qpdf.pipeForeignStreamData(
            foreign_data, pipeline, suppress_warnings, will_retry);
        QTC::TC("qpdf", "QPDF copy foreign with data",
                result ? 0 : 1);
    }
    else
    {
        QPDFObjectHandle foreign_stream =
            this->foreign_streams[QPDFObjGen(objid, generation)];
        result = foreign_stream.pipeStreamData(
            pipeline, nullptr, 0, qpdf_dl_none,
            suppress_warnings, will_retry);
        QTC::TC("qpdf", "QPDF copy foreign with foreign_stream",
                result ? 0 : 1);
    }
    return result;
}

void
QPDF::CopiedStreamDataProvider::registerForeignStream(
    QPDFObjGen const& local_og, QPDFObjectHandle foreign_stream)
{
    this->foreign_streams[local_og] = foreign_stream;
}

void
QPDF::CopiedStreamDataProvider::registerForeignStream(
    QPDFObjGen const& local_og,
    PointerHolder<ForeignStreamData> foreign_stream)
{
    this->foreign_stream_data[local_og] = foreign_stream;
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

QPDF::EncryptionParameters::EncryptionParameters() :
    encrypted(false),
    encryption_initialized(false),
    encryption_V(0),
    encryption_R(0),
    encrypt_metadata(true),
    cf_stream(e_none),
    cf_string(e_none),
    cf_file(e_none),
    cached_key_objid(0),
    cached_key_generation(0),
    user_password_matched(false),
    owner_password_matched(false)
{
}

QPDF::Members::Members() :
    unique_id(0),
    provided_password_is_hex_key(false),
    ignore_xref_streams(false),
    suppress_warnings(false),
    out_stream(&std::cout),
    err_stream(&std::cerr),
    attempt_recovery(true),
    encp(new EncryptionParameters),
    pushed_inherited_attributes_to_pages(false),
    copied_stream_data_provider(0),
    reconstructed_xref(false),
    fixed_dangling_refs(false),
    immediate_copy_from(false),
    in_parse(false),
    parsed(false),
    first_xref_item_offset(0),
    uncompressed_after_compressed(false)
{
}

QPDF::Members::~Members()
{
}

QPDF::QPDF() :
    m(new Members())
{
    m->tokenizer.allowEOF();
    // Generate a unique ID. It just has to be unique among all QPDF
    // objects allocated throughout the lifetime of this running
    // application.
    static std::atomic<unsigned long long> unique_id{0};
    m->unique_id = unique_id.fetch_add(1ULL);
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
    this->m->xref_table.clear();
    for (std::map<QPDFObjGen, ObjCache>::iterator iter =
             this->m->obj_cache.begin();
	 iter != this->m->obj_cache.end(); ++iter)
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
    this->m->file = source;
    parse(password);
}

void
QPDF::closeInputSource()
{
    this->m->file = 0;
}

void
QPDF::setPasswordIsHexKey(bool val)
{
    this->m->provided_password_is_hex_key = val;
}

void
QPDF::emptyPDF()
{
    processMemoryFile("empty PDF", EMPTY_PDF, strlen(EMPTY_PDF));
}

void
QPDF::setIgnoreXRefStreams(bool val)
{
    this->m->ignore_xref_streams = val;
}

void
QPDF::setOutputStreams(std::ostream* out, std::ostream* err)
{
    this->m->out_stream = out ? out : &std::cout;
    this->m->err_stream = err ? err : &std::cerr;
}

void
QPDF::setSuppressWarnings(bool val)
{
    this->m->suppress_warnings = val;
}

void
QPDF::setAttemptRecovery(bool val)
{
    this->m->attempt_recovery = val;
}

void
QPDF::setImmediateCopyFrom(bool val)
{
    this->m->immediate_copy_from = val;
}

std::vector<QPDFExc>
QPDF::getWarnings()
{
    std::vector<QPDFExc> result = this->m->warnings;
    this->m->warnings.clear();
    return result;
}

bool
QPDF::anyWarnings() const
{
    return ! this->m->warnings.empty();
}

bool
QPDF::findHeader()
{
    qpdf_offset_t global_offset = this->m->file->tell();
    std::string line = this->m->file->readLine(1024);
    char const* p = line.c_str();
    if (strncmp(p, "%PDF-", 5) != 0)
    {
        throw std::logic_error("findHeader is not looking at %PDF-");
    }
    p += 5;
    std::string version;
    // Note: The string returned by line.c_str() is always
    // null-terminated. The code below never overruns the buffer
    // because a null character always short-circuits further
    // advancement.
    bool valid = QUtil::is_digit(*p);
    if (valid)
    {
        while (QUtil::is_digit(*p))
        {
            version.append(1, *p++);
        }
        if ((*p == '.') && QUtil::is_digit(*(p+1)))
        {
            version.append(1, *p++);
            while (QUtil::is_digit(*p))
            {
                version.append(1, *p++);
            }
        }
        else
        {
            valid = false;
        }
    }
    if (valid)
    {
        this->m->pdf_version = version;
        if (global_offset != 0)
        {
            // Empirical evidence strongly suggests that when there is
            // leading material prior to the PDF header, all explicit
            // offsets in the file are such that 0 points to the
            // beginning of the header.
            QTC::TC("qpdf", "QPDF global offset");
            this->m->file = new OffsetInputSource(this->m->file, global_offset);
        }
    }
    return valid;
}

bool
QPDF::findStartxref()
{
    QPDFTokenizer::Token t = readToken(this->m->file);
    if (t == QPDFTokenizer::Token(QPDFTokenizer::tt_word, "startxref"))
    {
        t = readToken(this->m->file);
        if (t.getType() == QPDFTokenizer::tt_integer)
        {
            // Position in front of offset token
            this->m->file->seek(this->m->file->getLastOffset(), SEEK_SET);
            return true;
        }
    }
    return false;
}

void
QPDF::parse(char const* password)
{
    if (password)
    {
	this->m->encp->provided_password = password;
    }

    // Find the header anywhere in the first 1024 bytes of the file.
    PatternFinder hf(*this, &QPDF::findHeader);
    if (! this->m->file->findFirst("%PDF-", 0, 1024, hf))
    {
	QTC::TC("qpdf", "QPDF not a pdf file");
	warn(QPDFExc(qpdf_e_damaged_pdf, this->m->file->getName(),
                     "", 0, "can't find PDF header"));
        // QPDFWriter writes files that usually require at least
        // version 1.2 for /FlateDecode
        this->m->pdf_version = "1.2";
    }

    // PDF spec says %%EOF must be found within the last 1024 bytes of
    // the file.  We add an extra 30 characters to leave room for the
    // startxref stuff.
    this->m->file->seek(0, SEEK_END);
    qpdf_offset_t end_offset = this->m->file->tell();
    qpdf_offset_t start_offset = (end_offset > 1054 ? end_offset - 1054 : 0);
    PatternFinder sf(*this, &QPDF::findStartxref);
    qpdf_offset_t xref_offset = 0;
    if (this->m->file->findLast("startxref", start_offset, 0, sf))
    {
        xref_offset = QUtil::string_to_ll(
            readToken(this->m->file).getValue().c_str());
    }

    try
    {
	if (xref_offset == 0)
	{
	    QTC::TC("qpdf", "QPDF can't find startxref");
	    throw QPDFExc(qpdf_e_damaged_pdf, this->m->file->getName(), "", 0,
			  "can't find startxref");
	}
	read_xref(xref_offset);
    }
    catch (QPDFExc& e)
    {
	if (this->m->attempt_recovery)
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
    this->m->parsed = true;
}

void
QPDF::inParse(bool v)
{
    if (this->m->in_parse == v)
    {
        // This happens of QPDFObjectHandle::parseInternal tries to
        // resolve an indirect object while it is parsing.
        throw std::logic_error(
            "QPDF: re-entrant parsing detected. This is a qpdf bug."
            " Please report at https://github.com/qpdf/qpdf/issues.");
    }
    this->m->in_parse = v;
}

void
QPDF::warn(QPDFExc const& e)
{
    this->m->warnings.push_back(e);
    if (! this->m->suppress_warnings)
    {
	*this->m->err_stream
            << "WARNING: "
            << this->m->warnings.back().what() << std::endl;
    }
}

void
QPDF::setTrailer(QPDFObjectHandle obj)
{
    if (this->m->trailer.isInitialized())
    {
	return;
    }
    this->m->trailer = obj;
}

void
QPDF::reconstruct_xref(QPDFExc& e)
{
    if (this->m->reconstructed_xref)
    {
        // Avoid xref reconstruction infinite loops. This is getting
        // very hard to reproduce because qpdf is throwing many fewer
        // exceptions while parsing. Most situations are warnings now.
        throw e;
    }

    this->m->reconstructed_xref = true;

    warn(QPDFExc(qpdf_e_damaged_pdf, this->m->file->getName(), "", 0,
		 "file is damaged"));
    warn(e);
    warn(QPDFExc(qpdf_e_damaged_pdf, this->m->file->getName(), "", 0,
		 "Attempting to reconstruct cross-reference table"));

    // Delete all references to type 1 (uncompressed) objects
    std::set<QPDFObjGen> to_delete;
    for (std::map<QPDFObjGen, QPDFXRefEntry>::iterator iter =
	     this->m->xref_table.begin();
	 iter != this->m->xref_table.end(); ++iter)
    {
	if (((*iter).second).getType() == 1)
	{
	    to_delete.insert((*iter).first);
	}
    }
    for (std::set<QPDFObjGen>::iterator iter = to_delete.begin();
	 iter != to_delete.end(); ++iter)
    {
	this->m->xref_table.erase(*iter);
    }

    this->m->file->seek(0, SEEK_END);
    qpdf_offset_t eof = this->m->file->tell();
    this->m->file->seek(0, SEEK_SET);
    bool in_obj = false;
    qpdf_offset_t line_start = 0;
    // Don't allow very long tokens here during recovery.
    static size_t const MAX_LEN = 100;
    while (this->m->file->tell() < eof)
    {
        this->m->file->findAndSkipNextEOL();
        qpdf_offset_t next_line_start = this->m->file->tell();
        this->m->file->seek(line_start, SEEK_SET);
        QPDFTokenizer::Token t1 = readToken(this->m->file, MAX_LEN);
        qpdf_offset_t token_start =
            this->m->file->tell() - toO(t1.getValue().length());
        if (token_start >= next_line_start)
        {
            // don't process yet
        }
	else if (in_obj)
	{
            if (t1 == QPDFTokenizer::Token(QPDFTokenizer::tt_word, "endobj"))
	    {
		in_obj = false;
	    }
	}
        else
        {
            if (t1.getType() == QPDFTokenizer::tt_integer)
            {
                QPDFTokenizer::Token t2 =
                    readToken(this->m->file, MAX_LEN);
                QPDFTokenizer::Token t3 =
                    readToken(this->m->file, MAX_LEN);
                if ((t2.getType() == QPDFTokenizer::tt_integer) &&
                    (t3 == QPDFTokenizer::Token(QPDFTokenizer::tt_word, "obj")))
                {
                    in_obj = true;
                    int obj = QUtil::string_to_int(t1.getValue().c_str());
                    int gen = QUtil::string_to_int(t2.getValue().c_str());
                    insertXrefEntry(obj, 1, token_start, gen, true);
                }
            }
            else if ((! this->m->trailer.isInitialized()) &&
                     (t1 == QPDFTokenizer::Token(
                         QPDFTokenizer::tt_word, "trailer")))
            {
                QPDFObjectHandle t =
                    readObject(this->m->file, "trailer", 0, 0, false);
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
        this->m->file->seek(next_line_start, SEEK_SET);
        line_start = next_line_start;
    }

    if (! this->m->trailer.isInitialized())
    {
	// We could check the last encountered object to see if it was
	// an xref stream.  If so, we could try to get the trailer
	// from there.  This may make it possible to recover files
	// with bad startxref pointers even when they have object
	// streams.

	throw QPDFExc(qpdf_e_damaged_pdf, this->m->file->getName(), "", 0,
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
    std::set<qpdf_offset_t> visited;
    while (xref_offset)
    {
        visited.insert(xref_offset);
        char buf[7];
        memset(buf, 0, sizeof(buf));
	this->m->file->seek(xref_offset, SEEK_SET);
        // Some files miss the mark a little with startxref. We could
        // do a better job of searching in the neighborhood for
        // something that looks like either an xref table or stream,
        // but the simple heuristic of skipping whitespace can help
        // with the xref table case and is harmless with the stream
        // case.
        bool done = false;
        bool skipped_space = false;
        while (! done)
        {
            char ch;
            if (1 == this->m->file->read(&ch, 1))
            {
                if (QUtil::is_space(ch))
                {
                    skipped_space = true;
                }
                else
                {
                    this->m->file->unreadCh(ch);
                    done = true;
                }
            }
            else
            {
                QTC::TC("qpdf", "QPDF eof skipping spaces before xref",
                        skipped_space ? 0 : 1);
                done = true;
            }
        }

	this->m->file->read(buf, sizeof(buf) - 1);
        // The PDF spec says xref must be followed by a line
        // terminator, but files exist in the wild where it is
        // terminated by arbitrary whitespace.
        if ((strncmp(buf, "xref", 4) == 0) &&
            QUtil::is_space(buf[4]))
	{
            if (skipped_space)
            {
                QTC::TC("qpdf", "QPDF xref skipped space");
                warn(QPDFExc(qpdf_e_damaged_pdf, this->m->file->getName(),
                             "", 0,
                             "extraneous whitespace seen before xref"));
            }
            QTC::TC("qpdf", "QPDF xref space",
                    ((buf[4] == '\n') ? 0 :
                     (buf[4] == '\r') ? 1 :
                     (buf[4] == ' ') ? 2 : 9999));
            int skip = 4;
            // buf is null-terminated, and QUtil::is_space('\0') is
            // false, so this won't overrun.
            while (QUtil::is_space(buf[skip]))
            {
                ++skip;
            }
            xref_offset = read_xrefTable(xref_offset + skip);
	}
	else
	{
	    xref_offset = read_xrefStream(xref_offset);
	}
        if (visited.count(xref_offset) != 0)
        {
            QTC::TC("qpdf", "QPDF xref loop");
            throw QPDFExc(qpdf_e_damaged_pdf, this->m->file->getName(), "", 0,
                          "loop detected following xref tables");
        }
    }

    if (! this->m->trailer.isInitialized())
    {
        throw QPDFExc(qpdf_e_damaged_pdf, this->m->file->getName(), "", 0,
                      "unable to find trailer while reading xref");
    }
    int size = this->m->trailer.getKey("/Size").getIntValueAsInt();
    int max_obj = 0;
    if (! this->m->xref_table.empty())
    {
	max_obj = (*(this->m->xref_table.rbegin())).first.getObj();
    }
    if (! this->m->deleted_objects.empty())
    {
	max_obj = std::max(max_obj, *(this->m->deleted_objects.rbegin()));
    }
    if ((size < 1) || (size - 1 != max_obj))
    {
	QTC::TC("qpdf", "QPDF xref size mismatch");
	warn(QPDFExc(qpdf_e_damaged_pdf, this->m->file->getName(), "", 0,
		     std::string("reported number of objects (") +
		     QUtil::int_to_string(size) +
		     ") is not one plus the highest object number (" +
		     QUtil::int_to_string(max_obj) + ")"));
    }

    // We no longer need the deleted_objects table, so go ahead and
    // clear it out to make sure we never depend on its being set.
    this->m->deleted_objects.clear();
}

bool
QPDF::parse_xrefFirst(std::string const& line,
                      int& obj, int& num, int& bytes)
{
    // is_space and is_digit both return false on '\0', so this will
    // not overrun the null-terminated buffer.
    char const* p = line.c_str();
    char const* start = line.c_str();

    // Skip zero or more spaces
    while (QUtil::is_space(*p))
    {
        ++p;
    }
    // Require digit
    if (! QUtil::is_digit(*p))
    {
        return false;
    }
    // Gather digits
    std::string obj_str;
    while (QUtil::is_digit(*p))
    {
        obj_str.append(1, *p++);
    }
    // Require space
    if (! QUtil::is_space(*p))
    {
        return false;
    }
    // Skip spaces
    while (QUtil::is_space(*p))
    {
        ++p;
    }
    // Require digit
    if (! QUtil::is_digit(*p))
    {
        return false;
    }
    // Gather digits
    std::string num_str;
    while (QUtil::is_digit(*p))
    {
        num_str.append(1, *p++);
    }
    // Skip any space including line terminators
    while (QUtil::is_space(*p))
    {
        ++p;
    }
    bytes = toI(p - start);
    obj = QUtil::string_to_int(obj_str.c_str());
    num = QUtil::string_to_int(num_str.c_str());
    return true;
}

bool
QPDF::parse_xrefEntry(std::string const& line,
                      qpdf_offset_t& f1, int& f2, char& type)
{
    // is_space and is_digit both return false on '\0', so this will
    // not overrun the null-terminated buffer.
    char const* p = line.c_str();

    // Skip zero or more spaces. There aren't supposed to be any.
    bool invalid = false;
    while (QUtil::is_space(*p))
    {
        ++p;
        QTC::TC("qpdf", "QPDF ignore first space in xref entry");
        invalid = true;
    }
    // Require digit
    if (! QUtil::is_digit(*p))
    {
        return false;
    }
    // Gather digits
    std::string f1_str;
    while (QUtil::is_digit(*p))
    {
        f1_str.append(1, *p++);
    }
    // Require space
    if (! QUtil::is_space(*p))
    {
        return false;
    }
    if (QUtil::is_space(*(p+1)))
    {
        QTC::TC("qpdf", "QPDF ignore first extra space in xref entry");
        invalid = true;
    }
    // Skip spaces
    while (QUtil::is_space(*p))
    {
        ++p;
    }
    // Require digit
    if (! QUtil::is_digit(*p))
    {
        return false;
    }
    // Gather digits
    std::string f2_str;
    while (QUtil::is_digit(*p))
    {
        f2_str.append(1, *p++);
    }
    // Require space
    if (! QUtil::is_space(*p))
    {
        return false;
    }
    if (QUtil::is_space(*(p+1)))
    {
        QTC::TC("qpdf", "QPDF ignore second extra space in xref entry");
        invalid = true;
    }
    // Skip spaces
    while (QUtil::is_space(*p))
    {
        ++p;
    }
    if ((*p == 'f') || (*p == 'n'))
    {
        type = *p;
    }
    else
    {
        return false;
    }
    if ((f1_str.length() != 10) || (f2_str.length() != 5))
    {
        QTC::TC("qpdf", "QPDF ignore length error xref entry");
        invalid = true;
    }

    if (invalid)
    {
        warn(QPDFExc(qpdf_e_damaged_pdf, this->m->file->getName(),
                     "xref table",
                     this->m->file->getLastOffset(),
                     "accepting invalid xref table entry"));
    }

    f1 = QUtil::string_to_ll(f1_str.c_str());
    f2 = QUtil::string_to_int(f2_str.c_str());

    return true;
}

qpdf_offset_t
QPDF::read_xrefTable(qpdf_offset_t xref_offset)
{
    std::vector<QPDFObjGen> deleted_items;

    this->m->file->seek(xref_offset, SEEK_SET);
    bool done = false;
    while (! done)
    {
        char linebuf[51];
        memset(linebuf, 0, sizeof(linebuf));
        this->m->file->read(linebuf, sizeof(linebuf) - 1);
	std::string line = linebuf;
        int obj = 0;
        int num = 0;
        int bytes = 0;
        if (! parse_xrefFirst(line, obj, num, bytes))
	{
	    QTC::TC("qpdf", "QPDF invalid xref");
	    throw QPDFExc(qpdf_e_damaged_pdf, this->m->file->getName(),
			  "xref table", this->m->file->getLastOffset(),
			  "xref syntax invalid");
	}
        this->m->file->seek(this->m->file->getLastOffset() + bytes, SEEK_SET);
	for (qpdf_offset_t i = obj; i - num < obj; ++i)
	{
	    if (i == 0)
	    {
		// This is needed by checkLinearization()
		this->m->first_xref_item_offset = this->m->file->tell();
	    }
	    std::string xref_entry = this->m->file->readLine(30);
            // For xref_table, these will always be small enough to be ints
	    qpdf_offset_t f1 = 0;
	    int f2 = 0;
	    char type = '\0';
            if (! parse_xrefEntry(xref_entry, f1, f2, type))
	    {
		QTC::TC("qpdf", "QPDF invalid xref entry");
		throw QPDFExc(
		    qpdf_e_damaged_pdf, this->m->file->getName(),
		    "xref table", this->m->file->getLastOffset(),
		    "invalid xref entry (obj=" +
		    QUtil::int_to_string(i) + ")");
	    }
	    if (type == 'f')
	    {
		// Save deleted items until after we've checked the
		// XRefStm, if any.
		deleted_items.push_back(QPDFObjGen(toI(i), f2));
	    }
	    else
	    {
		insertXrefEntry(toI(i), 1, f1, f2);
	    }
	}
	qpdf_offset_t pos = this->m->file->tell();
	QPDFTokenizer::Token t = readToken(this->m->file);
	if (t == QPDFTokenizer::Token(QPDFTokenizer::tt_word, "trailer"))
	{
	    done = true;
	}
	else
	{
	    this->m->file->seek(pos, SEEK_SET);
	}
    }

    // Set offset to previous xref table if any
    QPDFObjectHandle cur_trailer =
	readObject(this->m->file, "trailer", 0, 0, false);
    if (! cur_trailer.isDictionary())
    {
	QTC::TC("qpdf", "QPDF missing trailer");
	throw QPDFExc(qpdf_e_damaged_pdf, this->m->file->getName(),
		      "", this->m->file->getLastOffset(),
		      "expected trailer dictionary");
    }

    if (! this->m->trailer.isInitialized())
    {
	setTrailer(cur_trailer);

	if (! this->m->trailer.hasKey("/Size"))
	{
	    QTC::TC("qpdf", "QPDF trailer lacks size");
	    throw QPDFExc(qpdf_e_damaged_pdf, this->m->file->getName(),
			  "trailer", this->m->file->getLastOffset(),
			  "trailer dictionary lacks /Size key");
	}
	if (! this->m->trailer.getKey("/Size").isInteger())
	{
	    QTC::TC("qpdf", "QPDF trailer size not integer");
	    throw QPDFExc(qpdf_e_damaged_pdf, this->m->file->getName(),
			  "trailer", this->m->file->getLastOffset(),
			  "/Size key in trailer dictionary is not "
			  "an integer");
	}
    }

    if (cur_trailer.hasKey("/XRefStm"))
    {
	if (this->m->ignore_xref_streams)
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
		throw QPDFExc(qpdf_e_damaged_pdf, this->m->file->getName(),
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
	    throw QPDFExc(qpdf_e_damaged_pdf, this->m->file->getName(),
			  "trailer", this->m->file->getLastOffset(),
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
    if (! this->m->ignore_xref_streams)
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
	throw QPDFExc(qpdf_e_damaged_pdf, this->m->file->getName(),
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
	throw QPDFExc(qpdf_e_damaged_pdf, this->m->file->getName(),
		      "xref stream", xref_offset,
		      "Cross-reference stream does not have"
		      " proper /W and /Index keys");
    }

    int W[3];
    size_t entry_size = 0;
    int max_bytes = sizeof(qpdf_offset_t);
    for (int i = 0; i < 3; ++i)
    {
	W[i] = W_obj.getArrayItem(i).getIntValueAsInt();
        if (W[i] > max_bytes)
        {
            throw QPDFExc(qpdf_e_damaged_pdf, this->m->file->getName(),
                          "xref stream", xref_offset,
                          "Cross-reference stream's /W contains"
                          " impossibly large values");
        }
	entry_size += toS(W[i]);
    }
    if (entry_size == 0)
    {
        throw QPDFExc(qpdf_e_damaged_pdf, this->m->file->getName(),
                      "xref stream", xref_offset,
                      "Cross-reference stream's /W indicates"
                      " entry size of 0");
    }
    unsigned long long max_num_entries =
        static_cast<unsigned long long>(-1) / entry_size;

    std::vector<long long> indx;
    if (Index_obj.isArray())
    {
	int n_index = Index_obj.getArrayNItems();
	if ((n_index % 2) || (n_index < 2))
	{
	    throw QPDFExc(qpdf_e_damaged_pdf, this->m->file->getName(),
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
		throw QPDFExc(qpdf_e_damaged_pdf, this->m->file->getName(),
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

    size_t num_entries = 0;
    for (size_t i = 1; i < indx.size(); i += 2)
    {
        if (indx.at(i) > QIntC::to_longlong(max_num_entries - num_entries))
        {
            throw QPDFExc(qpdf_e_damaged_pdf, this->m->file->getName(),
                          "xref stream", xref_offset,
                          "Cross-reference stream claims to contain"
                          " too many entries: " +
                          QUtil::int_to_string(indx.at(i)) + " " +
                          QUtil::uint_to_string(max_num_entries) + " " +
                          QUtil::uint_to_string(num_entries));
        }
	num_entries += toS(indx.at(i));
    }

    // entry_size and num_entries have both been validated to ensure
    // that this multiplication does not cause an overflow.
    size_t expected_size = entry_size * num_entries;

    PointerHolder<Buffer> bp = xref_obj.getStreamData(qpdf_dl_specialized);
    size_t actual_size = bp->getSize();

    if (expected_size != actual_size)
    {
	QPDFExc x(qpdf_e_damaged_pdf, this->m->file->getName(),
		  "xref stream", xref_offset,
		  "Cross-reference stream data has the wrong size;"
		  " expected = " + QUtil::uint_to_string(expected_size) +
		  "; actual = " + QUtil::uint_to_string(actual_size));
	if (expected_size > actual_size)
	{
	    throw x;
	}
	else
	{
	    warn(x);
	}
    }

    size_t cur_chunk = 0;
    int chunk_count = 0;

    bool saw_first_compressed_object = false;

    // Actual size vs. expected size check above ensures that we will
    // not overflow any buffers here.  We know that entry_size *
    // num_entries is equal to the size of the buffer.
    unsigned char const* data = bp->getBuffer();
    for (size_t i = 0; i < num_entries; ++i)
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
		fields[j] += toI(*p++);
	    }
	}

	// Get the object and generation number.  The object number is
	// based on /Index.  The generation number is 0 unless this is
	// an uncompressed object record, in which case the generation
	// number appears as the third field.
	int obj = toI(indx.at(cur_chunk));
        if ((obj < 0) ||
            ((std::numeric_limits<int>::max() - obj) < chunk_count))
        {
            std::ostringstream msg;
            msg << "adding " << chunk_count << " to " << obj
                << " while computing index in xref stream would cause"
                << " an integer overflow";
            throw std::range_error(msg.str());
        }
        obj += chunk_count;
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
		this->m->uncompressed_after_compressed = true;
	    }
	}
	else if (fields[0] == 2)
	{
	    saw_first_compressed_object = true;
	}
	if (obj == 0)
	{
	    // This is needed by checkLinearization()
	    this->m->first_xref_item_offset = xref_offset;
	}
	insertXrefEntry(obj, toI(fields[0]),
                        fields[1], toI(fields[2]));
    }

    if (! this->m->trailer.isInitialized())
    {
	setTrailer(dict);
    }

    if (dict.hasKey("/Prev"))
    {
	if (! dict.getKey("/Prev").isInteger())
	{
	    throw QPDFExc(qpdf_e_damaged_pdf, this->m->file->getName(),
			  "xref stream", this->m->file->getLastOffset(),
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
	if (this->m->xref_table.count(og))
	{
	    if (overwrite)
	    {
		QTC::TC("qpdf", "QPDF xref overwrite object");
		this->m->xref_table.erase(og);
	    }
	    else
	    {
		QTC::TC("qpdf", "QPDF xref reused object");
		return;
	    }
	}
	if (this->m->deleted_objects.count(obj))
	{
	    QTC::TC("qpdf", "QPDF xref deleted object");
	    return;
	}
    }

    switch (f0)
    {
      case 0:
	this->m->deleted_objects.insert(obj);
	break;

      case 1:
	// f2 is generation
	QTC::TC("qpdf", "QPDF xref gen > 0", ((f2 > 0) ? 1 : 0));
	this->m->xref_table[QPDFObjGen(obj, f2)] = QPDFXRefEntry(f0, f1, f2);
	break;

      case 2:
	this->m->xref_table[QPDFObjGen(obj, 0)] = QPDFXRefEntry(f0, f1, f2);
	break;

      default:
	throw QPDFExc(qpdf_e_damaged_pdf, this->m->file->getName(),
		      "xref stream", this->m->file->getLastOffset(),
		      "unknown xref stream entry type " +
		      QUtil::int_to_string(f0));
	break;
    }
}

void
QPDF::showXRefTable()
{
    for (std::map<QPDFObjGen, QPDFXRefEntry>::iterator iter =
	     this->m->xref_table.begin();
	 iter != this->m->xref_table.end(); ++iter)
    {
	QPDFObjGen const& og = (*iter).first;
	QPDFXRefEntry const& entry = (*iter).second;
	*this->m->out_stream << og.getObj() << "/" << og.getGen() << ": ";
	switch (entry.getType())
	{
	  case 1:
	    *this->m->out_stream
                << "uncompressed; offset = " << entry.getOffset();
	    break;

	  case 2:
	    *this->m->out_stream
                << "compressed; stream = "
                << entry.getObjStreamNumber()
                << ", index = " << entry.getObjStreamIndex();
	    break;

	  default:
	    throw std::logic_error("unknown cross-reference table type while"
				   " showing xref_table");
	    break;
	}
	*this->m->out_stream << std::endl;
    }
}

void
QPDF::fixDanglingReferences(bool force)
{
    if (this->m->fixed_dangling_refs && (! force))
    {
        return;
    }
    this->m->fixed_dangling_refs = true;

    // Create a set of all known indirect objects including those
    // we've previously resolved and those that we have created.
    std::set<QPDFObjGen> to_process;
    for (std::map<QPDFObjGen, ObjCache>::iterator iter =
	     this->m->obj_cache.begin();
	 iter != this->m->obj_cache.end(); ++iter)
    {
	to_process.insert((*iter).first);
    }
    for (std::map<QPDFObjGen, QPDFXRefEntry>::iterator iter =
	     this->m->xref_table.begin();
	 iter != this->m->xref_table.end(); ++iter)
    {
	to_process.insert((*iter).first);
    }

    // For each non-scalar item to process, put it in the queue.
    std::list<QPDFObjectHandle> queue;
    queue.push_back(this->m->trailer);
    for (std::set<QPDFObjGen>::iterator iter = to_process.begin();
         iter != to_process.end(); ++iter)
    {
        QPDFObjectHandle obj = QPDFObjectHandle::Factory::newIndirect(
            this, (*iter).getObj(), (*iter).getGen());
        if (obj.isDictionary() || obj.isArray())
        {
            queue.push_back(obj);
        }
        else if (obj.isStream())
        {
            queue.push_back(obj.getDict());
        }
    }

    // Process the queue by recursively resolving all object
    // references. We don't need to do loop detection because we don't
    // traverse known indirect objects when processing the queue.
    while (! queue.empty())
    {
        QPDFObjectHandle obj = queue.front();
        queue.pop_front();
        std::list<QPDFObjectHandle> to_check;
        if (obj.isDictionary())
        {
            std::map<std::string, QPDFObjectHandle> members =
                obj.getDictAsMap();
            for (std::map<std::string, QPDFObjectHandle>::iterator iter =
                     members.begin();
                 iter != members.end(); ++iter)
            {
                to_check.push_back((*iter).second);
            }
        }
        else if (obj.isArray())
        {
            QPDF_Array* arr =
                dynamic_cast<QPDF_Array*>(
                    QPDFObjectHandle::ObjAccessor::getObject(obj).getPointer());
            arr->addExplicitElementsToList(to_check);
        }
        for (std::list<QPDFObjectHandle>::iterator iter = to_check.begin();
             iter != to_check.end(); ++iter)
        {
            QPDFObjectHandle sub = *iter;
            if (sub.isIndirect())
            {
                if (sub.getOwningQPDF() == this)
                {
                    QPDFObjGen og(sub.getObjGen());
                    if (this->m->obj_cache.count(og) == 0)
                    {
                        QTC::TC("qpdf", "QPDF detected dangling ref");
                        queue.push_back(sub);
                    }
                }
            }
            else
            {
                queue.push_back(sub);
            }
        }

    }
}

size_t
QPDF::getObjectCount()
{
    // This method returns the next available indirect object number.
    // makeIndirectObject uses it for this purpose. After
    // fixDanglingReferences is called, all objects in the xref table
    // will also be in obj_cache.
    fixDanglingReferences();
    QPDFObjGen og(0, 0);
    if (! this->m->obj_cache.empty())
    {
	og = (*(this->m->obj_cache.rbegin())).first;
    }
    return toS(og.getObj());
}

std::vector<QPDFObjectHandle>
QPDF::getAllObjects()
{
    // After fixDanglingReferences is called, all objects are in the
    // object cache.
    fixDanglingReferences(true);
    std::vector<QPDFObjectHandle> result;
    for (std::map<QPDFObjGen, ObjCache>::iterator iter =
	     this->m->obj_cache.begin();
	 iter != this->m->obj_cache.end(); ++iter)
    {

	QPDFObjGen const& og = (*iter).first;
        result.push_back(QPDFObjectHandle::Factory::newIndirect(
                             this, og.getObj(), og.getGen()));
    }
    return result;
}

void
QPDF::setLastObjectDescription(std::string const& description,
			       int objid, int generation)
{
    this->m->last_object_description.clear();
    if (! description.empty())
    {
	this->m->last_object_description += description;
	if (objid > 0)
	{
	    this->m->last_object_description += ": ";
	}
    }
    if (objid > 0)
    {
	this->m->last_object_description += "object " +
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
    if (this->m->encp->encrypted && (! in_object_stream))
    {
        decrypter_ph = new StringDecrypter(this, objid, generation);
        decrypter = decrypter_ph.getPointer();
    }
    QPDFObjectHandle object = QPDFObjectHandle::parse(
        input, this->m->last_object_description,
        this->m->tokenizer, empty, decrypter, this);
    if (empty)
    {
        // Nothing in the PDF spec appears to allow empty objects, but
        // they have been encountered in actual PDF files and Adobe
        // Reader appears to ignore them.
        warn(QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                     this->m->last_object_description,
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
            // what we do here. We have also seen files that have
            // extraneous whitespace between the stream keyword and
            // the newline.
            bool done = false;
            while (! done)
            {
                done = true;
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
                                     this->m->last_object_description,
                                     input->tell(),
                                     "stream keyword followed"
                                     " by carriage return only"));
                        }
                    }
                }
                else if (QUtil::is_space(ch))
                {
                    warn(QPDFExc(
                             qpdf_e_damaged_pdf,
                             input->getName(),
                             this->m->last_object_description,
                             input->tell(),
                             "stream keyword followed by"
                             " extraneous whitespace"));
                    done = false;
                }
                else
                {
                    QTC::TC("qpdf", "QPDF stream without newline");
                    input->unreadCh(ch);
                    warn(QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                                 this->m->last_object_description,
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
                                  this->m->last_object_description, offset,
                                  "stream dictionary lacks /Length key");
                }

                QPDFObjectHandle length_obj = dict["/Length"];
                if (! length_obj.isInteger())
                {
                    QTC::TC("qpdf", "QPDF stream length not integer");
                    throw QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                                  this->m->last_object_description, offset,
                                  "/Length key in stream dictionary is not "
                                  "an integer");
                }

                length = toS(length_obj.getUIntValue());
                // Seek in two steps to avoid potential integer overflow
                input->seek(stream_offset, SEEK_SET);
                input->seek(toO(length), SEEK_CUR);
                if (! (readToken(input) ==
                       QPDFTokenizer::Token(
                           QPDFTokenizer::tt_word, "endstream")))
                {
                    QTC::TC("qpdf", "QPDF missing endstream");
                    throw QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                                  this->m->last_object_description,
                                  input->getLastOffset(),
                                  "expected endstream");
                }
            }
            catch (QPDFExc& e)
            {
                if (this->m->attempt_recovery)
                {
                    warn(e);
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

bool
QPDF::findEndstream()
{
    // Find endstream or endobj. Position the input at that token.
    QPDFTokenizer::Token t = readToken(this->m->file, 20);
    if ((t.getType() == QPDFTokenizer::tt_word) &&
        ((t.getValue() == "endobj") ||
         (t.getValue() == "endstream")))
    {
        this->m->file->seek(this->m->file->getLastOffset(), SEEK_SET);
        return true;
    }
    return false;
}

size_t
QPDF::recoverStreamLength(PointerHolder<InputSource> input,
			  int objid, int generation,
                          qpdf_offset_t stream_offset)
{
    // Try to reconstruct stream length by looking for
    // endstream or endobj
    warn(QPDFExc(qpdf_e_damaged_pdf, input->getName(),
		 this->m->last_object_description, stream_offset,
		 "attempting to recover stream length"));

    PatternFinder ef(*this, &QPDF::findEndstream);
    size_t length = 0;
    if (this->m->file->findFirst("end", stream_offset, 0, ef))
    {
        length = toS(this->m->file->tell() - stream_offset);
        // Reread endstream but, if it was endobj, don't skip that.
        QPDFTokenizer::Token t = readToken(this->m->file);
        if (t.getValue() == "endobj")
        {
            this->m->file->seek(this->m->file->getLastOffset(), SEEK_SET);
        }
    }

    if (length)
    {
	qpdf_offset_t this_obj_offset = 0;
	QPDFObjGen this_obj(0, 0);

	// Make sure this is inside this object
	for (std::map<QPDFObjGen, QPDFXRefEntry>::iterator iter =
		 this->m->xref_table.begin();
	     iter != this->m->xref_table.end(); ++iter)
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
        warn(QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                     this->m->last_object_description, stream_offset,
                     "unable to recover stream data;"
                     " treating stream as empty"));
    }
    else
    {
        warn(QPDFExc(qpdf_e_damaged_pdf, input->getName(),
                     this->m->last_object_description, stream_offset,
                     "recovered stream length: " +
                     QUtil::uint_to_string(length)));
    }

    QTC::TC("qpdf", "QPDF recovered stream length");
    return length;
}

QPDFTokenizer::Token
QPDF::readToken(PointerHolder<InputSource> input, size_t max_len)
{
    return this->m->tokenizer.readToken(
        input, this->m->last_object_description, true, max_len);
}

QPDFObjectHandle
QPDF::readObjectAtOffset(bool try_recovery,
			 qpdf_offset_t offset, std::string const& description,
			 int exp_objid, int exp_generation,
			 int& objid, int& generation)
{
    if (! this->m->attempt_recovery)
    {
        try_recovery = false;
    }
    setLastObjectDescription(description, exp_objid, exp_generation);

    // Special case: if offset is 0, just return null.  Some PDF
    // writers, in particular "Mac OS X 10.7.5 Quartz PDFContext", may
    // store deleted objects in the xref table as "0000000000 00000
    // n", which is not correct, but it won't hurt anything for to
    // ignore these.
    if (offset == 0)
    {
        QTC::TC("qpdf", "QPDF bogus 0 offset", 0);
	warn(QPDFExc(qpdf_e_damaged_pdf, this->m->file->getName(),
		     this->m->last_object_description, 0,
		     "object has offset 0"));
        return QPDFObjectHandle::newNull();
    }

    this->m->file->seek(offset, SEEK_SET);

    QPDFTokenizer::Token tobjid = readToken(this->m->file);
    QPDFTokenizer::Token tgen = readToken(this->m->file);
    QPDFTokenizer::Token tobj = readToken(this->m->file);

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
	    throw QPDFExc(qpdf_e_damaged_pdf, this->m->file->getName(),
			  this->m->last_object_description, offset,
			  "expected n n obj");
	}
	objid = QUtil::string_to_int(tobjid.getValue().c_str());
	generation = QUtil::string_to_int(tgen.getValue().c_str());

        if (objid == 0)
        {
            QTC::TC("qpdf", "QPDF object id 0");
            throw QPDFExc(qpdf_e_damaged_pdf, this->m->file->getName(),
                          this->m->last_object_description, offset,
                          "object with ID 0");
        }

	if ((exp_objid >= 0) &&
	    (! ((objid == exp_objid) && (generation == exp_generation))))
	{
	    QTC::TC("qpdf", "QPDF err wrong objid/generation");
	    QPDFExc e(qpdf_e_damaged_pdf, this->m->file->getName(),
                      this->m->last_object_description, offset,
                      std::string("expected ") +
                      QUtil::int_to_string(exp_objid) + " " +
                      QUtil::int_to_string(exp_generation) + " obj");
            if (try_recovery)
            {
                // Will be retried below
                throw e;
            }
            else
            {
                // We can try reading the object anyway even if the ID
                // doesn't match.
                warn(e);
            }
	}
    }
    catch (QPDFExc& e)
    {
	if ((exp_objid >= 0) && try_recovery)
	{
	    // Try again after reconstructing xref table
	    reconstruct_xref(e);
	    QPDFObjGen og(exp_objid, exp_generation);
	    if (this->m->xref_table.count(og) &&
		(this->m->xref_table[og].getType() == 1))
	    {
		qpdf_offset_t new_offset = this->m->xref_table[og].getOffset();
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
			 qpdf_e_damaged_pdf, this->m->file->getName(),
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
	this->m->file, description, objid, generation, false);

    if (! (readToken(this->m->file) ==
	   QPDFTokenizer::Token(QPDFTokenizer::tt_word, "endobj")))
    {
	QTC::TC("qpdf", "QPDF err expected endobj");
	warn(QPDFExc(qpdf_e_damaged_pdf, this->m->file->getName(),
		     this->m->last_object_description,
                     this->m->file->getLastOffset(),
		     "expected endobj"));
    }

    QPDFObjGen og(objid, generation);
    if (! this->m->obj_cache.count(og))
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
	qpdf_offset_t end_before_space = this->m->file->tell();

	// skip over spaces
	while (true)
	{
	    char ch;
	    if (this->m->file->read(&ch, 1))
	    {
		if (! isspace(static_cast<unsigned char>(ch)))
		{
		    this->m->file->seek(-1, SEEK_CUR);
		    break;
		}
	    }
	    else
	    {
		throw QPDFExc(qpdf_e_damaged_pdf, this->m->file->getName(),
			      this->m->last_object_description,
                              this->m->file->tell(),
			      "EOF after endobj");
	    }
	}
	qpdf_offset_t end_after_space = this->m->file->tell();

	this->m->obj_cache[og] =
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
    if (this->m->resolving.count(og))
    {
        // This can happen if an object references itself directly or
        // indirectly in some key that has to be resolved during
        // object parsing, such as stream length.
	QTC::TC("qpdf", "QPDF recursion loop in resolve");
	warn(QPDFExc(qpdf_e_damaged_pdf, this->m->file->getName(),
		     "", this->m->file->getLastOffset(),
		     "loop detected resolving object " +
		     QUtil::int_to_string(objid) + " " +
		     QUtil::int_to_string(generation)));
        return new QPDF_Null;
    }
    ResolveRecorder rr(this, og);

    if ((! this->m->obj_cache.count(og)) && this->m->xref_table.count(og))
    {
	QPDFXRefEntry const& entry = this->m->xref_table[og];
        try
        {
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
                throw QPDFExc(qpdf_e_damaged_pdf,
                              this->m->file->getName(), "", 0,
                              "object " +
                              QUtil::int_to_string(objid) + "/" +
                              QUtil::int_to_string(generation) +
                              " has unexpected xref entry type");
            }
        }
        catch (QPDFExc& e)
        {
            warn(e);
        }
        catch (std::exception& e)
        {
            warn(QPDFExc(qpdf_e_damaged_pdf, this->m->file->getName(), "", 0,
                         "object " +
                         QUtil::int_to_string(objid) + "/" +
                         QUtil::int_to_string(generation) +
                         ": error reading object: " + e.what()));
        }
    }
    if (this->m->obj_cache.count(og) == 0)
    {
        // PDF spec says unknown objects resolve to the null object.
        QTC::TC("qpdf", "QPDF resolve failure to null");
        QPDFObjectHandle oh = QPDFObjectHandle::newNull();
        this->m->obj_cache[og] =
            ObjCache(QPDFObjectHandle::ObjAccessor::getObject(oh), -1, -1);
    }

    PointerHolder<QPDFObject> result(this->m->obj_cache[og].object);
    if (! result->hasDescription())
    {
        result->setDescription(
            this,
            "object " + QUtil::int_to_string(objid) + " " +
            QUtil::int_to_string(generation));
    }
    return result;
}

void
QPDF::resolveObjectsInStream(int obj_stream_number)
{
    // Force resolution of object stream
    QPDFObjectHandle obj_stream = getObjectByID(obj_stream_number, 0);
    if (! obj_stream.isStream())
    {
	throw QPDFExc(qpdf_e_damaged_pdf, this->m->file->getName(),
		      this->m->last_object_description,
		      this->m->file->getLastOffset(),
		      "supposed object stream " +
		      QUtil::int_to_string(obj_stream_number) +
		      " is not a stream");
    }

    // For linearization data in the object, use the data from the
    // object stream for the objects in the stream.
    QPDFObjGen stream_og(obj_stream_number, 0);
    qpdf_offset_t end_before_space =
        this->m->obj_cache[stream_og].end_before_space;
    qpdf_offset_t end_after_space =
        this->m->obj_cache[stream_og].end_after_space;

    QPDFObjectHandle dict = obj_stream.getDict();
    if (! (dict.getKey("/Type").isName() &&
	   dict.getKey("/Type").getName() == "/ObjStm"))
    {
	QTC::TC("qpdf", "QPDF ERR object stream with wrong type");
	throw QPDFExc(qpdf_e_damaged_pdf, this->m->file->getName(),
		      this->m->last_object_description,
		      this->m->file->getLastOffset(),
		      "supposed object stream " +
		      QUtil::int_to_string(obj_stream_number) +
		      " has wrong type");
    }

    if (! (dict.getKey("/N").isInteger() &&
	   dict.getKey("/First").isInteger()))
    {
	throw QPDFExc(qpdf_e_damaged_pdf, this->m->file->getName(),
		      this->m->last_object_description,
		      this->m->file->getLastOffset(),
		      "object stream " +
		      QUtil::int_to_string(obj_stream_number) +
		      " has incorrect keys");
    }

    int n = dict.getKey("/N").getIntValueAsInt();
    int first = dict.getKey("/First").getIntValueAsInt();

    std::map<int, int> offsets;

    PointerHolder<Buffer> bp = obj_stream.getStreamData(qpdf_dl_specialized);
    PointerHolder<InputSource> input = new BufferInputSource(
        this->m->file->getName() +
        " object stream " + QUtil::int_to_string(obj_stream_number),
	bp.getPointer());

    for (int i = 0; i < n; ++i)
    {
	QPDFTokenizer::Token tnum = readToken(input);
	QPDFTokenizer::Token toffset = readToken(input);
	if (! ((tnum.getType() == QPDFTokenizer::tt_integer) &&
	       (toffset.getType() == QPDFTokenizer::tt_integer)))
	{
	    throw QPDFExc(qpdf_e_damaged_pdf, input->getName(),
			  this->m->last_object_description,
                          input->getLastOffset(),
			  "expected integer in object stream header");
	}

	int num = QUtil::string_to_int(tnum.getValue().c_str());
	int offset = QUtil::string_to_int(toffset.getValue().c_str());
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
        QPDFXRefEntry const& entry = this->m->xref_table[og];
        if ((entry.getType() == 2) &&
            (entry.getObjStreamNumber() == obj_stream_number))
        {
            int offset = (*iter).second;
            input->seek(offset, SEEK_SET);
            QPDFObjectHandle oh = readObject(input, "", obj, 0, true);
            this->m->obj_cache[og] =
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
    int max_objid = toI(getObjectCount());
    if (max_objid == std::numeric_limits<int>::max())
    {
        throw std::range_error(
            "max object id is too high to create new objects");
    }
    QPDFObjGen next(max_objid + 1, 0);
    this->m->obj_cache[next] =
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
    this->m->obj_cache[og] =
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
    // Do not preclude use of copyForeignObject on page objects. It is
    // a documented use case to copy pages this way if the intention
    // is to not update the pages tree.
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

    ObjCopier& obj_copier = this->m->object_copiers[other->m->unique_id];
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
        if (obj_copier.object_map.find(foreign_og) !=
            obj_copier.object_map.end())
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
        if (this->m->copied_stream_data_provider == 0)
        {
            this->m->copied_stream_data_provider =
                new CopiedStreamDataProvider(*this);
            this->m->copied_streams = this->m->copied_stream_data_provider;
        }
        QPDFObjGen local_og(result.getObjGen());
        // Copy information from the foreign stream so we can pipe its
        // data later without keeping the original QPDF object around.
        QPDF* foreign_stream_qpdf = foreign.getOwningQPDF();
        if (! foreign_stream_qpdf)
        {
            throw std::logic_error("unable to retrieve owning qpdf"
                                   " from foreign stream");
        }
        QPDF_Stream* stream =
            dynamic_cast<QPDF_Stream*>(
                QPDFObjectHandle::ObjAccessor::getObject(
                    foreign).getPointer());
        if (! stream)
        {
            throw std::logic_error("unable to retrieve underlying"
                                   " stream object from foreign stream");
        }
        PointerHolder<Buffer> stream_buffer =
            stream->getStreamDataBuffer();
        if ((foreign_stream_qpdf->m->immediate_copy_from) &&
            (stream_buffer.getPointer() == 0))
        {
            // Pull the stream data into a buffer before attempting
            // the copy operation. Do it on the source stream so that
            // if the source stream is copied multiple times, we don't
            // have to keep duplicating the memory.
            QTC::TC("qpdf", "QPDF immediate copy stream data");
            foreign.replaceStreamData(foreign.getRawStreamData(),
                                      dict.getKey("/Filter"),
                                      dict.getKey("/DecodeParms"));
            stream_buffer = stream->getStreamDataBuffer();
        }
        PointerHolder<QPDFObjectHandle::StreamDataProvider> stream_provider =
            stream->getStreamDataProvider();
        if (stream_buffer.getPointer())
        {
            QTC::TC("qpdf", "QPDF copy foreign stream with buffer");
            result.replaceStreamData(stream_buffer,
                                     dict.getKey("/Filter"),
                                     dict.getKey("/DecodeParms"));
        }
        else if (stream_provider.getPointer())
        {
            // In this case, the remote stream's QPDF must stay in scope.
            QTC::TC("qpdf", "QPDF copy foreign stream with provider");
            this->m->copied_stream_data_provider->registerForeignStream(
                local_og, foreign);
            result.replaceStreamData(this->m->copied_streams,
                                     dict.getKey("/Filter"),
                                     dict.getKey("/DecodeParms"));
        }
        else
        {
            PointerHolder<ForeignStreamData> foreign_stream_data =
                new ForeignStreamData(
                    foreign_stream_qpdf->m->encp,
                    foreign_stream_qpdf->m->file,
                    foreign.getObjectID(),
                    foreign.getGeneration(),
                    stream->getOffset(),
                    stream->getLength(),
                    (foreign_stream_qpdf->m->attachment_streams.count(
                        foreign.getObjGen()) > 0),
                    dict);
            this->m->copied_stream_data_provider->registerForeignStream(
                local_og, foreign_stream_data);
            result.replaceStreamData(this->m->copied_streams,
                                     dict.getKey("/Filter"),
                                     dict.getKey("/DecodeParms"));
        }
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
    ObjCache t = this->m->obj_cache[og1];
    this->m->obj_cache[og1] = this->m->obj_cache[og2];
    this->m->obj_cache[og2] = t;
}

unsigned long long
QPDF::getUniqueId() const
{
    return this->m->unique_id;
}

std::string
QPDF::getFilename() const
{
    return this->m->file->getName();
}

std::string
QPDF::getPDFVersion() const
{
    return this->m->pdf_version;
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
                    result = obj.getIntValueAsInt();
                }
            }
        }
    }
    return result;
}

QPDFObjectHandle
QPDF::getTrailer()
{
    return this->m->trailer;
}

QPDFObjectHandle
QPDF::getRoot()
{
    QPDFObjectHandle root = this->m->trailer.getKey("/Root");
    if (! root.isDictionary())
    {
        throw QPDFExc(qpdf_e_damaged_pdf, this->m->file->getName(),
                      "", this->m->file->getLastOffset(),
                      "unable to find /Root dictionary");
    }
    return root;
}

std::map<QPDFObjGen, QPDFXRefEntry>
QPDF::getXRefTable()
{
    if (! this->m->parsed)
    {
        throw std::logic_error("QPDF::getXRefTable called before parsing.");
    }

    return this->m->xref_table;
}

void
QPDF::getObjectStreamData(std::map<int, int>& omap)
{
    for (std::map<QPDFObjGen, QPDFXRefEntry>::iterator iter =
	     this->m->xref_table.begin();
	 iter != this->m->xref_table.end(); ++iter)
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
    QPDFObjectHandle encryption_dict =
        this->m->trailer.getKey("/Encrypt");
    QPDFObjGen encryption_dict_og = encryption_dict.getObjGen();

    std::set<QPDFObjGen> visited;
    std::list<QPDFObjectHandle> queue;
    queue.push_front(this->m->trailer);
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
	    else if ((! obj.isStream()) &&
		     (! (obj.isDictionary() &&
                         obj.hasKey("/ByteRange") &&
                         obj.hasKey("/Contents") &&
                         obj.hasKey("/Type") &&
                         obj.getKey("/Type").isName() &&
                         obj.getKey("/Type").getName() == "/Sig")))
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

bool
QPDF::pipeStreamData(PointerHolder<EncryptionParameters> encp,
                     PointerHolder<InputSource> file,
                     QPDF& qpdf_for_warning,
                     int objid, int generation,
		     qpdf_offset_t offset, size_t length,
		     QPDFObjectHandle stream_dict,
                     bool is_attachment_stream,
		     Pipeline* pipeline,
                     bool suppress_warnings,
                     bool will_retry)
{
    std::vector<PointerHolder<Pipeline> > to_delete;
    if (encp->encrypted)
    {
	decryptStream(encp, file, qpdf_for_warning,
                      pipeline, objid, generation,
                      stream_dict, is_attachment_stream, to_delete);
    }

    bool success = false;
    try
    {
	file->seek(offset, SEEK_SET);
	char buf[10240];
	while (length > 0)
	{
	    size_t to_read = (sizeof(buf) < length ? sizeof(buf) : length);
	    size_t len = file->read(buf, to_read);
	    if (len == 0)
	    {
		throw QPDFExc(qpdf_e_damaged_pdf,
			      file->getName(),
			      "",
			      file->getLastOffset(),
			      "unexpected EOF reading stream data");
	    }
	    length -= len;
	    pipeline->write(QUtil::unsigned_char_pointer(buf), len);
	}
        pipeline->finish();
        success = true;
    }
    catch (QPDFExc& e)
    {
        if (! suppress_warnings)
        {
            qpdf_for_warning.warn(e);
        }
    }
    catch (std::exception& e)
    {
        if (! suppress_warnings)
        {
            QTC::TC("qpdf", "QPDF decoding error warning");
            qpdf_for_warning.warn(
                QPDFExc(qpdf_e_damaged_pdf, file->getName(),
                        "", file->getLastOffset(),
                        "error decoding stream data for object " +
                        QUtil::int_to_string(objid) + " " +
                        QUtil::int_to_string(generation) + ": " + e.what()));
            if (will_retry)
            {
                qpdf_for_warning.warn(
                    QPDFExc(qpdf_e_damaged_pdf, file->getName(),
                            "", file->getLastOffset(),
                            "stream will be re-processed without"
                            " filtering to avoid data loss"));
            }
        }
    }
    if (! success)
    {
        try
        {
            pipeline->finish();
        }
        catch (std::exception&)
        {
            // ignore
        }
    }
    return success;
}

bool
QPDF::pipeStreamData(int objid, int generation,
		     qpdf_offset_t offset, size_t length,
		     QPDFObjectHandle stream_dict,
		     Pipeline* pipeline,
                     bool suppress_warnings,
                     bool will_retry)
{
    bool is_attachment_stream = (
        this->m->attachment_streams.count(
            QPDFObjGen(objid, generation)) > 0);
    return pipeStreamData(
        this->m->encp, this->m->file, *this,
        objid, generation, offset, length,
        stream_dict, is_attachment_stream,
        pipeline, suppress_warnings, will_retry);
}

bool
QPDF::pipeForeignStreamData(
    PointerHolder<ForeignStreamData> foreign,
    Pipeline* pipeline,
    bool suppress_warnings, bool will_retry)
{
    if (foreign->encp->encrypted)
    {
        QTC::TC("qpdf", "QPDF pipe foreign encrypted stream");
    }
    return pipeStreamData(
        foreign->encp, foreign->file, *this,
        foreign->foreign_objid, foreign->foreign_generation,
        foreign->offset, foreign->length,
        foreign->local_dict, foreign->is_attachment_stream,
        pipeline, suppress_warnings, will_retry);
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
            this->m->attachment_streams.insert(stream.getObjGen());
        }
    }
}

void
QPDF::stopOnError(std::string const& message)
{
    // Throw a generic exception when we lack context for something
    // more specific. New code should not use this. This method exists
    // to improve somewhat from calling assert in very old code.
    throw QPDFExc(qpdf_e_damaged_pdf, this->m->file->getName(),
                  "", this->m->file->getLastOffset(), message);
}
