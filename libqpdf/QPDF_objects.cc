#include <qpdf/qpdf-config.h> // include first for large file support

#include <qpdf/QPDF_private.hh>

#include <array>
#include <atomic>
#include <cstring>
#include <limits>
#include <map>
#include <regex>
#include <sstream>
#include <vector>

#include <qpdf/BufferInputSource.hh>
#include <qpdf/FileInputSource.hh>
#include <qpdf/InputSource_private.hh>
#include <qpdf/OffsetInputSource.hh>
#include <qpdf/Pipeline.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/QPDFLogger.hh>
#include <qpdf/QPDFObjectHandle_private.hh>
#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QPDFParser.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/Util.hh>

using namespace qpdf;
using namespace std::literals;

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
            throw std::logic_error(
                "QPDF operation attempted on a QPDF object with no input "
                "source. QPDF operations are invalid before processFile (or "
                "another process method) or after closeInputSource");
        }
    };
} // namespace

bool
QPDF::findStartxref()
{
    if (readToken(*m->file).isWord("startxref") && readToken(*m->file).isInteger()) {
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
    m->xref_table_max_offset = end_offset;
    // Sanity check on object ids. All objects must appear in xref table / stream. In all realistic
    // scenarios at least 3 bytes are required.
    if (m->xref_table_max_id > m->xref_table_max_offset / 3) {
        m->xref_table_max_id = static_cast<int>(m->xref_table_max_offset / 3);
    }
    qpdf_offset_t start_offset = (end_offset > 1054 ? end_offset - 1054 : 0);
    PatternFinder sf(*this, &QPDF::findStartxref);
    qpdf_offset_t xref_offset = 0;
    if (m->file->findLast("startxref", start_offset, 0, sf)) {
        xref_offset = QUtil::string_to_ll(readToken(*m->file).getValue().c_str());
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
            reconstruct_xref(e, xref_offset > 0);
            QTC::TC("qpdf", "QPDF reconstructed xref table");
        } else {
            throw;
        }
    }

    initializeEncryption();
    m->parsed = true;
    if (m->xref_table.size() > 0 && !getRoot().getKey("/Pages").isDictionary()) {
        // QPDFs created from JSON have an empty xref table and no root object yet.
        throw damagedPDF("", 0, "unable to find page tree");
    }
}

void
QPDF::inParse(bool v)
{
    if (m->in_parse == v) {
        // This happens if QPDFParser::parse tries to resolve an indirect object while it is
        // parsing.
        throw std::logic_error(
            "QPDF: re-entrant parsing detected. This is a qpdf bug."
            " Please report at https://github.com/qpdf/qpdf/issues.");
    }
    m->in_parse = v;
}

void
QPDF::setTrailer(QPDFObjectHandle obj)
{
    if (m->trailer) {
        return;
    }
    m->trailer = obj;
}

