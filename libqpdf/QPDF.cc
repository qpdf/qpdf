#include <qpdf/qpdf-config.h> // include first for large file support

#include <qpdf/QPDF.hh>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <memory.h>
#include <regex>
#include <sstream>
#include <vector>

#include <qpdf/BufferInputSource.hh>
#include <qpdf/FileInputSource.hh>
#include <qpdf/OffsetInputSource.hh>
#include <qpdf/Pipeline.hh>
#include <qpdf/Pl_Discard.hh>
#include <qpdf/Pl_OStream.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/QPDFLogger.hh>
#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QPDFParser.hh>
#include <qpdf/QPDF_Array.hh>
#include <qpdf/QPDF_Dictionary.hh>
#include <qpdf/QPDF_Null.hh>
#include <qpdf/QPDF_Reserved.hh>
#include <qpdf/QPDF_Stream.hh>
#include <qpdf/QPDF_Unresolved.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>

// This must be a fixed value. This API returns a const reference to it, and the C API relies on its
// being static as well.
std::string const QPDF::qpdf_version(QPDF_VERSION);

static char const* EMPTY_PDF = (
    // force line break
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
    "%%EOF\n");

namespace
{
    class InvalidInputSource: public InputSource
    {
      public:
        ~InvalidInputSource() override = default;
        qpdf_offset_t
        findAndSkipNextEOL() override
        {
            throwException();
            return 0;
        }
        std::string const&
        getName() const override
        {
            static std::string name("closed input source");
            return name;
        }
        qpdf_offset_t
        tell() override
        {
            throwException();
            return 0;
        }
        void
        seek(qpdf_offset_t offset, int whence) override
        {
            throwException();
        }
        void
        rewind() override
        {
            throwException();
        }
        size_t
        read(char* buffer, size_t length) override
        {
            throwException();
            return 0;
        }
        void
        unreadCh(char ch) override
        {
            throwException();
        }

      private:
        void
        throwException()
        {
            throw std::logic_error("QPDF operation attempted on a QPDF object with no input "
                                   "source. QPDF operations are invalid before processFile (or "
                                   "another process method) or after closeInputSource");
        }
    };
} // namespace

QPDF::ForeignStreamData::ForeignStreamData(
    std::shared_ptr<EncryptionParameters> encp,
    std::shared_ptr<InputSource> file,
    QPDFObjGen const& foreign_og,
    qpdf_offset_t offset,
    size_t length,
    QPDFObjectHandle local_dict) :
    encp(encp),
    file(file),
    foreign_og(foreign_og),
    offset(offset),
    length(length),
    local_dict(local_dict)
{
}

QPDF::CopiedStreamDataProvider::CopiedStreamDataProvider(QPDF& destination_qpdf) :
    QPDFObjectHandle::StreamDataProvider(true),
    destination_qpdf(destination_qpdf)
{
}

bool
QPDF::CopiedStreamDataProvider::provideStreamData(
    QPDFObjGen const& og, Pipeline* pipeline, bool suppress_warnings, bool will_retry)
{
    std::shared_ptr<ForeignStreamData> foreign_data = foreign_stream_data[og];
    bool result = false;
    if (foreign_data.get()) {
        result = destination_qpdf.pipeForeignStreamData(
            foreign_data, pipeline, suppress_warnings, will_retry);
        QTC::TC("qpdf", "QPDF copy foreign with data", result ? 0 : 1);
    } else {
        auto foreign_stream = foreign_streams[og];
        result = foreign_stream.pipeStreamData(
            pipeline, nullptr, 0, qpdf_dl_none, suppress_warnings, will_retry);
        QTC::TC("qpdf", "QPDF copy foreign with foreign_stream", result ? 0 : 1);
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
    QPDFObjGen const& local_og, std::shared_ptr<ForeignStreamData> foreign_stream)
{
    this->foreign_stream_data[local_og] = foreign_stream;
}

QPDF::StringDecrypter::StringDecrypter(QPDF* qpdf, QPDFObjGen const& og) :
    qpdf(qpdf),
    og(og)
{
}

void
QPDF::StringDecrypter::decryptString(std::string& val)
{
    qpdf->decryptString(val, og);
}

std::string const&
QPDF::QPDFVersion()
{
    // The C API relies on this being a static value.
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
    user_password_matched(false),
    owner_password_matched(false)
{
}

QPDF::Members::Members() :
    log(QPDFLogger::defaultLogger()),
    file(new InvalidInputSource()),
    encp(new EncryptionParameters)
{
}

QPDF::QPDF() :
    m(new Members())
{
    m->tokenizer.allowEOF();
    // Generate a unique ID. It just has to be unique among all QPDF objects allocated throughout
    // the lifetime of this running application.
    static std::atomic<unsigned long long> unique_id{0};
    m->unique_id = unique_id.fetch_add(1ULL);
}

QPDF::~QPDF()
{
    // If two objects are mutually referential (through each object having an array or dictionary
    // that contains an indirect reference to the other), the circular references in the
    // std::shared_ptr objects will prevent the objects from being deleted. Walk through all objects
    // in the object cache, which is those objects that we read from the file, and break all
    // resolved indirect references by replacing them with an internal object type representing that
    // they have been destroyed. Note that we can't break references like this at any time when the
    // QPDF object is active. The call to reset also causes all direct QPDFObjectHandle objects that
    // are reachable from this object to release their association with this QPDF. Direct objects
    // are not destroyed since they can be moved to other QPDF objects safely.

    // At this point, obviously no one is still using the QPDF object, but we'll explicitly clear
    // the xref table anyway just to prevent any possibility of resolve() succeeding.
    m->xref_table.clear();
    for (auto const& iter: m->obj_cache) {
        iter.second.object->disconnect();
        if (iter.second.object->getTypeCode() != ::ot_null) {
            iter.second.object->destroy();
        }
    }
}

std::shared_ptr<QPDF>
QPDF::create()
{
    return std::make_shared<QPDF>();
}

void
QPDF::processFile(char const* filename, char const* password)
{
    auto* fi = new FileInputSource(filename);
    processInputSource(std::shared_ptr<InputSource>(fi), password);
}

void
QPDF::processFile(char const* description, FILE* filep, bool close_file, char const* password)
{
    auto* fi = new FileInputSource(description, filep, close_file);
    processInputSource(std::shared_ptr<InputSource>(fi), password);
}

void
QPDF::processMemoryFile(
    char const* description, char const* buf, size_t length, char const* password)
{
    processInputSource(
        std::shared_ptr<InputSource>(
            // line-break
            new BufferInputSource(
                description, new Buffer(QUtil::unsigned_char_pointer(buf), length), true)),
        password);
}

void
QPDF::processInputSource(std::shared_ptr<InputSource> source, char const* password)
{
    m->file = source;
    parse(password);
}

void
QPDF::closeInputSource()
{
    m->file = std::shared_ptr<InputSource>(new InvalidInputSource());
}

void
QPDF::setPasswordIsHexKey(bool val)
{
    m->provided_password_is_hex_key = val;
}

void
QPDF::emptyPDF()
{
    processMemoryFile("empty PDF", EMPTY_PDF, strlen(EMPTY_PDF));
}

void
QPDF::registerStreamFilter(
    std::string const& filter_name, std::function<std::shared_ptr<QPDFStreamFilter>()> factory)
{
    QPDF_Stream::registerStreamFilter(filter_name, factory);
}

void
QPDF::setIgnoreXRefStreams(bool val)
{
    m->ignore_xref_streams = val;
}

std::shared_ptr<QPDFLogger>
QPDF::getLogger()
{
    return m->log;
}

void
QPDF::setLogger(std::shared_ptr<QPDFLogger> l)
{
    m->log = l;
}

void
QPDF::setOutputStreams(std::ostream* out, std::ostream* err)
{
    setLogger(QPDFLogger::create());
    m->log->setOutputStreams(out, err);
}

void
QPDF::setSuppressWarnings(bool val)
{
    m->suppress_warnings = val;
}

void
QPDF::setAttemptRecovery(bool val)
{
    m->attempt_recovery = val;
}

void
QPDF::setImmediateCopyFrom(bool val)
{
    m->immediate_copy_from = val;
}

std::vector<QPDFExc>
QPDF::getWarnings()
{
    std::vector<QPDFExc> result = m->warnings;
    m->warnings.clear();
    return result;
}

bool
QPDF::anyWarnings() const
{
    return !m->warnings.empty();
}

size_t
QPDF::numWarnings() const
{
    return m->warnings.size();
}

bool
QPDF::validatePDFVersion(char const*& p, std::string& version)
{
    bool valid = QUtil::is_digit(*p);
    if (valid) {
        while (QUtil::is_digit(*p)) {
            version.append(1, *p++);
        }
        if ((*p == '.') && QUtil::is_digit(*(p + 1))) {
            version.append(1, *p++);
            while (QUtil::is_digit(*p)) {
                version.append(1, *p++);
            }
        } else {
            valid = false;
        }
    }
    return valid;
}

bool
QPDF::findHeader()
{
    qpdf_offset_t global_offset = m->file->tell();
    std::string line = m->file->readLine(1024);
    char const* p = line.c_str();
    if (strncmp(p, "%PDF-", 5) != 0) {
        throw std::logic_error("findHeader is not looking at %PDF-");
    }
    p += 5;
    std::string version;
    // Note: The string returned by line.c_str() is always null-terminated. The code below never
    // overruns the buffer because a null character always short-circuits further advancement.
    bool valid = validatePDFVersion(p, version);
    if (valid) {
        m->pdf_version = version;
        if (global_offset != 0) {
            // Empirical evidence strongly suggests that when there is leading material prior to the
            // PDF header, all explicit offsets in the file are such that 0 points to the beginning
            // of the header.
            QTC::TC("qpdf", "QPDF global offset");
            m->file = std::shared_ptr<InputSource>(new OffsetInputSource(m->file, global_offset));
        }
    }
    return valid;
}

bool
QPDF::findStartxref()
{
    if (readToken(m->file).isWord("startxref") && readToken(m->file).isInteger()) {
        // Position in front of offset token
        m->file->seek(m->file->getLastOffset(), SEEK_SET);
        return true;
    }
    return false;
}

void
QPDF::parse(char const* password)
{
    if (password) {
        m->encp->provided_password = password;
    }

    // Find the header anywhere in the first 1024 bytes of the file.
    PatternFinder hf(*this, &QPDF::findHeader);
    if (!m->file->findFirst("%PDF-", 0, 1024, hf)) {
        QTC::TC("qpdf", "QPDF not a pdf file");
        warn(damagedPDF("", 0, "can't find PDF header"));
        // QPDFWriter writes files that usually require at least version 1.2 for /FlateDecode
        m->pdf_version = "1.2";
    }

    // PDF spec says %%EOF must be found within the last 1024 bytes of/ the file.  We add an extra
    // 30 characters to leave room for the startxref stuff.
    m->file->seek(0, SEEK_END);
    qpdf_offset_t end_offset = m->file->tell();
    qpdf_offset_t start_offset = (end_offset > 1054 ? end_offset - 1054 : 0);
    PatternFinder sf(*this, &QPDF::findStartxref);
    qpdf_offset_t xref_offset = 0;
    if (m->file->findLast("startxref", start_offset, 0, sf)) {
        xref_offset = QUtil::string_to_ll(readToken(m->file).getValue().c_str());
    }

    try {
        if (xref_offset == 0) {
            QTC::TC("qpdf", "QPDF can't find startxref");
            throw damagedPDF("", 0, "can't find startxref");
        }
        try {
            read_xref(xref_offset);
        } catch (QPDFExc&) {
            throw;
        } catch (std::exception& e) {
            throw damagedPDF("", 0, std::string("error reading xref: ") + e.what());
        }
    } catch (QPDFExc& e) {
        if (m->attempt_recovery) {
            reconstruct_xref(e);
            QTC::TC("qpdf", "QPDF reconstructed xref table");
        } else {
            throw;
        }
    }

    initializeEncryption();
    m->parsed = true;
}

void
QPDF::inParse(bool v)
{
    if (m->in_parse == v) {
        // This happens if QPDFParser::parse tries to resolve an indirect object while it is
        // parsing.
        throw std::logic_error("QPDF: re-entrant parsing detected. This is a qpdf bug."
                               " Please report at https://github.com/qpdf/qpdf/issues.");
    }
    m->in_parse = v;
}

void
QPDF::warn(QPDFExc const& e)
{
    m->warnings.push_back(e);
    if (!m->suppress_warnings) {
        *m->log->getWarn() << "WARNING: " << m->warnings.back().what() << "\n";
    }
}

void
QPDF::warn(
    qpdf_error_code_e error_code,
    std::string const& object,
    qpdf_offset_t offset,
    std::string const& message)
{
    warn(QPDFExc(error_code, getFilename(), object, offset, message));
}

void
QPDF::setTrailer(QPDFObjectHandle obj)
{
    if (m->trailer.isInitialized()) {
        return;
    }
    m->trailer = obj;
}

void
QPDF::reconstruct_xref(QPDFExc& e)
{
    if (m->reconstructed_xref) {
        // Avoid xref reconstruction infinite loops. This is getting very hard to reproduce because
        // qpdf is throwing many fewer exceptions while parsing. Most situations are warnings now.
        throw e;
    }

    m->reconstructed_xref = true;
    // We may find more objects, which may contain dangling references.
    m->fixed_dangling_refs = false;

    warn(damagedPDF("", 0, "file is damaged"));
    warn(e);
    warn(damagedPDF("", 0, "Attempting to reconstruct cross-reference table"));

    // Delete all references to type 1 (uncompressed) objects
    std::set<QPDFObjGen> to_delete;
    for (auto const& iter: m->xref_table) {
        if (iter.second.getType() == 1) {
            to_delete.insert(iter.first);
        }
    }
    for (auto const& iter: to_delete) {
        m->xref_table.erase(iter);
    }

    m->file->seek(0, SEEK_END);
    qpdf_offset_t eof = m->file->tell();
    m->file->seek(0, SEEK_SET);
    qpdf_offset_t line_start = 0;
    // Don't allow very long tokens here during recovery.
    static size_t const MAX_LEN = 100;
    while (m->file->tell() < eof) {
        m->file->findAndSkipNextEOL();
        qpdf_offset_t next_line_start = m->file->tell();
        m->file->seek(line_start, SEEK_SET);
        QPDFTokenizer::Token t1 = readToken(m->file, MAX_LEN);
        qpdf_offset_t token_start = m->file->tell() - toO(t1.getValue().length());
        if (token_start >= next_line_start) {
            // don't process yet -- wait until we get to the line containing this token
        } else if (t1.isInteger()) {
            QPDFTokenizer::Token t2 = readToken(m->file, MAX_LEN);
            if ((t2.isInteger()) && (readToken(m->file, MAX_LEN).isWord("obj"))) {
                int obj = QUtil::string_to_int(t1.getValue().c_str());
                int gen = QUtil::string_to_int(t2.getValue().c_str());
                insertReconstructedXrefEntry(obj, token_start, gen);
            }
        } else if (!m->trailer.isInitialized() && t1.isWord("trailer")) {
            QPDFObjectHandle t = readTrailer();
            if (!t.isDictionary()) {
                // Oh well.  It was worth a try.
            } else {
                setTrailer(t);
            }
        }
        m->file->seek(next_line_start, SEEK_SET);
        line_start = next_line_start;
    }
    m->deleted_objects.clear();

    if (!m->trailer.isInitialized()) {
        // We could check the last encountered object to see if it was an xref stream.  If so, we
        // could try to get the trailer from there.  This may make it possible to recover files with
        // bad startxref pointers even when they have object streams.

        throw damagedPDF("", 0, "unable to find trailer dictionary while recovering damaged file");
    }

    // We could iterate through the objects looking for streams and try to find objects inside of
    // them, but it's probably not worth the trouble.  Acrobat can't recover files with any errors
    // in an xref stream, and this would be a real long shot anyway.  If we wanted to do anything
    // that involved looking at stream contents, we'd also have to call initializeEncryption() here.
    // It's safe to call it more than once.
}

void
QPDF::read_xref(qpdf_offset_t xref_offset)
{
    std::map<int, int> free_table;
    std::set<qpdf_offset_t> visited;
    while (xref_offset) {
        visited.insert(xref_offset);
        char buf[7];
        memset(buf, 0, sizeof(buf));
        m->file->seek(xref_offset, SEEK_SET);
        // Some files miss the mark a little with startxref. We could do a better job of searching
        // in the neighborhood for something that looks like either an xref table or stream, but the
        // simple heuristic of skipping whitespace can help with the xref table case and is harmless
        // with the stream case.
        bool done = false;
        bool skipped_space = false;
        while (!done) {
            char ch;
            if (1 == m->file->read(&ch, 1)) {
                if (QUtil::is_space(ch)) {
                    skipped_space = true;
                } else {
                    m->file->unreadCh(ch);
                    done = true;
                }
            } else {
                QTC::TC("qpdf", "QPDF eof skipping spaces before xref", skipped_space ? 0 : 1);
                done = true;
            }
        }

        m->file->read(buf, sizeof(buf) - 1);
        // The PDF spec says xref must be followed by a line terminator, but files exist in the wild
        // where it is terminated by arbitrary whitespace.
        if ((strncmp(buf, "xref", 4) == 0) && QUtil::is_space(buf[4])) {
            if (skipped_space) {
                QTC::TC("qpdf", "QPDF xref skipped space");
                warn(damagedPDF("", 0, "extraneous whitespace seen before xref"));
            }
            QTC::TC(
                "qpdf",
                "QPDF xref space",
                ((buf[4] == '\n')       ? 0
                     : (buf[4] == '\r') ? 1
                     : (buf[4] == ' ')  ? 2
                                        : 9999));
            int skip = 4;
            // buf is null-terminated, and QUtil::is_space('\0') is false, so this won't overrun.
            while (QUtil::is_space(buf[skip])) {
                ++skip;
            }
            xref_offset = read_xrefTable(xref_offset + skip);
        } else {
            xref_offset = read_xrefStream(xref_offset);
        }
        if (visited.count(xref_offset) != 0) {
            QTC::TC("qpdf", "QPDF xref loop");
            throw damagedPDF("", 0, "loop detected following xref tables");
        }
    }

    if (!m->trailer.isInitialized()) {
        throw damagedPDF("", 0, "unable to find trailer while reading xref");
    }
    int size = m->trailer.getKey("/Size").getIntValueAsInt();
    int max_obj = 0;
    if (!m->xref_table.empty()) {
        max_obj = (*(m->xref_table.rbegin())).first.getObj();
    }
    if (!m->deleted_objects.empty()) {
        max_obj = std::max(max_obj, *(m->deleted_objects.rbegin()));
    }
    if ((size < 1) || (size - 1 != max_obj)) {
        QTC::TC("qpdf", "QPDF xref size mismatch");
        warn(damagedPDF(
            "",
            0,
            ("reported number of objects (" + std::to_string(size) +
             ") is not one plus the highest object number (" + std::to_string(max_obj) + ")")));
    }

    // We no longer need the deleted_objects table, so go ahead and clear it out to make sure we
    // never depend on its being set.
    m->deleted_objects.clear();
}

bool
QPDF::parse_xrefFirst(std::string const& line, int& obj, int& num, int& bytes)
{
    // is_space and is_digit both return false on '\0', so this will not overrun the null-terminated
    // buffer.
    char const* p = line.c_str();
    char const* start = line.c_str();

    // Skip zero or more spaces
    while (QUtil::is_space(*p)) {
        ++p;
    }
    // Require digit
    if (!QUtil::is_digit(*p)) {
        return false;
    }
    // Gather digits
    std::string obj_str;
    while (QUtil::is_digit(*p)) {
        obj_str.append(1, *p++);
    }
    // Require space
    if (!QUtil::is_space(*p)) {
        return false;
    }
    // Skip spaces
    while (QUtil::is_space(*p)) {
        ++p;
    }
    // Require digit
    if (!QUtil::is_digit(*p)) {
        return false;
    }
    // Gather digits
    std::string num_str;
    while (QUtil::is_digit(*p)) {
        num_str.append(1, *p++);
    }
    // Skip any space including line terminators
    while (QUtil::is_space(*p)) {
        ++p;
    }
    bytes = toI(p - start);
    obj = QUtil::string_to_int(obj_str.c_str());
    num = QUtil::string_to_int(num_str.c_str());
    return true;
}

bool
QPDF::parse_xrefEntry(std::string const& line, qpdf_offset_t& f1, int& f2, char& type)
{
    // is_space and is_digit both return false on '\0', so this will not overrun the null-terminated
    // buffer.
    char const* p = line.c_str();

    // Skip zero or more spaces. There aren't supposed to be any.
    bool invalid = false;
    while (QUtil::is_space(*p)) {
        ++p;
        QTC::TC("qpdf", "QPDF ignore first space in xref entry");
        invalid = true;
    }
    // Require digit
    if (!QUtil::is_digit(*p)) {
        return false;
    }
    // Gather digits
    std::string f1_str;
    while (QUtil::is_digit(*p)) {
        f1_str.append(1, *p++);
    }
    // Require space
    if (!QUtil::is_space(*p)) {
        return false;
    }
    if (QUtil::is_space(*(p + 1))) {
        QTC::TC("qpdf", "QPDF ignore first extra space in xref entry");
        invalid = true;
    }
    // Skip spaces
    while (QUtil::is_space(*p)) {
        ++p;
    }
    // Require digit
    if (!QUtil::is_digit(*p)) {
        return false;
    }
    // Gather digits
    std::string f2_str;
    while (QUtil::is_digit(*p)) {
        f2_str.append(1, *p++);
    }
    // Require space
    if (!QUtil::is_space(*p)) {
        return false;
    }
    if (QUtil::is_space(*(p + 1))) {
        QTC::TC("qpdf", "QPDF ignore second extra space in xref entry");
        invalid = true;
    }
    // Skip spaces
    while (QUtil::is_space(*p)) {
        ++p;
    }
    if ((*p == 'f') || (*p == 'n')) {
        type = *p;
    } else {
        return false;
    }
    if ((f1_str.length() != 10) || (f2_str.length() != 5)) {
        QTC::TC("qpdf", "QPDF ignore length error xref entry");
        invalid = true;
    }

    if (invalid) {
        warn(damagedPDF("xref table", "accepting invalid xref table entry"));
    }

    f1 = QUtil::string_to_ll(f1_str.c_str());
    f2 = QUtil::string_to_int(f2_str.c_str());

    return true;
}

qpdf_offset_t
QPDF::read_xrefTable(qpdf_offset_t xref_offset)
{
    std::vector<QPDFObjGen> deleted_items;

    m->file->seek(xref_offset, SEEK_SET);
    bool done = false;
    while (!done) {
        char linebuf[51];
        memset(linebuf, 0, sizeof(linebuf));
        m->file->read(linebuf, sizeof(linebuf) - 1);
        std::string line = linebuf;
        int obj = 0;
        int num = 0;
        int bytes = 0;
        if (!parse_xrefFirst(line, obj, num, bytes)) {
            QTC::TC("qpdf", "QPDF invalid xref");
            throw damagedPDF("xref table", "xref syntax invalid");
        }
        m->file->seek(m->file->getLastOffset() + bytes, SEEK_SET);
        for (qpdf_offset_t i = obj; i - num < obj; ++i) {
            if (i == 0) {
                // This is needed by checkLinearization()
                m->first_xref_item_offset = m->file->tell();
            }
            std::string xref_entry = m->file->readLine(30);
            // For xref_table, these will always be small enough to be ints
            qpdf_offset_t f1 = 0;
            int f2 = 0;
            char type = '\0';
            if (!parse_xrefEntry(xref_entry, f1, f2, type)) {
                QTC::TC("qpdf", "QPDF invalid xref entry");
                throw damagedPDF(
                    "xref table", "invalid xref entry (obj=" + std::to_string(i) + ")");
            }
            if (type == 'f') {
                // Save deleted items until after we've checked the XRefStm, if any.
                deleted_items.emplace_back(toI(i), f2);
            } else {
                insertXrefEntry(toI(i), 1, f1, f2);
            }
        }
        qpdf_offset_t pos = m->file->tell();
        if (readToken(m->file).isWord("trailer")) {
            done = true;
        } else {
            m->file->seek(pos, SEEK_SET);
        }
    }

    // Set offset to previous xref table if any
    QPDFObjectHandle cur_trailer = readTrailer();
    if (!cur_trailer.isDictionary()) {
        QTC::TC("qpdf", "QPDF missing trailer");
        throw damagedPDF("", "expected trailer dictionary");
    }

    if (!m->trailer.isInitialized()) {
        setTrailer(cur_trailer);

        if (!m->trailer.hasKey("/Size")) {
            QTC::TC("qpdf", "QPDF trailer lacks size");
            throw damagedPDF("trailer", "trailer dictionary lacks /Size key");
        }
        if (!m->trailer.getKey("/Size").isInteger()) {
            QTC::TC("qpdf", "QPDF trailer size not integer");
            throw damagedPDF("trailer", "/Size key in trailer dictionary is not an integer");
        }
    }

    if (cur_trailer.hasKey("/XRefStm")) {
        if (m->ignore_xref_streams) {
            QTC::TC("qpdf", "QPDF ignoring XRefStm in trailer");
        } else {
            if (cur_trailer.getKey("/XRefStm").isInteger()) {
                // Read the xref stream but disregard any return value -- we'll use our trailer's
                // /Prev key instead of the xref stream's.
                (void)read_xrefStream(cur_trailer.getKey("/XRefStm").getIntValue());
            } else {
                throw damagedPDF("xref stream", xref_offset, "invalid /XRefStm");
            }
        }
    }

    // Handle any deleted items now that we've read the /XRefStm.
    for (auto const& og: deleted_items) {
        insertFreeXrefEntry(og);
    }

    if (cur_trailer.hasKey("/Prev")) {
        if (!cur_trailer.getKey("/Prev").isInteger()) {
            QTC::TC("qpdf", "QPDF trailer prev not integer");
            throw damagedPDF("trailer", "/Prev key in trailer dictionary is not an integer");
        }
        QTC::TC("qpdf", "QPDF prev key in trailer dictionary");
        xref_offset = cur_trailer.getKey("/Prev").getIntValue();
    } else {
        xref_offset = 0;
    }

    return xref_offset;
}

qpdf_offset_t
QPDF::read_xrefStream(qpdf_offset_t xref_offset)
{
    if (!m->ignore_xref_streams) {
        QPDFObjGen x_og;
        QPDFObjectHandle xref_obj;
        try {
            xref_obj =
                readObjectAtOffset(false, xref_offset, "xref stream", QPDFObjGen(0, 0), x_og, true);
        } catch (QPDFExc&) {
            // ignore -- report error below
        }
        if (xref_obj.isStreamOfType("/XRef")) {
            QTC::TC("qpdf", "QPDF found xref stream");
            return processXRefStream(xref_offset, xref_obj);
        }
    }

    QTC::TC("qpdf", "QPDF can't find xref");
    throw damagedPDF("", xref_offset, "xref not found");
    return 0; // unreachable
}

qpdf_offset_t
QPDF::processXRefStream(qpdf_offset_t xref_offset, QPDFObjectHandle& xref_obj)
{
    QPDFObjectHandle dict = xref_obj.getDict();
    QPDFObjectHandle W_obj = dict.getKey("/W");
    QPDFObjectHandle Index_obj = dict.getKey("/Index");
    if (!(W_obj.isArray() && (W_obj.getArrayNItems() >= 3) && W_obj.getArrayItem(0).isInteger() &&
          W_obj.getArrayItem(1).isInteger() && W_obj.getArrayItem(2).isInteger() &&
          dict.getKey("/Size").isInteger() && (Index_obj.isArray() || Index_obj.isNull()))) {
        throw damagedPDF(
            "xref stream",
            xref_offset,
            "Cross-reference stream does not have proper /W and /Index keys");
    }

    int W[3];
    size_t entry_size = 0;
    int max_bytes = sizeof(qpdf_offset_t);
    for (int i = 0; i < 3; ++i) {
        W[i] = W_obj.getArrayItem(i).getIntValueAsInt();
        if (W[i] > max_bytes) {
            throw damagedPDF(
                "xref stream",
                xref_offset,
                "Cross-reference stream's /W contains impossibly large values");
        }
        entry_size += toS(W[i]);
    }
    if (entry_size == 0) {
        throw damagedPDF(
            "xref stream", xref_offset, "Cross-reference stream's /W indicates entry size of 0");
    }
    unsigned long long max_num_entries = static_cast<unsigned long long>(-1) / entry_size;

    std::vector<long long> indx;
    if (Index_obj.isArray()) {
        int n_index = Index_obj.getArrayNItems();
        if ((n_index % 2) || (n_index < 2)) {
            throw damagedPDF(
                "xref stream",
                xref_offset,
                "Cross-reference stream's /Index has an invalid number of "
                "values");
        }
        for (int i = 0; i < n_index; ++i) {
            if (Index_obj.getArrayItem(i).isInteger()) {
                indx.push_back(Index_obj.getArrayItem(i).getIntValue());
            } else {
                throw damagedPDF(
                    "xref stream",
                    xref_offset,
                    ("Cross-reference stream's /Index's item " + std::to_string(i) +
                     " is not an integer"));
            }
        }
        QTC::TC("qpdf", "QPDF xref /Index is array", n_index == 2 ? 0 : 1);
    } else {
        QTC::TC("qpdf", "QPDF xref /Index is null");
        long long size = dict.getKey("/Size").getIntValue();
        indx.push_back(0);
        indx.push_back(size);
    }

    size_t num_entries = 0;
    for (size_t i = 1; i < indx.size(); i += 2) {
        if (indx.at(i) > QIntC::to_longlong(max_num_entries - num_entries)) {
            throw damagedPDF(
                "xref stream",
                xref_offset,
                ("Cross-reference stream claims to contain too many entries: " +
                 std::to_string(indx.at(i)) + " " + std::to_string(max_num_entries) + " " +
                 std::to_string(num_entries)));
        }
        num_entries += toS(indx.at(i));
    }

    // entry_size and num_entries have both been validated to ensure that this multiplication does
    // not cause an overflow.
    size_t expected_size = entry_size * num_entries;

    std::shared_ptr<Buffer> bp = xref_obj.getStreamData(qpdf_dl_specialized);
    size_t actual_size = bp->getSize();

    if (expected_size != actual_size) {
        QPDFExc x = damagedPDF(
            "xref stream",
            xref_offset,
            ("Cross-reference stream data has the wrong size; expected = " +
             std::to_string(expected_size) + "; actual = " + std::to_string(actual_size)));
        if (expected_size > actual_size) {
            throw x;
        } else {
            warn(x);
        }
    }

    size_t cur_chunk = 0;
    int chunk_count = 0;

    bool saw_first_compressed_object = false;

    // Actual size vs. expected size check above ensures that we will not overflow any buffers here.
    // We know that entry_size * num_entries is equal to the size of the buffer.
    unsigned char const* data = bp->getBuffer();
    for (size_t i = 0; i < num_entries; ++i) {
        // Read this entry
        unsigned char const* entry = data + (entry_size * i);
        qpdf_offset_t fields[3];
        unsigned char const* p = entry;
        for (int j = 0; j < 3; ++j) {
            fields[j] = 0;
            if ((j == 0) && (W[0] == 0)) {
                QTC::TC("qpdf", "QPDF default for xref stream field 0");
                fields[0] = 1;
            }
            for (int k = 0; k < W[j]; ++k) {
                fields[j] <<= 8;
                fields[j] += toI(*p++);
            }
        }

        // Get the object and generation number.  The object number is based on /Index.  The
        // generation number is 0 unless this is an uncompressed object record, in which case the
        // generation number appears as the third field.
        int obj = toI(indx.at(cur_chunk));
        if ((obj < 0) || ((std::numeric_limits<int>::max() - obj) < chunk_count)) {
            std::ostringstream msg;
            msg.imbue(std::locale::classic());
            msg << "adding " << chunk_count << " to " << obj
                << " while computing index in xref stream would cause an integer overflow";
            throw std::range_error(msg.str());
        }
        obj += chunk_count;
        ++chunk_count;
        if (chunk_count >= indx.at(cur_chunk + 1)) {
            cur_chunk += 2;
            chunk_count = 0;
        }

        if (saw_first_compressed_object) {
            if (fields[0] != 2) {
                m->uncompressed_after_compressed = true;
            }
        } else if (fields[0] == 2) {
            saw_first_compressed_object = true;
        }
        if (obj == 0) {
            // This is needed by checkLinearization()
            m->first_xref_item_offset = xref_offset;
        }
        if (fields[0] == 0) {
            // Ignore fields[2], which we don't care about in this case. This works around the issue
            // of some PDF files that put invalid values, like -1, here for deleted objects.
            insertFreeXrefEntry(QPDFObjGen(obj, 0));
        } else {
            insertXrefEntry(obj, toI(fields[0]), fields[1], toI(fields[2]));
        }
    }

    if (!m->trailer.isInitialized()) {
        setTrailer(dict);
    }

    if (dict.hasKey("/Prev")) {
        if (!dict.getKey("/Prev").isInteger()) {
            throw damagedPDF(
                "xref stream", "/Prev key in xref stream dictionary is not an integer");
        }
        QTC::TC("qpdf", "QPDF prev key in xref stream dictionary");
        xref_offset = dict.getKey("/Prev").getIntValue();
    } else {
        xref_offset = 0;
    }

    return xref_offset;
}

void
QPDF::insertXrefEntry(int obj, int f0, qpdf_offset_t f1, int f2)
{
    // Populate the xref table in such a way that the first reference to an object that we see,
    // which is the one in the latest xref table in which it appears, is the one that gets stored.
    // This works because we are reading more recent appends before older ones.

    // If there is already an entry for this object and generation in the table, it means that a
    // later xref table has registered this object.  Disregard this one.

    if (m->deleted_objects.count(obj)) {
        QTC::TC("qpdf", "QPDF xref deleted object");
        return;
    }

    auto [iter, created] = m->xref_table.try_emplace(QPDFObjGen(obj, (f0 == 2 ? 0 : f2)));
    if (!created) {
        QTC::TC("qpdf", "QPDF xref reused object");
        return;
    }

    switch (f0) {
    case 1:
        // f2 is generation
        QTC::TC("qpdf", "QPDF xref gen > 0", ((f2 > 0) ? 1 : 0));
        iter->second = QPDFXRefEntry(f1);
        break;

    case 2:
        iter->second = QPDFXRefEntry(toI(f1), f2);
        break;

    default:
        throw damagedPDF("xref stream", "unknown xref stream entry type " + std::to_string(f0));
        break;
    }
}

void
QPDF::insertFreeXrefEntry(QPDFObjGen og)
{
    if (!m->xref_table.count(og)) {
        m->deleted_objects.insert(og.getObj());
    }
}

// Replace uncompressed object. This is used in xref recovery mode, which reads the file from
// beginning to end.
void
QPDF::insertReconstructedXrefEntry(int obj, qpdf_offset_t f1, int f2)
{
    QPDFObjGen og(obj, f2);
    if (!m->deleted_objects.count(obj)) {
        // deleted_objects stores the uncompressed objects removed from the xref table at the start
        // of recovery.
        QTC::TC("qpdf", "QPDF xref overwrite object");
        m->xref_table[QPDFObjGen(obj, f2)] = QPDFXRefEntry(f1);
    }
}

void
QPDF::showXRefTable()
{
    auto& cout = *m->log->getInfo();
    for (auto const& iter: m->xref_table) {
        QPDFObjGen const& og = iter.first;
        QPDFXRefEntry const& entry = iter.second;
        cout << og.unparse('/') << ": ";
        switch (entry.getType()) {
        case 1:
            cout << "uncompressed; offset = " << entry.getOffset();
            break;

        case 2:
            *m->log->getInfo() << "compressed; stream = " << entry.getObjStreamNumber()
                               << ", index = " << entry.getObjStreamIndex();
            break;

        default:
            throw std::logic_error("unknown cross-reference table type while"
                                   " showing xref_table");
            break;
        }
        m->log->info("\n");
    }
}

// Resolve all objects in the xref table. If this triggers a xref table reconstruction abort and
// return false. Otherwise return true.
bool
QPDF::resolveXRefTable()
{
    bool may_change = !m->reconstructed_xref;
    for (auto& iter: m->xref_table) {
        if (isUnresolved(iter.first)) {
            resolve(iter.first);
            if (may_change && m->reconstructed_xref) {
                return false;
            }
        }
    }
    return true;
}

// Ensure all objects in the pdf file, including those in indirect references, appear in the object
// cache.
void
QPDF::fixDanglingReferences(bool force)
{
    if (m->fixed_dangling_refs) {
        return;
    }
    if (!resolveXRefTable()) {
        QTC::TC("qpdf", "QPDF fix dangling triggered xref reconstruction");
        resolveXRefTable();
    }
    m->fixed_dangling_refs = true;
}

size_t
QPDF::getObjectCount()
{
    // This method returns the next available indirect object number. makeIndirectObject uses it for
    // this purpose. After fixDanglingReferences is called, all objects in the xref table will also
    // be in obj_cache.
    fixDanglingReferences();
    QPDFObjGen og;
    if (!m->obj_cache.empty()) {
        og = (*(m->obj_cache.rbegin())).first;
    }
    return toS(og.getObj());
}

std::vector<QPDFObjectHandle>
QPDF::getAllObjects()
{
    // After fixDanglingReferences is called, all objects are in the object cache.
    fixDanglingReferences();
    std::vector<QPDFObjectHandle> result;
    for (auto const& iter: m->obj_cache) {
        result.push_back(newIndirect(iter.first, iter.second.object));
    }
    return result;
}

void
QPDF::setLastObjectDescription(std::string const& description, QPDFObjGen const& og)
{
    m->last_object_description.clear();
    if (!description.empty()) {
        m->last_object_description += description;
        if (og.isIndirect()) {
            m->last_object_description += ": ";
        }
    }
    if (og.isIndirect()) {
        m->last_object_description += "object " + og.unparse(' ');
    }
}

QPDFObjectHandle
QPDF::readTrailer()
{
    qpdf_offset_t offset = m->file->tell();
    bool empty = false;
    auto object = QPDFParser(m->file, "trailer", m->tokenizer, nullptr, this).parse(empty, false);
    if (empty) {
        // Nothing in the PDF spec appears to allow empty objects, but they have been encountered in
        // actual PDF files and Adobe Reader appears to ignore them.
        warn(damagedPDF("trailer", "empty object treated as null"));
    } else if (object.isDictionary() && readToken(m->file).isWord("stream")) {
        warn(damagedPDF("trailer", m->file->tell(), "stream keyword found in trailer"));
    }
    // Override last_offset so that it points to the beginning of the object we just read
    m->file->setLastOffset(offset);
    return object;
}

QPDFObjectHandle
QPDF::readObject(std::string const& description, QPDFObjGen og)
{
    setLastObjectDescription(description, og);
    qpdf_offset_t offset = m->file->tell();
    bool empty = false;

    StringDecrypter decrypter{this, og};
    StringDecrypter* decrypter_ptr = m->encp->encrypted ? &decrypter : nullptr;
    auto object = QPDFParser(m->file, m->last_object_description, m->tokenizer, decrypter_ptr, this)
                      .parse(empty, false);
    if (empty) {
        // Nothing in the PDF spec appears to allow empty objects, but they have been encountered in
        // actual PDF files and Adobe Reader appears to ignore them.
        warn(damagedPDF(m->file, m->file->getLastOffset(), "empty object treated as null"));
        return object;
    }
    auto token = readToken(m->file);
    if (object.isDictionary() && token.isWord("stream")) {
        readStream(object, og, offset);
        token = readToken(m->file);
    }
    if (!token.isWord("endobj")) {
        QTC::TC("qpdf", "QPDF err expected endobj");
        warn(damagedPDF("expected endobj"));
    }
    return object;
}

// After reading stream dictionary and stream keyword, read rest of stream.
void
QPDF::readStream(QPDFObjectHandle& object, QPDFObjGen og, qpdf_offset_t offset)
{
    validateStreamLineEnd(object, og, offset);

    // Must get offset before accessing any additional objects since resolving a previously
    // unresolved indirect object will change file position.
    qpdf_offset_t stream_offset = m->file->tell();
    size_t length = 0;

    try {
        auto length_obj = object.getKey("/Length");

        if (!length_obj.isInteger()) {
            if (length_obj.isNull()) {
                QTC::TC("qpdf", "QPDF stream without length");
                throw damagedPDF(offset, "stream dictionary lacks /Length key");
            }
            QTC::TC("qpdf", "QPDF stream length not integer");
            throw damagedPDF(offset, "/Length key in stream dictionary is not an integer");
        }

        length = toS(length_obj.getUIntValue());
        // Seek in two steps to avoid potential integer overflow
        m->file->seek(stream_offset, SEEK_SET);
        m->file->seek(toO(length), SEEK_CUR);
        if (!readToken(m->file).isWord("endstream")) {
            QTC::TC("qpdf", "QPDF missing endstream");
            throw damagedPDF("expected endstream");
        }
    } catch (QPDFExc& e) {
        if (m->attempt_recovery) {
            warn(e);
            length = recoverStreamLength(m->file, og, stream_offset);
        } else {
            throw;
        }
    }
    object = newIndirect(og, QPDF_Stream::create(this, og, object, stream_offset, length));
}

void
QPDF::validateStreamLineEnd(QPDFObjectHandle& object, QPDFObjGen og, qpdf_offset_t offset)
{
    // The PDF specification states that the word "stream" should be followed by either a carriage
    // return and a newline or by a newline alone.  It specifically disallowed following it by a
    // carriage return alone since, in that case, there would be no way to tell whether the NL in a
    // CR NL sequence was part of the stream data.  However, some readers, including Adobe reader,
    // accept a carriage return by itself when followed by a non-newline character, so that's what
    // we do here. We have also seen files that have extraneous whitespace between the stream
    // keyword and the newline.
    while (true) {
        char ch;
        if (m->file->read(&ch, 1) == 0) {
            // A premature EOF here will result in some other problem that will get reported at
            // another time.
            return;
        }
        if (ch == '\n') {
            // ready to read stream data
            QTC::TC("qpdf", "QPDF stream with NL only");
            return;
        }
        if (ch == '\r') {
            // Read another character
            if (m->file->read(&ch, 1) != 0) {
                if (ch == '\n') {
                    // Ready to read stream data
                    QTC::TC("qpdf", "QPDF stream with CRNL");
                } else {
                    // Treat the \r by itself as the whitespace after endstream and start reading
                    // stream data in spite of not having seen a newline.
                    QTC::TC("qpdf", "QPDF stream with CR only");
                    m->file->unreadCh(ch);
                    warn(damagedPDF(
                        m->file->tell(), "stream keyword followed by carriage return only"));
                }
            }
            return;
        }
        if (!QUtil::is_space(ch)) {
            QTC::TC("qpdf", "QPDF stream without newline");
            m->file->unreadCh(ch);
            warn(damagedPDF(
                m->file->tell(), "stream keyword not followed by proper line terminator"));
            return;
        }
        warn(damagedPDF(m->file->tell(), "stream keyword followed by extraneous whitespace"));
    }
}

QPDFObjectHandle
QPDF::readObjectInStream(std::shared_ptr<InputSource>& input, int obj)
{
    m->last_object_description.erase(7); // last_object_description starts with "object "
    m->last_object_description += std::to_string(obj);
    m->last_object_description += " 0";

    bool empty = false;
    auto object = QPDFParser(input, m->last_object_description, m->tokenizer, nullptr, this)
                      .parse(empty, false);
    if (empty) {
        // Nothing in the PDF spec appears to allow empty objects, but they have been encountered in
        // actual PDF files and Adobe Reader appears to ignore them.
        warn(damagedPDF(input, input->getLastOffset(), "empty object treated as null"));
    }
    return object;
}

bool
QPDF::findEndstream()
{
    // Find endstream or endobj. Position the input at that token.
    auto t = readToken(m->file, 20);
    if (t.isWord("endobj") || t.isWord("endstream")) {
        m->file->seek(m->file->getLastOffset(), SEEK_SET);
        return true;
    }
    return false;
}

size_t
QPDF::recoverStreamLength(
    std::shared_ptr<InputSource> input, QPDFObjGen const& og, qpdf_offset_t stream_offset)
{
    // Try to reconstruct stream length by looking for endstream or endobj
    warn(damagedPDF(input, stream_offset, "attempting to recover stream length"));

    PatternFinder ef(*this, &QPDF::findEndstream);
    size_t length = 0;
    if (m->file->findFirst("end", stream_offset, 0, ef)) {
        length = toS(m->file->tell() - stream_offset);
        // Reread endstream but, if it was endobj, don't skip that.
        QPDFTokenizer::Token t = readToken(m->file);
        if (t.getValue() == "endobj") {
            m->file->seek(m->file->getLastOffset(), SEEK_SET);
        }
    }

    if (length) {
        qpdf_offset_t this_obj_offset = 0;
        QPDFObjGen this_og;

        // Make sure this is inside this object
        for (auto const& iter: m->xref_table) {
            QPDFXRefEntry const& entry = iter.second;
            if (entry.getType() == 1) {
                qpdf_offset_t obj_offset = entry.getOffset();
                if ((obj_offset > stream_offset) &&
                    ((this_obj_offset == 0) || (this_obj_offset > obj_offset))) {
                    this_obj_offset = obj_offset;
                    this_og = iter.first;
                }
            }
        }
        if (this_obj_offset && (this_og == og)) {
            // Well, we found endstream\nendobj within the space allowed for this object, so we're
            // probably in good shape.
        } else {
            QTC::TC("qpdf", "QPDF found wrong endstream in recovery");
        }
    }

    if (length == 0) {
        warn(damagedPDF(
            input, stream_offset, "unable to recover stream data; treating stream as empty"));
    } else {
        warn(
            damagedPDF(input, stream_offset, "recovered stream length: " + std::to_string(length)));
    }

    QTC::TC("qpdf", "QPDF recovered stream length");
    return length;
}

QPDFTokenizer::Token
QPDF::readToken(std::shared_ptr<InputSource> input, size_t max_len)
{
    return m->tokenizer.readToken(input, m->last_object_description, true, max_len);
}

QPDFObjectHandle
QPDF::readObjectAtOffset(
    bool try_recovery,
    qpdf_offset_t offset,
    std::string const& description,
    QPDFObjGen exp_og,
    QPDFObjGen& og,
    bool skip_cache_if_in_xref)
{
    bool check_og = true;
    if (exp_og.getObj() == 0) {
        // This method uses an expect object ID of 0 to indicate that we don't know or don't care
        // what the actual object ID is at this offset. This is true when we read the xref stream
        // and linearization hint streams. In this case, we don't verify the expect object
        // ID/generation against what was read from the file. There is also no reason to attempt
        // xref recovery if we get a failure in this case since the read attempt was not triggered
        // by an xref lookup.
        check_og = false;
        try_recovery = false;
    }
    setLastObjectDescription(description, exp_og);

    if (!m->attempt_recovery) {
        try_recovery = false;
    }

    // Special case: if offset is 0, just return null.  Some PDF writers, in particular
    // "Mac OS X 10.7.5 Quartz PDFContext", may store deleted objects in the xref table as
    // "0000000000 00000 n", which is not correct, but it won't hurt anything for us to ignore
    // these.
    if (offset == 0) {
        QTC::TC("qpdf", "QPDF bogus 0 offset", 0);
        warn(damagedPDF(0, "object has offset 0"));
        return QPDFObjectHandle::newNull();
    }

    m->file->seek(offset, SEEK_SET);

    QPDFTokenizer::Token tobjid = readToken(m->file);
    QPDFTokenizer::Token tgen = readToken(m->file);
    QPDFTokenizer::Token tobj = readToken(m->file);

    bool objidok = tobjid.isInteger();
    bool genok = tgen.isInteger();
    bool objok = tobj.isWord("obj");

    QTC::TC("qpdf", "QPDF check objid", objidok ? 1 : 0);
    QTC::TC("qpdf", "QPDF check generation", genok ? 1 : 0);
    QTC::TC("qpdf", "QPDF check obj", objok ? 1 : 0);

    try {
        if (!(objidok && genok && objok)) {
            QTC::TC("qpdf", "QPDF expected n n obj");
            throw damagedPDF(offset, "expected n n obj");
        }
        int objid = QUtil::string_to_int(tobjid.getValue().c_str());
        int generation = QUtil::string_to_int(tgen.getValue().c_str());
        og = QPDFObjGen(objid, generation);
        if (objid == 0) {
            QTC::TC("qpdf", "QPDF object id 0");
            throw damagedPDF(offset, "object with ID 0");
        }
        if (check_og && (exp_og != og)) {
            QTC::TC("qpdf", "QPDF err wrong objid/generation");
            QPDFExc e = damagedPDF(offset, "expected " + exp_og.unparse(' ') + " obj");
            if (try_recovery) {
                // Will be retried below
                throw e;
            } else {
                // We can try reading the object anyway even if the ID doesn't match.
                warn(e);
            }
        }
    } catch (QPDFExc& e) {
        if (try_recovery) {
            // Try again after reconstructing xref table
            reconstruct_xref(e);
            if (m->xref_table.count(exp_og) && (m->xref_table[exp_og].getType() == 1)) {
                qpdf_offset_t new_offset = m->xref_table[exp_og].getOffset();
                QPDFObjectHandle result =
                    readObjectAtOffset(false, new_offset, description, exp_og, og, false);
                QTC::TC("qpdf", "QPDF recovered in readObjectAtOffset");
                return result;
            } else {
                QTC::TC("qpdf", "QPDF object gone after xref reconstruction");
                warn(damagedPDF(
                    "",
                    0,
                    ("object " + exp_og.unparse(' ') +
                     " not found in file after regenerating cross reference "
                     "table")));
                return QPDFObjectHandle::newNull();
            }
        } else {
            throw;
        }
    }

    QPDFObjectHandle oh = readObject(description, og);

    if (isUnresolved(og)) {
        // Store the object in the cache here so it gets cached whether we first know the offset or
        // whether we first know the object ID and generation (in which we case we would get here
        // through resolve).

        // Determine the end offset of this object before and after white space.  We use these
        // numbers to validate linearization hint tables.  Offsets and lengths of objects may imply
        // the end of an object to be anywhere between these values.
        qpdf_offset_t end_before_space = m->file->tell();

        // skip over spaces
        while (true) {
            char ch;
            if (m->file->read(&ch, 1)) {
                if (!isspace(static_cast<unsigned char>(ch))) {
                    m->file->seek(-1, SEEK_CUR);
                    break;
                }
            } else {
                throw damagedPDF(m->file->tell(), "EOF after endobj");
            }
        }
        qpdf_offset_t end_after_space = m->file->tell();
        if (skip_cache_if_in_xref && m->xref_table.count(og)) {
            // Ordinarily, an object gets read here when resolved through xref table or stream. In
            // the special case of the xref stream and linearization hint tables, the offset comes
            // from another source. For the specific case of xref streams, the xref stream is read
            // and loaded into the object cache very early in parsing. Ordinarily, when a file is
            // updated by appending, items inserted into the xref table in later updates take
            // precedence over earlier items. In the special case of reusing the object number
            // previously used as the xref stream, we have the following order of events:
            //
            // * reused object gets loaded into the xref table
            // * old object is read here while reading xref streams
            // * original xref entry is ignored (since already in xref table)
            //
            // It is the second step that causes a problem. Even though the xref table is correct in
            // this case, the old object is already in the cache and so effectively prevails over
            // the reused object. To work around this issue, we have a special case for the xref
            // stream (via the skip_cache_if_in_xref): if the object is already in the xref stream,
            // don't cache what we read here.
            //
            // It is likely that the same bug may exist for linearization hint tables, but the
            // existing code uses end_before_space and end_after_space from the cache, so fixing
            // that would require more significant rework. The chances of a linearization hint
            // stream being reused seems smaller because the xref stream is probably the highest
            // object in the file and the linearization hint stream would be some random place in
            // the middle, so I'm leaving that bug unfixed for now. If the bug were to be fixed, we
            // could use !check_og in place of skip_cache_if_in_xref.
            QTC::TC("qpdf", "QPDF skipping cache for known unchecked object");
        } else {
            updateCache(og, oh.getObj(), end_before_space, end_after_space);
        }
    }

    return oh;
}

void
QPDF::resolve(QPDFObjGen og)
{
    if (!isUnresolved(og)) {
        return;
    }

    if (m->resolving.count(og)) {
        // This can happen if an object references itself directly or indirectly in some key that
        // has to be resolved during object parsing, such as stream length.
        QTC::TC("qpdf", "QPDF recursion loop in resolve");
        warn(damagedPDF("", "loop detected resolving object " + og.unparse(' ')));
        updateCache(og, QPDF_Null::create(), -1, -1);
        return;
    }
    ResolveRecorder rr(this, og);

    if (m->xref_table.count(og) != 0) {
        QPDFXRefEntry const& entry = m->xref_table[og];
        try {
            switch (entry.getType()) {
            case 1:
                {
                    qpdf_offset_t offset = entry.getOffset();
                    // Object stored in cache by readObjectAtOffset
                    QPDFObjGen a_og;
                    QPDFObjectHandle oh = readObjectAtOffset(true, offset, "", og, a_og, false);
                }
                break;

            case 2:
                resolveObjectsInStream(entry.getObjStreamNumber());
                break;

            default:
                throw damagedPDF(
                    "", 0, ("object " + og.unparse('/') + " has unexpected xref entry type"));
            }
        } catch (QPDFExc& e) {
            warn(e);
        } catch (std::exception& e) {
            warn(damagedPDF(
                "", 0, ("object " + og.unparse('/') + ": error reading object: " + e.what())));
        }
    }

    if (isUnresolved(og)) {
        // PDF spec says unknown objects resolve to the null object.
        QTC::TC("qpdf", "QPDF resolve failure to null");
        updateCache(og, QPDF_Null::create(), -1, -1);
    }

    auto result(m->obj_cache[og].object);
    result->setDefaultDescription(this, og);
}

void
QPDF::resolveObjectsInStream(int obj_stream_number)
{
    if (m->resolved_object_streams.count(obj_stream_number)) {
        return;
    }
    m->resolved_object_streams.insert(obj_stream_number);
    // Force resolution of object stream
    QPDFObjectHandle obj_stream = getObjectByID(obj_stream_number, 0);
    if (!obj_stream.isStream()) {
        throw damagedPDF(
            "supposed object stream " + std::to_string(obj_stream_number) + " is not a stream");
    }

    // For linearization data in the object, use the data from the object stream for the objects in
    // the stream.
    QPDFObjGen stream_og(obj_stream_number, 0);
    qpdf_offset_t end_before_space = m->obj_cache[stream_og].end_before_space;
    qpdf_offset_t end_after_space = m->obj_cache[stream_og].end_after_space;

    QPDFObjectHandle dict = obj_stream.getDict();
    if (!dict.isDictionaryOfType("/ObjStm")) {
        QTC::TC("qpdf", "QPDF ERR object stream with wrong type");
        warn(damagedPDF(
            "supposed object stream " + std::to_string(obj_stream_number) + " has wrong type"));
    }

    if (!(dict.getKey("/N").isInteger() && dict.getKey("/First").isInteger())) {
        throw damagedPDF(
            ("object stream " + std::to_string(obj_stream_number) + " has incorrect keys"));
    }

    int n = dict.getKey("/N").getIntValueAsInt();
    int first = dict.getKey("/First").getIntValueAsInt();

    std::map<int, int> offsets;

    std::shared_ptr<Buffer> bp = obj_stream.getStreamData(qpdf_dl_specialized);
    auto input = std::shared_ptr<InputSource>(
        // line-break
        new BufferInputSource(
            (m->file->getName() + " object stream " + std::to_string(obj_stream_number)),
            bp.get()));

    for (int i = 0; i < n; ++i) {
        QPDFTokenizer::Token tnum = readToken(input);
        QPDFTokenizer::Token toffset = readToken(input);
        if (!(tnum.isInteger() && toffset.isInteger())) {
            throw damagedPDF(
                input,
                m->last_object_description,
                input->getLastOffset(),
                "expected integer in object stream header");
        }

        int num = QUtil::string_to_int(tnum.getValue().c_str());
        long long offset = QUtil::string_to_int(toffset.getValue().c_str());
        offsets[num] = toI(offset + first);
    }

    // To avoid having to read the object stream multiple times, store all objects that would be
    // found here in the cache.  Remember that some objects stored here might have been overridden
    // by new objects appended to the file, so it is necessary to recheck the xref table and only
    // cache what would actually be resolved here.
    m->last_object_description.clear();
    m->last_object_description += "object ";
    for (auto const& iter: offsets) {
        QPDFObjGen og(iter.first, 0);
        QPDFXRefEntry const& entry = m->xref_table[og];
        if ((entry.getType() == 2) && (entry.getObjStreamNumber() == obj_stream_number)) {
            int offset = iter.second;
            input->seek(offset, SEEK_SET);
            QPDFObjectHandle oh = readObjectInStream(input, iter.first);
            updateCache(og, oh.getObj(), end_before_space, end_after_space);
        } else {
            QTC::TC("qpdf", "QPDF not caching overridden objstm object");
        }
    }
}

QPDFObjectHandle
QPDF::newIndirect(QPDFObjGen const& og, std::shared_ptr<QPDFObject> const& obj)
{
    obj->setDefaultDescription(this, og);
    return {obj};
}

void
QPDF::updateCache(
    QPDFObjGen const& og,
    std::shared_ptr<QPDFObject> const& object,
    qpdf_offset_t end_before_space,
    qpdf_offset_t end_after_space)
{
    object->setObjGen(this, og);
    if (isCached(og)) {
        auto& cache = m->obj_cache[og];
        cache.object->assign(object);
        cache.end_before_space = end_before_space;
        cache.end_after_space = end_after_space;
    } else {
        m->obj_cache[og] = ObjCache(object, end_before_space, end_after_space);
    }
}

bool
QPDF::isCached(QPDFObjGen const& og)
{
    return m->obj_cache.count(og) != 0;
}

bool
QPDF::isUnresolved(QPDFObjGen const& og)
{
    return !isCached(og) || m->obj_cache[og].object->isUnresolved();
}

QPDFObjGen
QPDF::nextObjGen()
{
    int max_objid = toI(getObjectCount());
    if (max_objid == std::numeric_limits<int>::max()) {
        throw std::range_error("max object id is too high to create new objects");
    }
    return QPDFObjGen(max_objid + 1, 0);
}

QPDFObjectHandle
QPDF::makeIndirectFromQPDFObject(std::shared_ptr<QPDFObject> const& obj)
{
    QPDFObjGen next{nextObjGen()};
    m->obj_cache[next] = ObjCache(obj, -1, -1);
    return newIndirect(next, m->obj_cache[next].object);
}

QPDFObjectHandle
QPDF::makeIndirectObject(QPDFObjectHandle oh)
{
    if (!oh.isInitialized()) {
        throw std::logic_error("attempted to make an uninitialized QPDFObjectHandle indirect");
    }
    return makeIndirectFromQPDFObject(oh.getObj());
}

QPDFObjectHandle
QPDF::newReserved()
{
    return makeIndirectFromQPDFObject(QPDF_Reserved::create());
}

QPDFObjectHandle
QPDF::newIndirectNull()
{
    return makeIndirectFromQPDFObject(QPDF_Null::create());
}

QPDFObjectHandle
QPDF::newStream()
{
    return makeIndirectFromQPDFObject(
        QPDF_Stream::create(this, nextObjGen(), QPDFObjectHandle::newDictionary(), 0, 0));
}

QPDFObjectHandle
QPDF::newStream(std::shared_ptr<Buffer> data)
{
    auto result = newStream();
    result.replaceStreamData(data, QPDFObjectHandle::newNull(), QPDFObjectHandle::newNull());
    return result;
}

QPDFObjectHandle
QPDF::newStream(std::string const& data)
{
    auto result = newStream();
    result.replaceStreamData(data, QPDFObjectHandle::newNull(), QPDFObjectHandle::newNull());
    return result;
}

QPDFObjectHandle
QPDF::reserveObjectIfNotExists(QPDFObjGen const& og)
{
    if (!isCached(og) && m->xref_table.count(og) == 0) {
        updateCache(og, QPDF_Reserved::create(), -1, -1);
        return newIndirect(og, m->obj_cache[og].object);
    } else {
        return getObject(og);
    }
}

QPDFObjectHandle
QPDF::reserveStream(QPDFObjGen const& og)
{
    return {QPDF_Stream::create(this, og, QPDFObjectHandle::newDictionary(), 0, 0)};
}

QPDFObjectHandle
QPDF::getObject(QPDFObjGen const& og)
{
    // This method is called by the parser and therefore must not resolve any objects.
    if (!isCached(og)) {
        m->obj_cache[og] = ObjCache(QPDF_Unresolved::create(this, og), -1, -1);
    }
    return newIndirect(og, m->obj_cache[og].object);
}

QPDFObjectHandle
QPDF::getObject(int objid, int generation)
{
    return getObject(QPDFObjGen(objid, generation));
}

QPDFObjectHandle
QPDF::getObjectByObjGen(QPDFObjGen const& og)
{
    return getObject(og);
}

QPDFObjectHandle
QPDF::getObjectByID(int objid, int generation)
{
    return getObject(QPDFObjGen(objid, generation));
}

void
QPDF::replaceObject(int objid, int generation, QPDFObjectHandle oh)
{
    replaceObject(QPDFObjGen(objid, generation), oh);
}

void
QPDF::replaceObject(QPDFObjGen const& og, QPDFObjectHandle oh)
{
    if (oh.isIndirect() || !oh.isInitialized()) {
        QTC::TC("qpdf", "QPDF replaceObject called with indirect object");
        throw std::logic_error("QPDF::replaceObject called with indirect object handle");
    }
    updateCache(og, oh.getObj(), -1, -1);
}

void
QPDF::replaceReserved(QPDFObjectHandle reserved, QPDFObjectHandle replacement)
{
    QTC::TC("qpdf", "QPDF replaceReserved");
    auto tc = reserved.getTypeCode();
    if (!(tc == ::ot_reserved || tc == ::ot_null)) {
        throw std::logic_error("replaceReserved called with non-reserved object");
    }
    replaceObject(reserved.getObjGen(), replacement);
}

QPDFObjectHandle
QPDF::copyForeignObject(QPDFObjectHandle foreign)
{
    // Here's an explanation of what's going on here.
    //
    // A QPDFObjectHandle that is an indirect object has an owning QPDF. The object ID and
    // generation refers to an object in the owning QPDF. When we copy the QPDFObjectHandle from a
    // foreign QPDF into the local QPDF, we have to replace all indirect object references with
    // references to the corresponding object in the local file.
    //
    // To do this, we maintain mappings from foreign object IDs to local object IDs for each foreign
    // QPDF that we are copying from. The mapping is stored in an ObjCopier, which contains a
    // mapping from the foreign ObjGen to the local QPDFObjectHandle.
    //
    // To copy, we do a deep traversal of the foreign object with loop detection to discover all
    // indirect objects that are encountered, stopping at page boundaries. Whenever we encounter an
    // indirect object, we check to see if we have already created a local copy of it. If not, we
    // allocate a "reserved" object (or, for a stream, just a new stream) and store in the map the
    // mapping from the foreign object ID to the new object. While we
    // do this, we keep a list of objects to copy.
    //
    // Once we are done with the traversal, we copy all the objects that we need to copy. However,
    // the copies will contain indirect object IDs that refer to objects in the foreign file. We
    // need to replace them with references to objects in the local file. This is what
    // replaceForeignIndirectObjects does. Once we have created a copy of the foreign object with
    // all the indirect references replaced with new ones in the local context, we can replace the
    // local reserved object with the copy. This mechanism allows us to copy objects with circular
    // references in any order.

    // For streams, rather than copying the objects, we set up the stream data to pull from the
    // original stream by using a stream data provider. This is done in a manner that doesn't
    // require the original QPDF object but may require the original source of the stream data with
    // special handling for immediate_copy_from. This logic is also in
    // replaceForeignIndirectObjects.

    // Note that we explicitly allow use of copyForeignObject on page objects. It is a documented
    // use case to copy pages this way if the intention is to not update the pages tree.
    if (!foreign.isIndirect()) {
        QTC::TC("qpdf", "QPDF copyForeign direct");
        throw std::logic_error("QPDF::copyForeign called with direct object handle");
    }
    QPDF& other = foreign.getQPDF();
    if (&other == this) {
        QTC::TC("qpdf", "QPDF copyForeign not foreign");
        throw std::logic_error("QPDF::copyForeign called with object from this QPDF");
    }

    ObjCopier& obj_copier = m->object_copiers[other.m->unique_id];
    if (!obj_copier.visiting.empty()) {
        throw std::logic_error("obj_copier.visiting is not empty"
                               " at the beginning of copyForeignObject");
    }

    // Make sure we have an object in this file for every referenced object in the old file.
    // obj_copier.object_map maps foreign QPDFObjGen to local objects.  For everything new that we
    // have to copy, the local object will be a reservation, unless it is a stream, in which case
    // the local object will already be a stream.
    reserveObjects(foreign, obj_copier, true);

    if (!obj_copier.visiting.empty()) {
        throw std::logic_error("obj_copier.visiting is not empty after reserving objects");
    }

    // Copy any new objects and replace the reservations.
    for (auto& to_copy: obj_copier.to_copy) {
        QPDFObjectHandle copy = replaceForeignIndirectObjects(to_copy, obj_copier, true);
        if (!to_copy.isStream()) {
            QPDFObjGen og(to_copy.getObjGen());
            replaceReserved(obj_copier.object_map[og], copy);
        }
    }
    obj_copier.to_copy.clear();

    auto og = foreign.getObjGen();
    if (!obj_copier.object_map.count(og)) {
        warn(damagedPDF("unexpected reference to /Pages object while copying foreign object; "
                        "replacing with null"));
        return QPDFObjectHandle::newNull();
    }
    return obj_copier.object_map[foreign.getObjGen()];
}

void
QPDF::reserveObjects(QPDFObjectHandle foreign, ObjCopier& obj_copier, bool top)
{
    auto foreign_tc = foreign.getTypeCode();
    if (foreign_tc == ::ot_reserved) {
        throw std::logic_error("QPDF: attempting to copy a foreign reserved object");
    }

    if (foreign.isPagesObject()) {
        QTC::TC("qpdf", "QPDF not copying pages object");
        return;
    }

    if (foreign.isIndirect()) {
        QPDFObjGen foreign_og(foreign.getObjGen());
        if (!obj_copier.visiting.add(foreign_og)) {
            QTC::TC("qpdf", "QPDF loop reserving objects");
            return;
        }
        if (obj_copier.object_map.count(foreign_og) > 0) {
            QTC::TC("qpdf", "QPDF already reserved object");
            if (!(top && foreign.isPageObject() && obj_copier.object_map[foreign_og].isNull())) {
                obj_copier.visiting.erase(foreign);
                return;
            }
        } else {
            QTC::TC("qpdf", "QPDF copy indirect");
            obj_copier.object_map[foreign_og] =
                foreign.isStream() ? newStream() : newIndirectNull();
            if ((!top) && foreign.isPageObject()) {
                QTC::TC("qpdf", "QPDF not crossing page boundary");
                obj_copier.visiting.erase(foreign_og);
                return;
            }
        }
        obj_copier.to_copy.push_back(foreign);
    }

    if (foreign_tc == ::ot_array) {
        QTC::TC("qpdf", "QPDF reserve array");
        int n = foreign.getArrayNItems();
        for (int i = 0; i < n; ++i) {
            reserveObjects(foreign.getArrayItem(i), obj_copier, false);
        }
    } else if (foreign_tc == ::ot_dictionary) {
        QTC::TC("qpdf", "QPDF reserve dictionary");
        for (auto const& key: foreign.getKeys()) {
            reserveObjects(foreign.getKey(key), obj_copier, false);
        }
    } else if (foreign_tc == ::ot_stream) {
        QTC::TC("qpdf", "QPDF reserve stream");
        reserveObjects(foreign.getDict(), obj_copier, false);
    }

    obj_copier.visiting.erase(foreign);
}

QPDFObjectHandle
QPDF::replaceForeignIndirectObjects(QPDFObjectHandle foreign, ObjCopier& obj_copier, bool top)
{
    auto foreign_tc = foreign.getTypeCode();
    QPDFObjectHandle result;
    if ((!top) && foreign.isIndirect()) {
        QTC::TC("qpdf", "QPDF replace indirect");
        auto mapping = obj_copier.object_map.find(foreign.getObjGen());
        if (mapping == obj_copier.object_map.end()) {
            // This case would occur if this is a reference to a Pages object that we didn't
            // traverse into.
            QTC::TC("qpdf", "QPDF replace foreign indirect with null");
            result = QPDFObjectHandle::newNull();
        } else {
            result = mapping->second;
        }
    } else if (foreign_tc == ::ot_array) {
        QTC::TC("qpdf", "QPDF replace array");
        result = QPDFObjectHandle::newArray();
        int n = foreign.getArrayNItems();
        for (int i = 0; i < n; ++i) {
            result.appendItem(
                // line-break
                replaceForeignIndirectObjects(foreign.getArrayItem(i), obj_copier, false));
        }
    } else if (foreign_tc == ::ot_dictionary) {
        QTC::TC("qpdf", "QPDF replace dictionary");
        result = QPDFObjectHandle::newDictionary();
        std::set<std::string> keys = foreign.getKeys();
        for (auto const& iter: keys) {
            result.replaceKey(
                iter, replaceForeignIndirectObjects(foreign.getKey(iter), obj_copier, false));
        }
    } else if (foreign_tc == ::ot_stream) {
        QTC::TC("qpdf", "QPDF replace stream");
        result = obj_copier.object_map[foreign.getObjGen()];
        result.assertStream();
        QPDFObjectHandle dict = result.getDict();
        QPDFObjectHandle old_dict = foreign.getDict();
        std::set<std::string> keys = old_dict.getKeys();
        for (auto const& iter: keys) {
            dict.replaceKey(
                iter, replaceForeignIndirectObjects(old_dict.getKey(iter), obj_copier, false));
        }
        copyStreamData(result, foreign);
    } else {
        foreign.assertScalar();
        result = foreign;
        result.makeDirect();
    }

    if (top && (!result.isStream()) && result.isIndirect()) {
        throw std::logic_error("replacement for foreign object is indirect");
    }

    return result;
}

void
QPDF::copyStreamData(QPDFObjectHandle result, QPDFObjectHandle foreign)
{
    // This method was originally written for copying foreign streams, but it is used by
    // QPDFObjectHandle to copy streams from the same QPDF object as well.

    QPDFObjectHandle dict = result.getDict();
    QPDFObjectHandle old_dict = foreign.getDict();
    if (m->copied_stream_data_provider == nullptr) {
        m->copied_stream_data_provider = new CopiedStreamDataProvider(*this);
        m->copied_streams =
            std::shared_ptr<QPDFObjectHandle::StreamDataProvider>(m->copied_stream_data_provider);
    }
    QPDFObjGen local_og(result.getObjGen());
    // Copy information from the foreign stream so we can pipe its data later without keeping the
    // original QPDF object around.

    QPDF& foreign_stream_qpdf =
        foreign.getQPDF("unable to retrieve owning qpdf from foreign stream");

    auto stream = foreign.getObjectPtr()->as<QPDF_Stream>();
    if (stream == nullptr) {
        throw std::logic_error("unable to retrieve underlying"
                               " stream object from foreign stream");
    }
    std::shared_ptr<Buffer> stream_buffer = stream->getStreamDataBuffer();
    if ((foreign_stream_qpdf.m->immediate_copy_from) && (stream_buffer == nullptr)) {
        // Pull the stream data into a buffer before attempting the copy operation. Do it on the
        // source stream so that if the source stream is copied multiple times, we don't have to
        // keep duplicating the memory.
        QTC::TC("qpdf", "QPDF immediate copy stream data");
        foreign.replaceStreamData(
            foreign.getRawStreamData(),
            old_dict.getKey("/Filter"),
            old_dict.getKey("/DecodeParms"));
        stream_buffer = stream->getStreamDataBuffer();
    }
    std::shared_ptr<QPDFObjectHandle::StreamDataProvider> stream_provider =
        stream->getStreamDataProvider();
    if (stream_buffer.get()) {
        QTC::TC("qpdf", "QPDF copy foreign stream with buffer");
        result.replaceStreamData(
            stream_buffer, dict.getKey("/Filter"), dict.getKey("/DecodeParms"));
    } else if (stream_provider.get()) {
        // In this case, the remote stream's QPDF must stay in scope.
        QTC::TC("qpdf", "QPDF copy foreign stream with provider");
        m->copied_stream_data_provider->registerForeignStream(local_og, foreign);
        result.replaceStreamData(
            m->copied_streams, dict.getKey("/Filter"), dict.getKey("/DecodeParms"));
    } else {
        auto foreign_stream_data = std::make_shared<ForeignStreamData>(
            foreign_stream_qpdf.m->encp,
            foreign_stream_qpdf.m->file,
            foreign.getObjGen(),
            stream->getParsedOffset(),
            stream->getLength(),
            dict);
        m->copied_stream_data_provider->registerForeignStream(local_og, foreign_stream_data);
        result.replaceStreamData(
            m->copied_streams, dict.getKey("/Filter"), dict.getKey("/DecodeParms"));
    }
}

void
QPDF::swapObjects(int objid1, int generation1, int objid2, int generation2)
{
    swapObjects(QPDFObjGen(objid1, generation1), QPDFObjGen(objid2, generation2));
}

void
QPDF::swapObjects(QPDFObjGen const& og1, QPDFObjGen const& og2)
{
    // Force objects to be read from the input source if needed, then swap them in the cache.
    resolve(og1);
    resolve(og2);
    m->obj_cache[og1].object->swapWith(m->obj_cache[og2].object);
}

unsigned long long
QPDF::getUniqueId() const
{
    return m->unique_id;
}

std::string
QPDF::getFilename() const
{
    return m->file->getName();
}

PDFVersion
QPDF::getVersionAsPDFVersion()
{
    int major = 1;
    int minor = 3;
    int extension_level = getExtensionLevel();

    std::regex v("^[[:space:]]*([0-9]+)\\.([0-9]+)");
    std::smatch match;
    if (std::regex_search(m->pdf_version, match, v)) {
        major = QUtil::string_to_int(match[1].str().c_str());
        minor = QUtil::string_to_int(match[2].str().c_str());
    }

    return {major, minor, extension_level};
}

std::string
QPDF::getPDFVersion() const
{
    return m->pdf_version;
}

int
QPDF::getExtensionLevel()
{
    int result = 0;
    QPDFObjectHandle obj = getRoot();
    if (obj.hasKey("/Extensions")) {
        obj = obj.getKey("/Extensions");
        if (obj.isDictionary() && obj.hasKey("/ADBE")) {
            obj = obj.getKey("/ADBE");
            if (obj.isDictionary() && obj.hasKey("/ExtensionLevel")) {
                obj = obj.getKey("/ExtensionLevel");
                if (obj.isInteger()) {
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
    return m->trailer;
}

QPDFObjectHandle
QPDF::getRoot()
{
    QPDFObjectHandle root = m->trailer.getKey("/Root");
    if (!root.isDictionary()) {
        throw damagedPDF("", 0, "unable to find /Root dictionary");
    } else if (
        // Check_mode is an interim solution to request #810 pending a more comprehensive review of
        // the approach to more extensive checks and warning levels.
        m->check_mode && !root.getKey("/Type").isNameAndEquals("/Catalog")) {
        warn(damagedPDF("", 0, "catalog /Type entry missing or invalid"));
        root.replaceKey("/Type", "/Catalog"_qpdf);
    }
    return root;
}

std::map<QPDFObjGen, QPDFXRefEntry>
QPDF::getXRefTable()
{
    if (!m->parsed) {
        throw std::logic_error("QPDF::getXRefTable called before parsing.");
    }

    return m->xref_table;
}

void
QPDF::getObjectStreamData(std::map<int, int>& omap)
{
    for (auto const& iter: m->xref_table) {
        QPDFObjGen const& og = iter.first;
        QPDFXRefEntry const& entry = iter.second;
        if (entry.getType() == 2) {
            omap[og.getObj()] = entry.getObjStreamNumber();
        }
    }
}

std::vector<QPDFObjGen>
QPDF::getCompressibleObjGens()
{
    // Return a list of objects that are allowed to be in object streams.  Walk through the objects
    // by traversing the document from the root, including a traversal of the pages tree.  This
    // makes that objects that are on the same page are more likely to be in the same object stream,
    // which is slightly more efficient, particularly with linearized files.  This is better than
    // iterating through the xref table since it avoids preserving orphaned items.

    // Exclude encryption dictionary, if any
    QPDFObjectHandle encryption_dict = m->trailer.getKey("/Encrypt");
    QPDFObjGen encryption_dict_og = encryption_dict.getObjGen();

    QPDFObjGen::set visited;
    std::list<QPDFObjectHandle> queue;
    queue.push_front(m->trailer);
    std::vector<QPDFObjGen> result;
    while (!queue.empty()) {
        QPDFObjectHandle obj = queue.front();
        queue.pop_front();
        if (obj.isIndirect()) {
            QPDFObjGen og = obj.getObjGen();
            if (!visited.add(og)) {
                QTC::TC("qpdf", "QPDF loop detected traversing objects");
                continue;
            }
            if (og == encryption_dict_og) {
                QTC::TC("qpdf", "QPDF exclude encryption dictionary");
            } else if (!(obj.isStream() ||
                         (obj.isDictionaryOfType("/Sig") && obj.hasKey("/ByteRange") &&
                          obj.hasKey("/Contents")))) {
                result.push_back(og);
            }
        }
        if (obj.isStream()) {
            QPDFObjectHandle dict = obj.getDict();
            std::set<std::string> keys = dict.getKeys();
            for (auto iter = keys.rbegin(); iter != keys.rend(); ++iter) {
                std::string const& key = *iter;
                QPDFObjectHandle value = dict.getKey(key);
                if (key == "/Length") {
                    // omit stream lengths
                    if (value.isIndirect()) {
                        QTC::TC("qpdf", "QPDF exclude indirect length");
                    }
                } else {
                    queue.push_front(value);
                }
            }
        } else if (obj.isDictionary()) {
            std::set<std::string> keys = obj.getKeys();
            for (auto iter = keys.rbegin(); iter != keys.rend(); ++iter) {
                queue.push_front(obj.getKey(*iter));
            }
        } else if (obj.isArray()) {
            int n = obj.getArrayNItems();
            for (int i = 1; i <= n; ++i) {
                queue.push_front(obj.getArrayItem(n - i));
            }
        }
    }

    return result;
}

bool
QPDF::pipeStreamData(
    std::shared_ptr<EncryptionParameters> encp,
    std::shared_ptr<InputSource> file,
    QPDF& qpdf_for_warning,
    QPDFObjGen const& og,
    qpdf_offset_t offset,
    size_t length,
    QPDFObjectHandle stream_dict,
    Pipeline* pipeline,
    bool suppress_warnings,
    bool will_retry)
{
    std::vector<std::shared_ptr<Pipeline>> to_delete;
    if (encp->encrypted) {
        decryptStream(encp, file, qpdf_for_warning, pipeline, og, stream_dict, to_delete);
    }

    bool success = false;
    try {
        file->seek(offset, SEEK_SET);
        char buf[10240];
        while (length > 0) {
            size_t to_read = (sizeof(buf) < length ? sizeof(buf) : length);
            size_t len = file->read(buf, to_read);
            if (len == 0) {
                throw damagedPDF(
                    file, "", file->getLastOffset(), "unexpected EOF reading stream data");
            }
            length -= len;
            pipeline->write(buf, len);
        }
        pipeline->finish();
        success = true;
    } catch (QPDFExc& e) {
        if (!suppress_warnings) {
            qpdf_for_warning.warn(e);
        }
    } catch (std::exception& e) {
        if (!suppress_warnings) {
            QTC::TC("qpdf", "QPDF decoding error warning");
            qpdf_for_warning.warn(
                // line-break
                damagedPDF(
                    file,
                    "",
                    file->getLastOffset(),
                    ("error decoding stream data for object " + og.unparse(' ') + ": " +
                     e.what())));
            if (will_retry) {
                qpdf_for_warning.warn(
                    // line-break
                    damagedPDF(
                        file,
                        "",
                        file->getLastOffset(),
                        "stream will be re-processed without"
                        " filtering to avoid data loss"));
            }
        }
    }
    if (!success) {
        try {
            pipeline->finish();
        } catch (std::exception&) {
            // ignore
        }
    }
    return success;
}

bool
QPDF::pipeStreamData(
    QPDFObjGen const& og,
    qpdf_offset_t offset,
    size_t length,
    QPDFObjectHandle stream_dict,
    Pipeline* pipeline,
    bool suppress_warnings,
    bool will_retry)
{
    return pipeStreamData(
        m->encp,
        m->file,
        *this,
        og,
        offset,
        length,
        stream_dict,
        pipeline,
        suppress_warnings,
        will_retry);
}

bool
QPDF::pipeForeignStreamData(
    std::shared_ptr<ForeignStreamData> foreign,
    Pipeline* pipeline,
    bool suppress_warnings,
    bool will_retry)
{
    if (foreign->encp->encrypted) {
        QTC::TC("qpdf", "QPDF pipe foreign encrypted stream");
    }
    return pipeStreamData(
        foreign->encp,
        foreign->file,
        *this,
        foreign->foreign_og,
        foreign->offset,
        foreign->length,
        foreign->local_dict,
        pipeline,
        suppress_warnings,
        will_retry);
}

// Throw a generic exception when we lack context for something more specific. New code should not
// use this. This method exists to improve somewhat from calling assert in very old code.
void
QPDF::stopOnError(std::string const& message)
{
    throw damagedPDF("", message);
}

// Return an exception of type qpdf_e_damaged_pdf.
QPDFExc
QPDF::damagedPDF(
    std::shared_ptr<InputSource> const& input,
    std::string const& object,
    qpdf_offset_t offset,
    std::string const& message)
{
    return {qpdf_e_damaged_pdf, input->getName(), object, offset, message};
}

// Return an exception of type qpdf_e_damaged_pdf.  The object is taken from
// m->last_object_description.
QPDFExc
QPDF::damagedPDF(
    std::shared_ptr<InputSource> const& input, qpdf_offset_t offset, std::string const& message)
{
    return damagedPDF(input, m->last_object_description, offset, message);
}

// Return an exception of type qpdf_e_damaged_pdf.  The filename is taken from m->file.
QPDFExc
QPDF::damagedPDF(std::string const& object, qpdf_offset_t offset, std::string const& message)
{
    return {qpdf_e_damaged_pdf, m->file->getName(), object, offset, message};
}

// Return an exception of type qpdf_e_damaged_pdf.  The filename is taken from m->file and the
// offset from .m->file->getLastOffset().
QPDFExc
QPDF::damagedPDF(std::string const& object, std::string const& message)
{
    return damagedPDF(object, m->file->getLastOffset(), message);
}

// Return an exception of type qpdf_e_damaged_pdf. The filename is taken from m->file and the object
// from .m->last_object_description.
QPDFExc
QPDF::damagedPDF(qpdf_offset_t offset, std::string const& message)
{
    return damagedPDF(m->last_object_description, offset, message);
}

// Return an exception of type qpdf_e_damaged_pdf.  The filename is taken from m->file, the object
// from m->last_object_description and the offset from m->file->getLastOffset().
QPDFExc
QPDF::damagedPDF(std::string const& message)
{
    return damagedPDF(m->last_object_description, m->file->getLastOffset(), message);
}

bool
QPDF::everCalledGetAllPages() const
{
    return m->ever_called_get_all_pages;
}

bool
QPDF::everPushedInheritedAttributesToPages() const
{
    return m->ever_pushed_inherited_attributes_to_pages;
}

void
QPDF::removeSecurityRestrictions()
{
    auto root = getRoot();
    root.removeKey("/Perms");
    auto acroform = root.getKey("/AcroForm");
    if (acroform.isDictionary() && acroform.hasKey("/SigFlags")) {
        acroform.replaceKey("/SigFlags", QPDFObjectHandle::newInteger(0));
    }
}