void
QPDF::reconstruct_xref(QPDFExc& e, bool found_startxref)
{
    if (m->reconstructed_xref) {
        // Avoid xref reconstruction infinite loops. This is getting very hard to reproduce because
        // qpdf is throwing many fewer exceptions while parsing. Most situations are warnings now.
        throw e;
    }

    // If recovery generates more than 1000 warnings, the file is so severely damaged that there
    // probably is no point trying to continue.
    const auto max_warnings = m->warnings.size() + 1000U;
    auto check_warnings = [this, max_warnings]() {
        if (m->warnings.size() > max_warnings) {
            throw damagedPDF("", 0, "too many errors while reconstructing cross-reference table");
        }
    };

    m->reconstructed_xref = true;
    // We may find more objects, which may contain dangling references.
    m->fixed_dangling_refs = false;

    warn(damagedPDF("", 0, "file is damaged"));
    warn(e);
    warn(damagedPDF("", 0, "Attempting to reconstruct cross-reference table"));

    // Delete all references to type 1 (uncompressed) objects
    std::vector<QPDFObjGen> to_delete;
    for (auto const& iter: m->xref_table) {
        if (iter.second.getType() == 1) {
            to_delete.emplace_back(iter.first);
        }
    }
    for (auto const& iter: to_delete) {
        m->xref_table.erase(iter);
    }

    std::vector<std::tuple<int, int, qpdf_offset_t>> found_objects;
    std::vector<qpdf_offset_t> trailers;
    std::vector<qpdf_offset_t> startxrefs;

    m->file->seek(0, SEEK_END);
    qpdf_offset_t eof = m->file->tell();
    m->file->seek(0, SEEK_SET);
    // Don't allow very long tokens here during recovery. All the interesting tokens are covered.
    static size_t const MAX_LEN = 10;
    while (m->file->tell() < eof) {
        QPDFTokenizer::Token t1 = readToken(*m->file, MAX_LEN);
        qpdf_offset_t token_start = m->file->tell() - toO(t1.getValue().length());
        if (t1.isInteger()) {
            auto pos = m->file->tell();
            auto t2 = readToken(*m->file, MAX_LEN);
            if (t2.isInteger() && readToken(*m->file, MAX_LEN).isWord("obj")) {
                int obj = QUtil::string_to_int(t1.getValue().c_str());
                int gen = QUtil::string_to_int(t2.getValue().c_str());
                if (obj <= m->xref_table_max_id) {
                    found_objects.emplace_back(obj, gen, token_start);
                } else {
                    warn(damagedPDF(
                        "", 0, "ignoring object with impossibly large id " + std::to_string(obj)));
                }
            }
            m->file->seek(pos, SEEK_SET);
        } else if (!m->trailer && t1.isWord("trailer")) {
            trailers.emplace_back(m->file->tell());
        } else if (!found_startxref && t1.isWord("startxref")) {
            startxrefs.emplace_back(m->file->tell());
        }
        check_warnings();
        m->file->findAndSkipNextEOL();
    }

    if (!found_startxref && !startxrefs.empty() && !found_objects.empty() &&
        startxrefs.back() > std::get<2>(found_objects.back())) {
        try {
            m->file->seek(startxrefs.back(), SEEK_SET);
            if (auto offset = QUtil::string_to_ll(readToken(*m->file).getValue().data())) {
                read_xref(offset);
                if (getRoot().getKey("/Pages").isDictionary()) {
                    QTC::TC("qpdf", "QPDF startxref more than 1024 before end");
                    warn(
                        damagedPDF("", 0, "startxref was more than 1024 bytes before end of file"));
                    initializeEncryption();
                    m->parsed = true;
                    m->reconstructed_xref = false;
                    return;
                }
            }
        } catch (...) {
            // ok, bad luck. Do recovery.
        }
    }

    auto rend = found_objects.rend();
    for (auto it = found_objects.rbegin(); it != rend; it++) {
        auto [obj, gen, token_start] = *it;
        insertXrefEntry(obj, 1, token_start, gen);
        check_warnings();
    }
    m->deleted_objects.clear();

    for (auto it = trailers.rbegin(); it != trailers.rend(); it++) {
        m->file->seek(*it, SEEK_SET);
        auto t = readTrailer();
        if (!t.isDictionary()) {
            // Oh well.  It was worth a try.
        } else {
            if (t.hasKey("/Root")) {
                m->trailer = t;
                break;
            }
            warn(damagedPDF("trailer", *it, "recovered trailer has no /Root entry"));
        }
        check_warnings();
    }

    if (!m->trailer) {
        qpdf_offset_t max_offset{0};
        size_t max_size{0};
        // If there are any xref streams, take the last one to appear.
        for (auto const& iter: m->xref_table) {
            auto entry = iter.second;
            if (entry.getType() != 1) {
                continue;
            }
            auto oh = getObject(iter.first);
            try {
                if (!oh.isStreamOfType("/XRef")) {
                    continue;
                }
            } catch (std::exception&) {
                continue;
            }
            auto offset = entry.getOffset();
            auto size = oh.getDict().getKey("/Size").getUIntValueAsUInt();
            if (size > max_size || (size == max_size && offset > max_offset)) {
                max_offset = offset;
                setTrailer(oh.getDict());
            }
            check_warnings();
        }
        if (max_offset > 0) {
            try {
                read_xref(max_offset);
            } catch (std::exception&) {
                warn(damagedPDF(
                    "", 0, "error decoding candidate xref stream while recovering damaged file"));
            }
            QTC::TC("qpdf", "QPDF recover xref stream");
        }
    }

    if (!m->trailer || (!m->parsed && !m->trailer.getKey("/Root").isDictionary())) {
        // Try to find a Root dictionary. As a quick fix try the one with the highest object id.
        QPDFObjectHandle root;
        for (auto const& iter: m->obj_cache) {
            try {
                if (QPDFObjectHandle(iter.second.object).isDictionaryOfType("/Catalog")) {
                    root = iter.second.object;
                }
            } catch (std::exception&) {
                continue;
            }
        }
        if (root) {
            if (!m->trailer) {
                warn(damagedPDF(
                    "", 0, "unable to find trailer dictionary while recovering damaged file"));
                m->trailer = QPDFObjectHandle::newDictionary();
            }
            m->trailer.replaceKey("/Root", root);
        }
    }

    if (!m->trailer) {
        // We could check the last encountered object to see if it was an xref stream.  If so, we
        // could try to get the trailer from there.  This may make it possible to recover files with
        // bad startxref pointers even when they have object streams.

        throw damagedPDF("", 0, "unable to find trailer dictionary while recovering damaged file");
    }
    if (m->xref_table.empty()) {
        // We cannot check for an empty xref table in parse because empty tables are valid when
        // creating QPDF objects from JSON.
        throw damagedPDF("", 0, "unable to find objects while recovering damaged file");
    }
    check_warnings();
    if (!m->parsed) {
        m->parsed = true;
        getAllPages();
        check_warnings();
        if (m->all_pages.empty()) {
            m->parsed = false;
            throw damagedPDF("", 0, "unable to find any pages while recovering damaged file");
        }
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
                if (util::is_space(ch)) {
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
        if ((strncmp(buf, "xref", 4) == 0) && util::is_space(buf[4])) {
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
            // buf is null-terminated, and util::is_space('\0') is false, so this won't overrun.
            while (util::is_space(buf[skip])) {
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

    if (!m->trailer) {
        throw damagedPDF("", 0, "unable to find trailer while reading xref");
    }
    int size = m->trailer.getKey("/Size").getIntValueAsInt();
    int max_obj = 0;
    if (!m->xref_table.empty()) {
        max_obj = m->xref_table.rbegin()->first.getObj();
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

    // Make sure we keep only the highest generation for any object.
    QPDFObjGen last_og{-1, 0};
    for (auto const& item: m->xref_table) {
        auto id = item.first.getObj();
        if (id == last_og.getObj() && id > 0) {
            removeObject(last_og);
        }
        last_og = item.first;
    }
}

bool
QPDF::parse_xrefFirst(std::string const& line, int& obj, int& num, int& bytes)
{
    // is_space and is_digit both return false on '\0', so this will not overrun the null-terminated
    // buffer.
    char const* p = line.c_str();
    char const* start = line.c_str();

    // Skip zero or more spaces
    while (util::is_space(*p)) {
        ++p;
    }
    // Require digit
    if (!util::is_digit(*p)) {
        return false;
    }
    // Gather digits
    std::string obj_str;
    while (util::is_digit(*p)) {
        obj_str.append(1, *p++);
    }
    // Require space
    if (!util::is_space(*p)) {
        return false;
    }
    // Skip spaces
    while (util::is_space(*p)) {
        ++p;
    }
    // Require digit
    if (!util::is_digit(*p)) {
        return false;
    }
    // Gather digits
    std::string num_str;
    while (util::is_digit(*p)) {
        num_str.append(1, *p++);
    }
    // Skip any space including line terminators
    while (util::is_space(*p)) {
        ++p;
    }
    bytes = toI(p - start);
    obj = QUtil::string_to_int(obj_str.c_str());
    num = QUtil::string_to_int(num_str.c_str());
    return true;
}

bool
QPDF::read_bad_xrefEntry(qpdf_offset_t& f1, int& f2, char& type)
{
    // Reposition after initial read attempt and reread.
    m->file->seek(m->file->getLastOffset(), SEEK_SET);
    auto line = m->file->readLine(30);

    // is_space and is_digit both return false on '\0', so this will not overrun the null-terminated
    // buffer.
    char const* p = line.data();

    // Skip zero or more spaces. There aren't supposed to be any.
    bool invalid = false;
    while (util::is_space(*p)) {
        ++p;
        QTC::TC("qpdf", "QPDF ignore first space in xref entry");
        invalid = true;
    }
    // Require digit
    if (!util::is_digit(*p)) {
        return false;
    }
    // Gather digits
    std::string f1_str;
    while (util::is_digit(*p)) {
        f1_str.append(1, *p++);
    }
    // Require space
    if (!util::is_space(*p)) {
        return false;
    }
    if (util::is_space(*(p + 1))) {
        QTC::TC("qpdf", "QPDF ignore first extra space in xref entry");
        invalid = true;
    }
    // Skip spaces
    while (util::is_space(*p)) {
        ++p;
    }
    // Require digit
    if (!util::is_digit(*p)) {
        return false;
    }
    // Gather digits
    std::string f2_str;
    while (util::is_digit(*p)) {
        f2_str.append(1, *p++);
    }
    // Require space
    if (!util::is_space(*p)) {
        return false;
    }
    if (util::is_space(*(p + 1))) {
        QTC::TC("qpdf", "QPDF ignore second extra space in xref entry");
        invalid = true;
    }
    // Skip spaces
    while (util::is_space(*p)) {
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

// Optimistically read and parse xref entry. If entry is bad, call read_bad_xrefEntry and return
// result.
bool
QPDF::read_xrefEntry(qpdf_offset_t& f1, int& f2, char& type)
{
    std::array<char, 21> line;
    if (m->file->read(line.data(), 20) != 20) {
        // C++20: [[unlikely]]
        return false;
    }
    line[20] = '\0';
    char const* p = line.data();

    int f1_len = 0;
    int f2_len = 0;

    // is_space and is_digit both return false on '\0', so this will not overrun the null-terminated
    // buffer.

    // Gather f1 digits. NB No risk of overflow as 9'999'999'999 < max long long.
    while (*p == '0') {
        ++f1_len;
        ++p;
    }
    while (util::is_digit(*p) && f1_len++ < 10) {
        f1 *= 10;
        f1 += *p++ - '0';
    }
    // Require space
    if (!util::is_space(*p++)) {
        // Entry doesn't start with space or digit.
        // C++20: [[unlikely]]
        return false;
    }
    // Gather digits. NB No risk of overflow as 99'999 < max int.
    while (*p == '0') {
        ++f2_len;
        ++p;
    }
    while (util::is_digit(*p) && f2_len++ < 5) {
        f2 *= 10;
        f2 += static_cast<int>(*p++ - '0');
    }
    if (util::is_space(*p++) && (*p == 'f' || *p == 'n')) {
        // C++20: [[likely]]
        type = *p;
        // No test for valid line[19].
        if (*(++p) && *(++p) && (*p == '\n' || *p == '\r') && f1_len == 10 && f2_len == 5) {
            // C++20: [[likely]]
            return true;
        }
    }
    return read_bad_xrefEntry(f1, f2, type);
}

// Read a single cross-reference table section and associated trailer.
qpdf_offset_t
QPDF::read_xrefTable(qpdf_offset_t xref_offset)
{
    m->file->seek(xref_offset, SEEK_SET);
    std::string line;
    while (true) {
        line.assign(50, '\0');
        m->file->read(line.data(), line.size());
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
            // For xref_table, these will always be small enough to be ints
            qpdf_offset_t f1 = 0;
            int f2 = 0;
            char type = '\0';
            if (!read_xrefEntry(f1, f2, type)) {
                QTC::TC("qpdf", "QPDF invalid xref entry");
                throw damagedPDF(
                    "xref table", "invalid xref entry (obj=" + std::to_string(i) + ")");
            }
            if (type == 'f') {
                insertFreeXrefEntry(QPDFObjGen(toI(i), f2));
            } else {
                insertXrefEntry(toI(i), 1, f1, f2);
            }
        }
        qpdf_offset_t pos = m->file->tell();
        if (readToken(*m->file).isWord("trailer")) {
            break;
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

    if (!m->trailer) {
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

    if (cur_trailer.hasKey("/Prev")) {
        if (!cur_trailer.getKey("/Prev").isInteger()) {
            QTC::TC("qpdf", "QPDF trailer prev not integer");
            throw damagedPDF("trailer", "/Prev key in trailer dictionary is not an integer");
        }
        QTC::TC("qpdf", "QPDF prev key in trailer dictionary");
        return cur_trailer.getKey("/Prev").getIntValue();
    }

    return 0;
}

// Read a single cross-reference stream.
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

// Return the entry size of the xref stream and the processed W array.
std::pair<int, std::array<int, 3>>
QPDF::processXRefW(QPDFObjectHandle& dict, std::function<QPDFExc(std::string_view)> damaged)
{
    auto W_obj = dict.getKey("/W");
    if (!(W_obj.isArray() && (W_obj.getArrayNItems() >= 3) && W_obj.getArrayItem(0).isInteger() &&
          W_obj.getArrayItem(1).isInteger() && W_obj.getArrayItem(2).isInteger())) {
        throw damaged("Cross-reference stream does not have a proper /W key");
    }

    std::array<int, 3> W;
    int entry_size = 0;
    auto w_vector = W_obj.getArrayAsVector();
    int max_bytes = sizeof(qpdf_offset_t);
    for (size_t i = 0; i < 3; ++i) {
        W[i] = w_vector[i].getIntValueAsInt();
        if (W[i] > max_bytes) {
            throw damaged("Cross-reference stream's /W contains impossibly large values");
        }
        if (W[i] < 0) {
            throw damaged("Cross-reference stream's /W contains negative values");
        }
        entry_size += W[i];
    }
    if (entry_size == 0) {
        throw damaged("Cross-reference stream's /W indicates entry size of 0");
    }
    return {entry_size, W};
}

// Validate Size key and return the maximum number of entries that the xref stream can contain.
int
QPDF::processXRefSize(
    QPDFObjectHandle& dict, int entry_size, std::function<QPDFExc(std::string_view)> damaged)
{
    // Number of entries is limited by the highest possible object id and stream size.
    auto max_num_entries = std::numeric_limits<int>::max();
    if (max_num_entries > (std::numeric_limits<qpdf_offset_t>::max() / entry_size)) {
        max_num_entries = toI(std::numeric_limits<qpdf_offset_t>::max() / entry_size);
    }

    auto Size_obj = dict.getKey("/Size");
    long long size;
    if (!dict.getKey("/Size").getValueAsInt(size)) {
        throw damaged("Cross-reference stream does not have a proper /Size key");
    } else if (size < 0) {
        throw damaged("Cross-reference stream has a negative /Size key");
    } else if (size >= max_num_entries) {
        throw damaged("Cross-reference stream has an impossibly large /Size key");
    }
    // We are not validating that Size <= (Size key of parent xref / trailer).
    return max_num_entries;
}

// Return the number of entries of the xref stream and the processed Index array.
std::pair<int, std::vector<std::pair<int, int>>>
QPDF::processXRefIndex(
    QPDFObjectHandle& dict, int max_num_entries, std::function<QPDFExc(std::string_view)> damaged)
{
    auto size = dict.getKey("/Size").getIntValueAsInt();
    auto Index_obj = dict.getKey("/Index");

    if (Index_obj.isArray()) {
        std::vector<std::pair<int, int>> indx;
        int num_entries = 0;
        auto index_vec = Index_obj.getArrayAsVector();
        if ((index_vec.size() % 2) || index_vec.size() < 2) {
            throw damaged("Cross-reference stream's /Index has an invalid number of values");
        }

        int i = 0;
        long long first = 0;
        for (auto& val: index_vec) {
            if (val.isInteger()) {
                if (i % 2) {
                    auto count = val.getIntValue();
                    if (count <= 0) {
                        throw damaged(
                            "Cross-reference stream section claims to contain " +
                            std::to_string(count) + " entries");
                    }
                    // We are guarding against the possibility of num_entries * entry_size
                    // overflowing. We are not checking that entries are in ascending order as
                    // required by the spec, which probably should generate a warning. We are also
                    // not checking that for each subsection first object number + number of entries
                    // <= /Size. The spec requires us to ignore object number > /Size.
                    if (first > (max_num_entries - count) ||
                        count > (max_num_entries - num_entries)) {
                        throw damaged(
                            "Cross-reference stream claims to contain too many entries: " +
                            std::to_string(first) + " " + std::to_string(max_num_entries) + " " +
                            std::to_string(num_entries));
                    }
                    indx.emplace_back(static_cast<int>(first), static_cast<int>(count));
                    num_entries += static_cast<int>(count);
                } else {
                    first = val.getIntValue();
                    if (first < 0) {
                        throw damaged(
                            "Cross-reference stream's /Index contains a negative object id");
                    } else if (first > max_num_entries) {
                        throw damaged(
                            "Cross-reference stream's /Index contains an impossibly "
                            "large object id");
                    }
                }
            } else {
                throw damaged(
                    "Cross-reference stream's /Index's item " + std::to_string(i) +
                    " is not an integer");
            }
            i++;
        }
        QTC::TC("qpdf", "QPDF xref /Index is array", index_vec.size() == 2 ? 0 : 1);
        return {num_entries, indx};
    } else if (Index_obj.isNull()) {
        QTC::TC("qpdf", "QPDF xref /Index is null");
        return {size, {{0, size}}};
    } else {
        throw damaged("Cross-reference stream does not have a proper /Index key");
    }
}

qpdf_offset_t
QPDF::processXRefStream(qpdf_offset_t xref_offset, QPDFObjectHandle& xref_obj)
{
    auto damaged = [this, xref_offset](std::string_view msg) -> QPDFExc {
        return damagedPDF("xref stream", xref_offset, msg.data());
    };

    auto dict = xref_obj.getDict();

    auto [entry_size, W] = processXRefW(dict, damaged);
    int max_num_entries = processXRefSize(dict, entry_size, damaged);
    auto [num_entries, indx] = processXRefIndex(dict, max_num_entries, damaged);

    std::shared_ptr<Buffer> bp = xref_obj.getStreamData(qpdf_dl_specialized);
    size_t actual_size = bp->getSize();
    auto expected_size = toS(entry_size) * toS(num_entries);

    if (expected_size != actual_size) {
        QPDFExc x = damaged(
            "Cross-reference stream data has the wrong size; expected = " +
            std::to_string(expected_size) + "; actual = " + std::to_string(actual_size));
        if (expected_size > actual_size) {
            throw x;
        } else {
            warn(x);
        }
    }

    bool saw_first_compressed_object = false;

    // Actual size vs. expected size check above ensures that we will not overflow any buffers here.
    // We know that entry_size * num_entries is less or equal to the size of the buffer.
    auto p = bp->getBuffer();
    for (auto [obj, sec_entries]: indx) {
        // Process a subsection.
        for (int i = 0; i < sec_entries; ++i) {
            // Read this entry
            std::array<qpdf_offset_t, 3> fields{};
            if (W[0] == 0) {
                QTC::TC("qpdf", "QPDF default for xref stream field 0");
                fields[0] = 1;
            }
            for (size_t j = 0; j < 3; ++j) {
                for (int k = 0; k < W[j]; ++k) {
                    fields[j] <<= 8;
                    fields[j] |= *p++;
                }
            }

            // Get the generation number.  The generation number is 0 unless this is an uncompressed
            // object record, in which case the generation number appears as the third field.
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
            } else if (fields[0] == 0) {
                // Ignore fields[2], which we don't care about in this case. This works around the
                // issue of some PDF files that put invalid values, like -1, here for deleted
                // objects.
                insertFreeXrefEntry(QPDFObjGen(obj, 0));
            } else {
                insertXrefEntry(obj, toI(fields[0]), fields[1], toI(fields[2]));
            }
            ++obj;
        }
    }

    if (!m->trailer) {
        setTrailer(dict);
    }

    if (dict.hasKey("/Prev")) {
        if (!dict.getKey("/Prev").isInteger()) {
            throw damagedPDF(
                "xref stream", "/Prev key in xref stream dictionary is not an integer");
        }
        QTC::TC("qpdf", "QPDF prev key in xref stream dictionary");
        return dict.getKey("/Prev").getIntValue();
    } else {
        return 0;
    }
}

void
QPDF::insertXrefEntry(int obj, int f0, qpdf_offset_t f1, int f2)
{
    // Populate the xref table in such a way that the first reference to an object that we see,
    // which is the one in the latest xref table in which it appears, is the one that gets stored.
    // This works because we are reading more recent appends before older ones.

    // If there is already an entry for this object and generation in the table, it means that a
    // later xref table has registered this object.  Disregard this one.
    int new_gen = f0 == 2 ? 0 : f2;

    if (!(obj > 0 && obj <= m->xref_table_max_id && 0 <= f2 && new_gen < 65535)) {
        // We are ignoring invalid objgens. Most will arrive here from xref reconstruction. There
        // is probably no point having another warning but we could count invalid items in order to
        // decide when to give up.
        QTC::TC("qpdf", "QPDF xref overwrite invalid objgen");
        // ignore impossibly large object ids or object ids > Size.
        return;
    }

    if (m->deleted_objects.count(obj)) {
        QTC::TC("qpdf", "QPDF xref deleted object");
        return;
    }

    if (f0 == 2 && static_cast<int>(f1) == obj) {
        warn(damagedPDF("xref stream", "self-referential object stream " + std::to_string(obj)));
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
            throw std::logic_error("unknown cross-reference table type while showing xref_table");
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
QPDF::setLastObjectDescription(std::string const& description, QPDFObjGen og)
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
    auto object =
        QPDFParser(*m->file, "trailer", m->tokenizer, nullptr, this, true).parse(empty, false);
    if (empty) {
        // Nothing in the PDF spec appears to allow empty objects, but they have been encountered in
        // actual PDF files and Adobe Reader appears to ignore them.
        warn(damagedPDF("trailer", "empty object treated as null"));
    } else if (object.isDictionary() && readToken(*m->file).isWord("stream")) {
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
    auto object =
        QPDFParser(*m->file, m->last_object_description, m->tokenizer, decrypter_ptr, this, true)
            .parse(empty, false);
    if (empty) {
        // Nothing in the PDF spec appears to allow empty objects, but they have been encountered in
        // actual PDF files and Adobe Reader appears to ignore them.
        warn(damagedPDF(*m->file, m->file->getLastOffset(), "empty object treated as null"));
        return object;
    }
    auto token = readToken(*m->file);
    if (object.isDictionary() && token.isWord("stream")) {
        readStream(object, og, offset);
        token = readToken(*m->file);
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
        if (!readToken(*m->file).isWord("endstream")) {
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
    object = QPDFObjectHandle(qpdf::Stream(*this, og, object, stream_offset, length));
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
        if (!util::is_space(ch)) {
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
QPDF::readObjectInStream(BufferInputSource& input, int stream_id, int obj_id)
{
    bool empty = false;
    auto object =
        QPDFParser(input, stream_id, obj_id, m->last_object_description, m->tokenizer, this)
            .parse(empty, false);
    if (empty) {
        // Nothing in the PDF spec appears to allow empty objects, but they have been encountered in
        // actual PDF files and Adobe Reader appears to ignore them.
        warn(QPDFExc(
            qpdf_e_damaged_pdf,
            m->file->getName() + " object stream " + std::to_string(stream_id),
            +"object " + std::to_string(obj_id) + " 0, offset " +
                std::to_string(input.getLastOffset()),
            0,
            "empty object treated as null"));
    }
    return object;
}

bool
QPDF::findEndstream()
{
    // Find endstream or endobj. Position the input at that token.
    auto t = readToken(*m->file, 20);
    if (t.isWord("endobj") || t.isWord("endstream")) {
        m->file->seek(m->file->getLastOffset(), SEEK_SET);
        return true;
    }
    return false;
}

size_t
QPDF::recoverStreamLength(
    std::shared_ptr<InputSource> input, QPDFObjGen og, qpdf_offset_t stream_offset)
{
    // Try to reconstruct stream length by looking for endstream or endobj
    warn(damagedPDF(*input, stream_offset, "attempting to recover stream length"));

    PatternFinder ef(*this, &QPDF::findEndstream);
    size_t length = 0;
    if (m->file->findFirst("end", stream_offset, 0, ef)) {
        length = toS(m->file->tell() - stream_offset);
        // Reread endstream but, if it was endobj, don't skip that.
        QPDFTokenizer::Token t = readToken(*m->file);
        if (t.getValue() == "endobj") {
            m->file->seek(m->file->getLastOffset(), SEEK_SET);
        }
    }

    if (length) {
        auto end = stream_offset + toO(length);
        qpdf_offset_t found_offset = 0;
        QPDFObjGen found_og;

        // Make sure this is inside this object
        for (auto const& [current_og, entry]: m->xref_table) {
            if (entry.getType() == 1) {
                qpdf_offset_t obj_offset = entry.getOffset();
                if (found_offset < obj_offset && obj_offset < end) {
                    found_offset = obj_offset;
                    found_og = current_og;
                }
            }
        }
        if (!found_offset || found_og == og) {
            // If we are trying to recover an XRef stream the xref table will not contain and
            // won't contain any entries, therefore we cannot check the found length. Otherwise we
            // found endstream\nendobj within the space allowed for this object, so we're probably
            // in good shape.
        } else {
            QTC::TC("qpdf", "QPDF found wrong endstream in recovery");
            length = 0;
        }
    }

    if (length == 0) {
        warn(damagedPDF(
            *input, stream_offset, "unable to recover stream data; treating stream as empty"));
    } else {
        warn(damagedPDF(
            *input, stream_offset, "recovered stream length: " + std::to_string(length)));
    }

    QTC::TC("qpdf", "QPDF recovered stream length");
    return length;
}

QPDFTokenizer::Token
QPDF::readToken(InputSource& input, size_t max_len)
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
    try {
        QPDFTokenizer::Token tobjid = readToken(*m->file);
        bool objidok = tobjid.isInteger();
        QTC::TC("qpdf", "QPDF check objid", objidok ? 1 : 0);
        if (!objidok) {
            QTC::TC("qpdf", "QPDF expected n n obj");
            throw damagedPDF(offset, "expected n n obj");
        }
        QPDFTokenizer::Token tgen = readToken(*m->file);
        bool genok = tgen.isInteger();
        QTC::TC("qpdf", "QPDF check generation", genok ? 1 : 0);
        if (!genok) {
            throw damagedPDF(offset, "expected n n obj");
        }
        QPDFTokenizer::Token tobj = readToken(*m->file);

        bool objok = tobj.isWord("obj");
        QTC::TC("qpdf", "QPDF check obj", objok ? 1 : 0);

        if (!objok) {
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

std::shared_ptr<QPDFObject> const&
QPDF::resolve(QPDFObjGen og)
{
    if (!isUnresolved(og)) {
        return m->obj_cache[og].object;
    }

    if (m->resolving.count(og)) {
        // This can happen if an object references itself directly or indirectly in some key that
        // has to be resolved during object parsing, such as stream length.
        QTC::TC("qpdf", "QPDF recursion loop in resolve");
        warn(damagedPDF("", "loop detected resolving object " + og.unparse(' ')));
        updateCache(og, QPDFObject::create<QPDF_Null>(), -1, -1);
        return m->obj_cache[og].object;
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
        updateCache(og, QPDFObject::create<QPDF_Null>(), -1, -1);
    }

    auto& result(m->obj_cache[og].object);
    result->setDefaultDescription(this, og);
    return result;
}

void
QPDF::resolveObjectsInStream(int obj_stream_number)
{
    auto damaged =
        [this, obj_stream_number](int id, qpdf_offset_t offset, std::string const& msg) -> QPDFExc {
        return {
            qpdf_e_damaged_pdf,
            m->file->getName() + " object stream " + std::to_string(obj_stream_number),
            +"object " + std::to_string(id) + " 0",
            offset,
            msg};
    };

    if (m->resolved_object_streams.count(obj_stream_number)) {
        return;
    }
    m->resolved_object_streams.insert(obj_stream_number);
    // Force resolution of object stream
    auto obj_stream = getObject(obj_stream_number, 0).as_stream();
    if (!obj_stream) {
        throw damagedPDF(
            "object " + std::to_string(obj_stream_number) + " 0",
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
            "object " + std::to_string(obj_stream_number) + " 0",
            "supposed object stream " + std::to_string(obj_stream_number) + " has wrong type"));
    }

    unsigned int n{0};
    int first{0};
    if (!(dict.getKey("/N").getValueAsUInt(n) && dict.getKey("/First").getValueAsInt(first))) {
        throw damagedPDF(
            "object " + std::to_string(obj_stream_number) + " 0",
            "object stream " + std::to_string(obj_stream_number) + " has incorrect keys");
    }

    std::vector<std::pair<int, long long>> offsets;

    auto bp = obj_stream.getStreamData(qpdf_dl_specialized);
    BufferInputSource input("", bp.get());

    long long last_offset = -1;
    for (unsigned int i = 0; i < n; ++i) {
        auto tnum = readToken(input);
        auto toffset = readToken(input);
        if (!(tnum.isInteger() && toffset.isInteger())) {
            throw damaged(0, input.getLastOffset(), "expected integer in object stream header");
        }

        int num = QUtil::string_to_int(tnum.getValue().c_str());
        long long offset = QUtil::string_to_int(toffset.getValue().c_str());

        if (num == obj_stream_number) {
            QTC::TC("qpdf", "QPDF ignore self-referential object stream");
            warn(damaged(num, input.getLastOffset(), "object stream claims to contain itself"));
            continue;
        }

        if (num < 1) {
            QTC::TC("qpdf", "QPDF object stream contains id < 1");
            warn(damaged(num, input.getLastOffset(), "object id is invalid"s));
            continue;
        }

        if (offset <= last_offset) {
            QTC::TC("qpdf", "QPDF object stream offsets not increasing");
            warn(damaged(
                num,
                offset,
                "offset is invalid (must be larger than previous offset " +
                    std::to_string(last_offset) + ")"));
            continue;
        }
        last_offset = offset;

        if (num > m->xref_table_max_id) {
            continue;
        }

        offsets.emplace_back(num, offset + first);
    }

    // To avoid having to read the object stream multiple times, store all objects that would be
    // found here in the cache.  Remember that some objects stored here might have been overridden
    // by new objects appended to the file, so it is necessary to recheck the xref table and only
    // cache what would actually be resolved here.
    for (auto const& [id, offset]: offsets) {
        QPDFObjGen og(id, 0);
        auto entry = m->xref_table.find(og);
        if (entry != m->xref_table.end() && entry->second.getType() == 2 &&
            entry->second.getObjStreamNumber() == obj_stream_number) {
            input.seek(offset, SEEK_SET);
            QPDFObjectHandle oh = readObjectInStream(input, obj_stream_number, id);
            updateCache(og, oh.getObj(), end_before_space, end_after_space);
        } else {
            QTC::TC("qpdf", "QPDF not caching overridden objstm object");
        }
    }
}

QPDFObjectHandle
QPDF::newIndirect(QPDFObjGen og, std::shared_ptr<QPDFObject> const& obj)
{
    obj->setDefaultDescription(this, og);
    return {obj};
}

void
QPDF::updateCache(
    QPDFObjGen og,
    std::shared_ptr<QPDFObject> const& object,
    qpdf_offset_t end_before_space,
    qpdf_offset_t end_after_space,
    bool destroy)
{
    object->setObjGen(this, og);
    if (isCached(og)) {
        auto& cache = m->obj_cache[og];
        object->move_to(cache.object, destroy);
        cache.end_before_space = end_before_space;
        cache.end_after_space = end_after_space;
    } else {
        m->obj_cache[og] = ObjCache(object, end_before_space, end_after_space);
    }
}

bool
QPDF::isCached(QPDFObjGen og)
{
    return m->obj_cache.count(og) != 0;
}

bool
QPDF::isUnresolved(QPDFObjGen og)
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
    if (!oh) {
        throw std::logic_error("attempted to make an uninitialized QPDFObjectHandle indirect");
    }
    return makeIndirectFromQPDFObject(oh.getObj());
}

std::shared_ptr<QPDFObject>
QPDF::getObjectForParser(int id, int gen, bool parse_pdf)
{
    // This method is called by the parser and therefore must not resolve any objects.
    auto og = QPDFObjGen(id, gen);
    if (auto iter = m->obj_cache.find(og); iter != m->obj_cache.end()) {
        return iter->second.object;
    }
    if (m->xref_table.count(og) || !m->parsed) {
        return m->obj_cache.insert({og, QPDFObject::create<QPDF_Unresolved>(this, og)})
            .first->second.object;
    }
    if (parse_pdf) {
        return QPDFObject::create<QPDF_Null>();
    }
    return m->obj_cache.insert({og, QPDFObject::create<QPDF_Null>(this, og)}).first->second.object;
}

std::shared_ptr<QPDFObject>
QPDF::getObjectForJSON(int id, int gen)
{
    auto og = QPDFObjGen(id, gen);
    auto [it, inserted] = m->obj_cache.try_emplace(og);
    auto& obj = it->second.object;
    if (inserted) {
        obj = (m->parsed && !m->xref_table.count(og))
            ? QPDFObject::create<QPDF_Null>(this, og)
            : QPDFObject::create<QPDF_Unresolved>(this, og);
    }
    return obj;
}

QPDFObjectHandle
QPDF::getObject(QPDFObjGen og)
{
    if (auto it = m->obj_cache.find(og); it != m->obj_cache.end()) {
        return {it->second.object};
    } else if (m->parsed && !m->xref_table.count(og)) {
        return QPDFObject::create<QPDF_Null>();
    } else {
        auto result =
            m->obj_cache.try_emplace(og, QPDFObject::create<QPDF_Unresolved>(this, og), -1, -1);
        return {result.first->second.object};
    }
}

void
QPDF::replaceObject(int objid, int generation, QPDFObjectHandle oh)
{
    replaceObject(QPDFObjGen(objid, generation), oh);
}

void
QPDF::replaceObject(QPDFObjGen og, QPDFObjectHandle oh)
{
    if (!oh || (oh.isIndirect() && !(oh.isStream() && oh.getObjGen() == og))) {
        QTC::TC("qpdf", "QPDF replaceObject called with indirect object");
        throw std::logic_error("QPDF::replaceObject called with indirect object handle");
    }
    updateCache(og, oh.getObj(), -1, -1, false);
}

void
QPDF::removeObject(QPDFObjGen og)
{
    m->xref_table.erase(og);
    if (auto cached = m->obj_cache.find(og); cached != m->obj_cache.end()) {
        // Take care of any object handles that may be floating around.
        cached->second.object->assign_null();
        cached->second.object->setObjGen(nullptr, QPDFObjGen());
        m->obj_cache.erase(cached);
    }
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

void
QPDF::swapObjects(int objid1, int generation1, int objid2, int generation2)
{
    swapObjects(QPDFObjGen(objid1, generation1), QPDFObjGen(objid2, generation2));
}

void
QPDF::swapObjects(QPDFObjGen og1, QPDFObjGen og2)
{
    // Force objects to be read from the input source if needed, then swap them in the cache.
    resolve(og1);
    resolve(og2);
    m->obj_cache[og1].object->swapWith(m->obj_cache[og2].object);
}

size_t
QPDF::tableSize()
{
    // If obj_cache is dense, accommodate all object in tables,else accommodate only original
    // objects.
    auto max_xref = m->xref_table.size() ? m->xref_table.crbegin()->first.getObj() : 0;
    auto max_obj = m->obj_cache.size() ? m->obj_cache.crbegin()->first.getObj() : 0;
    auto max_id = std::numeric_limits<int>::max() - 1;
    if (max_obj >= max_id || max_xref >= max_id) {
        // Temporary fix. Long-term solution is
        // - QPDFObjGen to enforce objgens are valid and sensible
        // - xref table and obj cache to protect against insertion of impossibly large obj ids
        stopOnError("Impossibly large object id encountered.");
    }
    if (max_obj < 1.1 * std::max(toI(m->obj_cache.size()), max_xref)) {
        return toS(++max_obj);
    }
    return toS(++max_xref);
}

std::vector<QPDFObjGen>
QPDF::getCompressibleObjVector()
{
    return getCompressibleObjGens<QPDFObjGen>();
}

std::vector<bool>
QPDF::getCompressibleObjSet()
{
    return getCompressibleObjGens<bool>();
}

template <typename T>
std::vector<T>
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

    const size_t max_obj = getObjectCount();
    std::vector<bool> visited(max_obj, false);
    std::vector<QPDFObjectHandle> queue;
    queue.reserve(512);
    queue.push_back(m->trailer);
    std::vector<T> result;
    if constexpr (std::is_same_v<T, QPDFObjGen>) {
        result.reserve(m->obj_cache.size());
    } else if constexpr (std::is_same_v<T, bool>) {
        result.resize(max_obj + 1U, false);
    } else {
        throw std::logic_error("Unsupported type in QPDF::getCompressibleObjGens");
    }
    while (!queue.empty()) {
        auto obj = queue.back();
        queue.pop_back();
        if (obj.getObjectID() > 0) {
            QPDFObjGen og = obj.getObjGen();
            const size_t id = toS(og.getObj() - 1);
            if (id >= max_obj) {
                throw std::logic_error(
                    "unexpected object id encountered in getCompressibleObjGens");
            }
            if (visited[id]) {
                QTC::TC("qpdf", "QPDF loop detected traversing objects");
                continue;
            }

            // Check whether this is the current object. If not, remove it (which changes it into a
            // direct null and therefore stops us from revisiting it) and move on to the next object
            // in the queue.
            auto upper = m->obj_cache.upper_bound(og);
            if (upper != m->obj_cache.end() && upper->first.getObj() == og.getObj()) {
                removeObject(og);
                continue;
            }

            visited[id] = true;

            if (og == encryption_dict_og) {
                QTC::TC("qpdf", "QPDF exclude encryption dictionary");
            } else if (!(obj.isStream() ||
                         (obj.isDictionaryOfType("/Sig") && obj.hasKey("/ByteRange") &&
                          obj.hasKey("/Contents")))) {
                if constexpr (std::is_same_v<T, QPDFObjGen>) {
                    result.push_back(og);
                } else if constexpr (std::is_same_v<T, bool>) {
                    result[id + 1U] = true;
                }
            }
        }
        if (obj.isStream()) {
            auto dict = obj.getDict().as_dictionary();
            auto end = dict.crend();
            for (auto iter = dict.crbegin(); iter != end; ++iter) {
                std::string const& key = iter->first;
                QPDFObjectHandle const& value = iter->second;
                if (!value.null()) {
                    if (key == "/Length") {
                        // omit stream lengths
                        if (value.isIndirect()) {
                            QTC::TC("qpdf", "QPDF exclude indirect length");
                        }
                    } else {
                        queue.emplace_back(value);
                    }
                }
            }
        } else if (obj.isDictionary()) {
            auto dict = obj.as_dictionary();
            auto end = dict.crend();
            for (auto iter = dict.crbegin(); iter != end; ++iter) {
                if (!iter->second.null()) {
                    queue.emplace_back(iter->second);
                }
            }
        } else if (auto items = obj.as_array()) {
            queue.insert(queue.end(), items.crbegin(), items.crend());
        }
    }

    return result;
}
