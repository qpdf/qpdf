#include <qpdf/qpdf-config.h> // include first for large file support

#include <qpdf/QPDF_private.hh>

#include <array>
#include <cstring>
#include <limits>
#include <map>
#include <vector>

#include <qpdf/BufferInputSource.hh>
#include <qpdf/OffsetInputSource.hh>
#include <qpdf/Pipeline.hh>
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

using Objects = QPDF::Objects;
using Xref_table = Objects::Xref_table;

namespace
{
    class InvalidInputSource final: public InputSource
    {
      public:
        InvalidInputSource(std::string const& name) :
            name(name)
        {
        }
        ~InvalidInputSource() final = default;
        qpdf_offset_t
        findAndSkipNextEOL() final
        {
            throwException();
            return 0;
        }
        std::string const&
        getName() const final
        {
            return name;
        }
        qpdf_offset_t
        tell() final
        {
            throwException();
            return 0;
        }
        void
        seek(qpdf_offset_t offset, int whence) final
        {
            throwException();
        }
        void
        rewind() final
        {
            throwException();
        }
        size_t
        read(char* buffer, size_t length) final
        {
            throwException();
            return 0;
        }
        void
        unreadCh(char ch) final
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

        std::string const& name;
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
Xref_table::initialize_empty()
{
    initialized_ = true;
    trailer_ = QPDFObjectHandle::newDictionary();
    auto rt = qpdf.makeIndirectObject(QPDFObjectHandle::newDictionary());
    auto pgs = qpdf.makeIndirectObject(QPDFObjectHandle::newDictionary());
    pgs.replaceKey("/Type", QPDFObjectHandle::newName("/Pages"));
    pgs.replaceKey("/Kids", QPDFObjectHandle::newArray());
    pgs.replaceKey("/Count", QPDFObjectHandle::newInteger(0));
    rt.replaceKey("/Type", QPDFObjectHandle::newName("/Catalog"));
    rt.replaceKey("/Pages", pgs);
    trailer_.replaceKey("/Root", rt);
    trailer_.replaceKey("/Size", QPDFObjectHandle::newInteger(3));
}

void
Xref_table::initialize_json()
{
    initialized_ = true;
    table.resize(1);
    trailer_ = QPDFObjectHandle::newDictionary();
    trailer_.replaceKey("/Size", QPDFObjectHandle::newInteger(1));
}

void
Xref_table::initialize()
{
    // PDF spec says %%EOF must be found within the last 1024 bytes of/ the file.  We add an extra
    // 30 characters to leave room for the startxref stuff.
    file->seek(0, SEEK_END);
    qpdf_offset_t end_offset = file->tell();
    // Sanity check on object ids. All objects must appear in xref table / stream. In all realistic
    // scenarios at least 3 bytes are required.
    if (max_id_ > end_offset / 3) {
        max_id_ = static_cast<int>(end_offset / 3);
    }
    qpdf_offset_t start_offset = (end_offset > 1054 ? end_offset - 1054 : 0);
    PatternFinder sf(qpdf, &QPDF::findStartxref);
    qpdf_offset_t xref_offset = 0;
    if (file->findLast("startxref", start_offset, 0, sf)) {
        xref_offset = QUtil::string_to_ll(read_token().getValue().c_str());
    }

    try {
        if (xref_offset == 0) {
            QTC::TC("qpdf", "QPDF can't find startxref");
            throw damaged_pdf("can't find startxref");
        }
        try {
            read(xref_offset);
        } catch (QPDFExc&) {
            throw;
        } catch (std::exception& e) {
            throw damaged_pdf(std::string("error reading xref: ") + e.what());
        }
    } catch (QPDFExc& e) {
        if (attempt_recovery_) {
            reconstruct(e);
            QTC::TC("qpdf", "QPDF reconstructed xref table");
        } else {
            throw;
        }
    }

    prepare_obj_table();
    initialized_ = true;
}

// Remove any dangling reference picked up while parsing the xref table from the object table..
void
Xref_table::prepare_obj_table()
{
    for (auto it = objects.table.begin(), end = objects.table.end(); it != end;) {
        if (type(it->first)) {
            ++it;
        } else {
            it->second.object->assign(QPDF_Null::create());
            it->second.object->setObjGen(nullptr, QPDFObjGen());
            it = objects.table.erase(it);
        }
    }
}

void
Xref_table::reconstruct(QPDFExc& e)
{
    if (reconstructed_) {
        // Avoid xref reconstruction infinite loops. This is getting very hard to reproduce because
        // qpdf is throwing many fewer exceptions while parsing. Most situations are warnings now.
        throw e;
    }

    // If recovery generates more than 1000 warnings, the file is so severely damaged that there
    // probably is no point trying to continue.
    const auto max_warnings = qpdf.m->warnings.size() + 1000U;
    auto check_warnings = [this, max_warnings]() {
        if (qpdf.m->warnings.size() > max_warnings) {
            throw damaged_pdf("too many errors while reconstructing cross-reference table");
        }
    };

    reconstructed_ = true;
    // We may find more objects, which may contain dangling references.
    qpdf.m->fixed_dangling_refs = false;

    warn_damaged("file is damaged");
    qpdf.warn(e);
    warn_damaged("Attempting to reconstruct cross-reference table");

    // Delete all references to type 1 (uncompressed) objects
    for (auto& iter: table) {
        if (iter.type() == 1) {
            iter = {};
        }
    }

    std::vector<std::tuple<int, int, qpdf_offset_t>> found_objects;
    std::vector<qpdf_offset_t> trailers;
    int max_found = 0;

    file->seek(0, SEEK_END);
    qpdf_offset_t eof = file->tell();
    file->seek(0, SEEK_SET);
    // Don't allow very long tokens here during recovery. All the interesting tokens are covered.
    static size_t const MAX_LEN = 10;
    while (file->tell() < eof) {
        QPDFTokenizer::Token t1 = read_token(MAX_LEN);
        qpdf_offset_t token_start = file->tell() - toO(t1.getValue().length());
        if (t1.isInteger()) {
            auto pos = file->tell();
            QPDFTokenizer::Token t2 = read_token(MAX_LEN);
            if (t2.isInteger() && read_token(MAX_LEN).isWord("obj")) {
                int obj = QUtil::string_to_int(t1.getValue().c_str());
                int gen = QUtil::string_to_int(t2.getValue().c_str());
                if (obj <= max_id_) {
                    found_objects.emplace_back(obj, gen, token_start);
                    if (obj > max_found) {
                        max_found = obj;
                    }
                } else {
                    warn_damaged("ignoring object with impossibly large id " + std::to_string(obj));
                }
            }
            file->seek(pos, SEEK_SET);
        } else if (!trailer_ && t1.isWord("trailer")) {
            trailers.emplace_back(file->tell());
        }
        file->findAndSkipNextEOL();
    }

    table.resize(toS(max_found) + 1);

    for (auto tr: trailers) {
        file->seek(tr, SEEK_SET);
        auto t = read_trailer();
        if (!t.isDictionary()) {
            // Oh well.  It was worth a try.
        } else {
            trailer_ = t;
            break;
        }
        check_warnings();
    }

    auto rend = found_objects.rend();
    for (auto it = found_objects.rbegin(); it != rend; it++) {
        auto [obj, gen, token_start] = *it;
        insert(obj, 1, token_start, gen);
        check_warnings();
    }

    if (!trailer_) {
        qpdf_offset_t max_offset{0};
        // If there are any xref streams, take the last one to appear.
        int i = -1;
        for (auto const& item: table) {
            ++i;
            if (item.type() != 1) {
                continue;
            }
            auto oh = objects.get(i, item.gen());
            try {
                if (!oh.isStreamOfType("/XRef")) {
                    continue;
                }
            } catch (std::exception&) {
                continue;
            }
            auto offset = item.offset();
            if (offset > max_offset) {
                max_offset = offset;
                trailer_ = oh.getDict();
            }
            check_warnings();
        }
        if (max_offset > 0) {
            try {
                read(max_offset);
            } catch (std::exception&) {
                throw damaged_pdf(
                    "error decoding candidate xref stream while recovering damaged file");
            }
            QTC::TC("qpdf", "QPDF recover xref stream");
        }
    }

    if (!trailer_) {
        // We could check the last encountered object to see if it was an xref stream.  If so, we
        // could try to get the trailer from there.  This may make it possible to recover files with
        // bad startxref pointers even when they have object streams.

        throw damaged_pdf("unable to find trailer dictionary while recovering damaged file");
    }
    if (table.empty()) {
        // We cannot check for an empty xref table in parse because empty tables are valid when
        // creating QPDF objects from JSON.
        throw damaged_pdf("unable to find objects while recovering damaged file");
    }
    check_warnings();
    if (!initialized_) {
        initialized_ = true;
        qpdf.getAllPages();
        check_warnings();
        if (qpdf.m->all_pages.empty()) {
            initialized_ = false;
            throw damaged_pdf("unable to find any pages while recovering damaged file");
        }
    }
    // We could iterate through the objects looking for streams and try to find objects inside of
    // them, but it's probably not worth the trouble.  Acrobat can't recover files with any errors
    // in an xref stream, and this would be a real long shot anyway.  If we wanted to do anything
    // that involved looking at stream contents, we'd also have to call initializeEncryption() here.
    // It's safe to call it more than once.
}

void
Xref_table::read(qpdf_offset_t xref_offset)
{
    std::map<int, int> free_table;
    std::set<qpdf_offset_t> visited;
    while (xref_offset) {
        visited.insert(xref_offset);
        char buf[7];
        memset(buf, 0, sizeof(buf));
        file->seek(xref_offset, SEEK_SET);
        // Some files miss the mark a little with startxref. We could do a better job of searching
        // in the neighborhood for something that looks like either an xref table or stream, but the
        // simple heuristic of skipping whitespace can help with the xref table case and is harmless
        // with the stream case.
        bool done = false;
        bool skipped_space = false;
        while (!done) {
            char ch;
            if (1 == file->read(&ch, 1)) {
                if (QUtil::is_space(ch)) {
                    skipped_space = true;
                } else {
                    file->unreadCh(ch);
                    done = true;
                }
            } else {
                QTC::TC("qpdf", "QPDF eof skipping spaces before xref", skipped_space ? 0 : 1);
                done = true;
            }
        }

        file->read(buf, sizeof(buf) - 1);
        // The PDF spec says xref must be followed by a line terminator, but files exist in the wild
        // where it is terminated by arbitrary whitespace.
        if ((strncmp(buf, "xref", 4) == 0) && QUtil::is_space(buf[4])) {
            if (skipped_space) {
                QTC::TC("qpdf", "QPDF xref skipped space");
                warn_damaged("extraneous whitespace seen before xref");
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
            xref_offset = process_section(xref_offset + skip);
        } else {
            xref_offset = read_stream(xref_offset);
        }
        if (visited.count(xref_offset) != 0) {
            QTC::TC("qpdf", "QPDF xref loop");
            throw damaged_pdf("loop detected following xref tables");
        }
    }

    if (!trailer_) {
        throw damaged_pdf("unable to find trailer while reading xref");
    }
    int size = trailer_.getKey("/Size").getIntValueAsInt();

    if (size < 3) {
        throw damaged_pdf("too few objects - file can't have a page tree");
    }

    // We are no longer reporting what the highest id in the xref table is. I don't think it adds
    // anything. If we want to report more detail, we should report the total number of missing
    // entries, including missing entries before the last actual entry.
}

Xref_table::Subsection
Xref_table::subsection(std::string const& line)
{
    auto terminate = [this]() -> void {
        QTC::TC("qpdf", "QPDF invalid xref");
        throw damaged_table("xref syntax invalid");
    };

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
        terminate();
    }
    // Gather digits
    std::string obj_str;
    while (QUtil::is_digit(*p)) {
        obj_str.append(1, *p++);
    }
    // Require space
    if (!QUtil::is_space(*p)) {
        terminate();
    }
    // Skip spaces
    while (QUtil::is_space(*p)) {
        ++p;
    }
    // Require digit
    if (!QUtil::is_digit(*p)) {
        terminate();
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
    auto obj = QUtil::string_to_int(obj_str.c_str());
    auto count = QUtil::string_to_int(num_str.c_str());
    if (obj > max_id() || count > max_id() || (obj + count) > max_id()) {
        throw damaged_table("xref table subsection header contains impossibly large entry");
    }
    return {obj, count, file->getLastOffset() + toI(p - start)};
}

std::vector<Xref_table::Subsection>
Xref_table::bad_subsections(std::string& line, qpdf_offset_t start)
{
    std::vector<Xref_table::Subsection> result;
    file->seek(start, SEEK_SET);

    while (true) {
        line.assign(50, '\0');
        file->read(line.data(), line.size());
        auto [obj, num, offset] = result.emplace_back(subsection(line));
        file->seek(offset, SEEK_SET);
        for (qpdf_offset_t i = obj; i - num < obj; ++i) {
            if (!std::get<0>(read_entry())) {
                QTC::TC("qpdf", "QPDF invalid xref entry");
                throw damaged_table("invalid xref entry (obj=" + std::to_string(i) + ")");
            }
        }
        qpdf_offset_t pos = file->tell();
        if (read_token().isWord("trailer")) {
            return result;
        } else {
            file->seek(pos, SEEK_SET);
        }
    }
}

// Optimistically read and parse all subsection headers. If an error is encountered return the
// result of bad_subsections.
std::vector<Xref_table::Subsection>
Xref_table::subsections(std::string& line)
{
    auto recovery_offset = file->tell();
    try {
        std::vector<Xref_table::Subsection> result;

        while (true) {
            line.assign(50, '\0');
            file->read(line.data(), line.size());
            auto& sub = result.emplace_back(subsection(line));
            auto count = std::get<1>(sub);
            auto offset = std::get<2>(sub);
            file->seek(offset + 20 * toO(count) - 1, SEEK_SET);
            file->read(line.data(), 1);
            if (!(line[0] == '\n' || line[0] == '\r')) {
                return bad_subsections(line, recovery_offset);
            }
            qpdf_offset_t pos = file->tell();
            if (read_token().isWord("trailer")) {
                return result;
            } else {
                file->seek(pos, SEEK_SET);
            }
        }
    } catch (...) {
        return bad_subsections(line, recovery_offset);
    }
}

// Returns (success, f1, f2, type).
std::tuple<bool, qpdf_offset_t, int, char>
Xref_table::read_bad_entry()
{
    qpdf_offset_t f1{0};
    int f2{0};
    char type{'\0'};
    // Reposition after initial read attempt and reread.
    file->seek(file->getLastOffset(), SEEK_SET);
    auto line = file->readLine(30);

    // is_space and is_digit both return false on '\0', so this will not overrun the null-terminated
    // buffer.
    char const* p = line.data();

    // Skip zero or more spaces. There aren't supposed to be any.
    bool invalid = false;
    while (QUtil::is_space(*p)) {
        ++p;
        QTC::TC("qpdf", "QPDF ignore first space in xref entry");
        invalid = true;
    }
    // Require digit
    if (!QUtil::is_digit(*p)) {
        return {false, 0, 0, '\0'};
    }
    // Gather digits
    std::string f1_str;
    while (QUtil::is_digit(*p)) {
        f1_str.append(1, *p++);
    }
    // Require space
    if (!QUtil::is_space(*p)) {
        return {false, 0, 0, '\0'};
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
        return {false, 0, 0, '\0'};
    }
    // Gather digits
    std::string f2_str;
    while (QUtil::is_digit(*p)) {
        f2_str.append(1, *p++);
    }
    // Require space
    if (!QUtil::is_space(*p)) {
        return {false, 0, 0, '\0'};
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
        return {false, 0, 0, '\0'};
    }
    if ((f1_str.length() != 10) || (f2_str.length() != 5)) {
        QTC::TC("qpdf", "QPDF ignore length error xref entry");
        invalid = true;
    }

    if (invalid) {
        qpdf.warn(damaged_table("accepting invalid xref table entry"));
    }

    f1 = QUtil::string_to_ll(f1_str.c_str());
    f2 = QUtil::string_to_int(f2_str.c_str());

    return {true, f1, f2, type};
}

// Optimistically read and parse xref entry. If entry is bad, call read_bad_xrefEntry and return
// result. Returns (success, f1, f2, type).
std::tuple<bool, qpdf_offset_t, int, char>
Xref_table::read_entry()
{
    qpdf_offset_t f1{0};
    int f2{0};
    char type{'\0'};
    std::array<char, 21> line;

    if (file->read(line.data(), 20) != 20) {
        // C++20: [[unlikely]]
        return {false, 0, 0, '\0'};
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
    while (QUtil::is_digit(*p) && f1_len++ < 10) {
        f1 *= 10;
        f1 += *p++ - '0';
    }
    // Require space
    if (!QUtil::is_space(*p++)) {
        // Entry doesn't start with space or digit.
        // C++20: [[unlikely]]
        return {false, 0, 0, '\0'};
    }
    // Gather digits. NB No risk of overflow as 99'999 < max int.
    while (*p == '0') {
        ++f2_len;
        ++p;
    }
    while (QUtil::is_digit(*p) && f2_len++ < 5) {
        f2 *= 10;
        f2 += static_cast<int>(*p++ - '0');
    }
    if (QUtil::is_space(*p++) && (*p == 'f' || *p == 'n')) {
        // C++20: [[likely]]
        type = *p;
        // No test for valid line[19].
        if (*(++p) && *(++p) && (*p == '\n' || *p == '\r') && f1_len == 10 && f2_len == 5) {
            // C++20: [[likely]]
            return {true, f1, f2, type};
        }
    }
    return read_bad_entry();
}

// Read a single cross-reference table section and associated trailer.
qpdf_offset_t
Xref_table::process_section(qpdf_offset_t xref_offset)
{
    file->seek(xref_offset, SEEK_SET);
    std::string line;
    auto subs = subsections(line);

    auto cur_trailer_offset = file->tell();
    auto cur_trailer = read_trailer();
    if (!cur_trailer.isDictionary()) {
        QTC::TC("qpdf", "QPDF missing trailer");
        throw qpdf.damagedPDF("", "expected trailer dictionary");
    }

    if (!trailer_) {
        unsigned int sz;
        trailer_ = cur_trailer;

        if (!trailer_.hasKey("/Size")) {
            QTC::TC("qpdf", "QPDF trailer lacks size");
            throw qpdf.damagedPDF("trailer", "trailer dictionary lacks /Size key");
        }
        if (!trailer_.getKey("/Size").getValueAsUInt(sz)) {
            QTC::TC("qpdf", "QPDF trailer size not integer");
            throw qpdf.damagedPDF("trailer", "/Size key in trailer dictionary is not an integer");
        }
        if (sz >= static_cast<unsigned int>(max_id_)) {
            QTC::TC("qpdf", "QPDF trailer size impossibly large");
            throw qpdf.damagedPDF("trailer", "/Size key in trailer dictionary is impossibly large");
        }
        table.resize(sz);
    }

    for (auto [obj, num, offset]: subs) {
        file->seek(offset, SEEK_SET);
        for (qpdf_offset_t i = obj; i - num < obj; ++i) {
            if (i == 0) {
                // This is needed by checkLinearization()
                first_item_offset_ = file->tell();
            }
            // For xref_table, these will always be small enough to be ints
            auto [success, f1, f2, type] = read_entry();
            if (!success) {
                throw damaged_table("invalid xref entry (obj=" + std::to_string(i) + ")");
            }
            if (type == 'f') {
                insert_free(QPDFObjGen(toI(i), f2));
            } else {
                insert(toI(i), 1, f1, f2);
            }
        }
        qpdf_offset_t pos = file->tell();
        if (read_token().isWord("trailer")) {
            break;
        } else {
            file->seek(pos, SEEK_SET);
        }
    }

    if (cur_trailer.hasKey("/XRefStm")) {
        if (ignore_streams_) {
            QTC::TC("qpdf", "QPDF ignoring XRefStm in trailer");
        } else {
            if (cur_trailer.getKey("/XRefStm").isInteger()) {
                // Read the xref stream but disregard any return value -- we'll use our trailer's
                // /Prev key instead of the xref stream's.
                (void)read_stream(cur_trailer.getKey("/XRefStm").getIntValue());
            } else {
                throw qpdf.damagedPDF("xref stream", cur_trailer_offset, "invalid /XRefStm");
            }
        }
    }

    if (cur_trailer.hasKey("/Prev")) {
        if (!cur_trailer.getKey("/Prev").isInteger()) {
            QTC::TC("qpdf", "QPDF trailer prev not integer");
            throw qpdf.damagedPDF(
                "trailer", cur_trailer_offset, "/Prev key in trailer dictionary is not an integer");
        }
        QTC::TC("qpdf", "QPDF prev key in trailer dictionary");
        return cur_trailer.getKey("/Prev").getIntValue();
    }

    return 0;
}

// Read a single cross-reference stream.
qpdf_offset_t
Xref_table::read_stream(qpdf_offset_t xref_offset)
{
    if (!ignore_streams_) {
        QPDFObjGen x_og;
        QPDFObjectHandle xref_obj;
        try {
            xref_obj =
                objects.read(false, xref_offset, "xref stream", QPDFObjGen(0, 0), x_og, true);
        } catch (QPDFExc&) {
            // ignore -- report error below
        }
        if (xref_obj.isStreamOfType("/XRef")) {
            QTC::TC("qpdf", "QPDF found xref stream");
            return process_stream(xref_offset, xref_obj);
        }
    }

    QTC::TC("qpdf", "QPDF can't find xref");
    throw qpdf.damagedPDF("", xref_offset, "xref not found");
    return 0; // unreachable
}

// Return the entry size of the xref stream and the processed W array.
std::pair<int, std::array<int, 3>>
Xref_table::process_W(QPDFObjectHandle& dict, std::function<QPDFExc(std::string_view)> damaged)
{
    auto W_obj = dict.getKey("/W");
    if (!(W_obj.isArray() && W_obj.getArrayNItems() >= 3 && W_obj.getArrayItem(0).isInteger() &&
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

// Validate Size entry and return the maximum number of entries that the xref stream can contain and
// the value of the Size entry.
std::pair<int, size_t>
Xref_table::process_Size(
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
    return {max_num_entries, toS(size)};
}

// Return the number of entries of the xref stream and the processed Index array.
std::pair<int, std::vector<std::pair<int, int>>>
Xref_table::process_Index(
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
                        throw damaged("Cross-reference stream's /Index contains an impossibly "
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
Xref_table::process_stream(qpdf_offset_t xref_offset, QPDFObjectHandle& xref_obj)
{
    auto damaged = [this, xref_offset](std::string_view msg) -> QPDFExc {
        return qpdf.damagedPDF("xref stream", xref_offset, msg.data());
    };

    auto dict = xref_obj.getDict();

    auto [entry_size, W] = process_W(dict, damaged);
    auto [max_num_entries, size] = process_Size(dict, entry_size, damaged);
    auto [num_entries, indx] = process_Index(dict, max_num_entries, damaged);

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
            qpdf.warn(x);
        }
    }

    if (!trailer_) {
        trailer_ = dict;
        if (size > toS(max_id_)) {
            throw damaged("Cross-reference stream /Size entry is impossibly large");
        }
        table.resize(size);
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
                    uncompressed_after_compressed_ = true;
                }
            } else if (fields[0] == 2) {
                saw_first_compressed_object = true;
            }
            if (obj == 0) {
                // This is needed by checkLinearization()
                first_item_offset_ = xref_offset;
            } else if (fields[0] == 0) {
                // Ignore fields[2], which we don't care about in this case. This works around the
                // issue of some PDF files that put invalid values, like -1, here for deleted
                // objects.
                insert_free(QPDFObjGen(obj, 0));
            } else {
                insert(obj, toI(fields[0]), fields[1], toI(fields[2]));
            }
            ++obj;
        }
    }

    if (dict.hasKey("/Prev")) {
        if (!dict.getKey("/Prev").isInteger()) {
            throw qpdf.damagedPDF(
                "xref stream", "/Prev key in xref stream dictionary is not an integer");
        }
        QTC::TC("qpdf", "QPDF prev key in xref stream dictionary");
        return dict.getKey("/Prev").getIntValue();
    } else {
        return 0;
    }
}

void
Xref_table::insert(int obj, int f0, qpdf_offset_t f1, int f2)
{
    // Populate the xref table in such a way that the first reference to an object that we see,
    // which is the one in the latest xref table in which it appears, is the one that gets stored.
    // This works because we are reading more recent appends before older ones.

    // If there is already an entry for this object and generation in the table, it means that a
    // later xref table has registered this object.  Disregard this one.

    int new_gen = f0 == 2 ? 0 : f2;

    if (!(obj > 0 && static_cast<size_t>(obj) < table.size() && 0 <= f2 && new_gen < 65535)) {
        // We are ignoring invalid objgens. Most will arrive here from xref reconstruction. There
        // is probably no point having another warning but we could count invalid items in order to
        // decide when to give up.
        QTC::TC("qpdf", "QPDF xref overwrite invalid objgen");
        return;
    }

    auto& entry = table[static_cast<size_t>(obj)];
    auto old_type = entry.type();

    if (!old_type && entry.gen() > 0) {
        // At the moment we are processing the updates last to first and therefore the gen doesn't
        // matter as long as it > 0 to distinguish it from an uninitialized entry. This will need
        // to be revisited when we want to support incremental updates or more comprehensive
        // checking.
        QTC::TC("qpdf", "QPDF xref deleted object");
        return;
    }

    if (f0 == 2 && static_cast<int>(f1) == obj) {
        qpdf.warn(qpdf.damagedPDF(
            "xref stream", "self-referential object stream " + std::to_string(obj)));
        return;
    }

    if (old_type && entry.gen() >= new_gen) {
        QTC::TC("qpdf", "QPDF xref reused object");
        return;
    }

    switch (f0) {
    case 1:
        // f2 is generation
        QTC::TC("qpdf", "QPDF xref gen > 0", (f2 > 0) ? 1 : 0);
        entry = {f2, Uncompressed(f1)};
        break;

    case 2:
        entry = {0, Compressed(toI(f1), f2)};
        object_streams_ = true;
        break;

    default:
        throw qpdf.damagedPDF(
            "xref stream", "unknown xref stream entry type " + std::to_string(f0));
        break;
    }
}

void
Xref_table::insert_free(QPDFObjGen og)
{
    // At the moment we are processing the updates last to first and therefore the gen doesn't
    // matter as long as it > 0 to distinguish it from an uninitialized entry. This will need to be
    // revisited when we want to support incremental updates or more comprehensive checking.
    if (og.getObj() < 1) {
        return;
    }
    size_t id = static_cast<size_t>(og.getObj());
    if (id < table.size() && !type(id)) {
        table[id] = {1, {}};
    }
}

QPDFObjGen
Xref_table::at_offset(qpdf_offset_t offset) const noexcept
{
    int id = 0;
    int gen = 0;
    qpdf_offset_t start = 0;

    int i = 0;
    for (auto const& item: table) {
        auto o = item.offset();
        if (start < o && o <= offset) {
            start = o;
            id = i;
            gen = item.gen();
        }
        ++i;
    }
    return QPDFObjGen(id, gen);
}

std::map<QPDFObjGen, QPDFXRefEntry>
Xref_table::as_map() const
{
    std::map<QPDFObjGen, QPDFXRefEntry> result;
    int i{0};
    for (auto const& item: table) {
        switch (item.type()) {
        case 0:
            break;
        case 1:
            result.emplace(QPDFObjGen(i, item.gen()), item.offset());
            break;
        case 2:
            result.emplace(
                QPDFObjGen(i, 0), QPDFXRefEntry(item.stream_number(), item.stream_index()));
            break;
        default:
            throw std::logic_error("Xref_table: invalid entry type");
        }
        ++i;
    }
    return result;
}

void
Xref_table::show()
{
    auto& cout = *qpdf.m->log->getInfo();
    int i = -1;
    for (auto const& item: table) {
        ++i;
        if (item.type()) {
            cout << std::to_string(i) << "/" << std::to_string(item.gen()) << ": ";
            switch (item.type()) {
            case 1:
                cout << "uncompressed; offset = " << item.offset() << "\n";
                break;

            case 2:
                cout << "compressed; stream = " << item.stream_number()
                     << ", index = " << item.stream_index() << "\n";
                break;

            default:
                throw std::logic_error(
                    "unknown cross-reference table type while showing xref_table");
            }
        }
    }
}

// Resolve all objects in the xref table.
void
Xref_table::resolve_all()
{
    bool may_change = !reconstructed_;
    int i = -1;
    for (auto& item: table) {
        ++i;
        if (item.type()) {
            if (objects.unresolved(QPDFObjGen(i, item.gen()))) {
                objects.resolve(QPDFObjGen(i, item.gen()));
                if (may_change && reconstructed_) {
                    QTC::TC("qpdf", "QPDF fix dangling triggered xref reconstruction");
                    resolve_all();
                    return;
                }
            }
        }
    }
}

std::vector<QPDFObjectHandle>
Objects ::all()
{
    // After fixDanglingReferences is called, all objects are in the object cache.
    qpdf.fixDanglingReferences();
    std::vector<QPDFObjectHandle> result;
    for (auto const& iter: table) {
        result.emplace_back(iter.second.object);
    }
    return result;
}

QPDFObjectHandle
Xref_table::read_trailer()
{
    qpdf_offset_t offset = file->tell();
    bool empty = false;
    auto object = QPDFParser(*file, "trailer", tokenizer, nullptr, &qpdf, true).parse(empty, false);
    if (empty) {
        // Nothing in the PDF spec appears to allow empty objects, but they have been encountered in
        // actual PDF files and Adobe Reader appears to ignore them.
        qpdf.warn(qpdf.damagedPDF("trailer", "empty object treated as null"));
    } else if (object.isDictionary() && read_token().isWord("stream")) {
        qpdf.warn(qpdf.damagedPDF("trailer", file->tell(), "stream keyword found in trailer"));
    }
    // Override last_offset so that it points to the beginning of the object we just read
    file->setLastOffset(offset);
    return object;
}

QPDFObjectHandle
Objects::read_object(std::string const& description, QPDFObjGen og)
{
    qpdf.setLastObjectDescription(description, og);
    qpdf_offset_t offset = m->file->tell();
    bool empty = false;

    StringDecrypter decrypter{&qpdf, og};
    StringDecrypter* decrypter_ptr = m->encp->encrypted ? &decrypter : nullptr;
    auto object =
        QPDFParser(*m->file, m->last_object_description, m->tokenizer, decrypter_ptr, &qpdf, true)
            .parse(empty, false);
    if (empty) {
        // Nothing in the PDF spec appears to allow empty objects, but they have been encountered in
        // actual PDF files and Adobe Reader appears to ignore them.
        qpdf.warn(
            qpdf.damagedPDF(*m->file, m->file->getLastOffset(), "empty object treated as null"));
        return object;
    }
    auto token = qpdf.readToken(*m->file);
    if (object.isDictionary() && token.isWord("stream")) {
        read_stream(object, og, offset);
        token = qpdf.readToken(*m->file);
    }
    if (!token.isWord("endobj")) {
        QTC::TC("qpdf", "QPDF err expected endobj");
        qpdf.warn(qpdf.damagedPDF("expected endobj"));
    }
    return object;
}

// After reading stream dictionary and stream keyword, read rest of stream.
void
Objects::read_stream(QPDFObjectHandle& object, QPDFObjGen og, qpdf_offset_t offset)
{
    validate_stream_line_end(object, og, offset);

    // Must get offset before accessing any additional objects since resolving a previously
    // unresolved indirect object will change file position.
    qpdf_offset_t stream_offset = m->file->tell();
    size_t length = 0;

    try {
        auto length_obj = object.getKey("/Length");

        if (!length_obj.isInteger()) {
            if (length_obj.isNull()) {
                QTC::TC("qpdf", "QPDF stream without length");
                throw qpdf.damagedPDF(offset, "stream dictionary lacks /Length key");
            }
            QTC::TC("qpdf", "QPDF stream length not integer");
            throw qpdf.damagedPDF(offset, "/Length key in stream dictionary is not an integer");
        }

        length = toS(length_obj.getUIntValue());
        // Seek in two steps to avoid potential integer overflow
        m->file->seek(stream_offset, SEEK_SET);
        m->file->seek(toO(length), SEEK_CUR);
        if (!qpdf.readToken(*m->file).isWord("endstream")) {
            QTC::TC("qpdf", "QPDF missing endstream");
            throw qpdf.damagedPDF("expected endstream");
        }
    } catch (QPDFExc& e) {
        if (m->attempt_recovery) {
            qpdf.warn(e);
            length = recover_stream_length(m->file_sp, og, stream_offset);
        } else {
            throw;
        }
    }
    object = {QPDF_Stream::create(&qpdf, og, object, stream_offset, length)};
}

void
Objects::validate_stream_line_end(QPDFObjectHandle& object, QPDFObjGen og, qpdf_offset_t offset)
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
                    qpdf.warn(qpdf.damagedPDF(
                        m->file->tell(), "stream keyword followed by carriage return only"));
                }
            }
            return;
        }
        if (!QUtil::is_space(ch)) {
            QTC::TC("qpdf", "QPDF stream without newline");
            m->file->unreadCh(ch);
            qpdf.warn(qpdf.damagedPDF(
                m->file->tell(), "stream keyword not followed by proper line terminator"));
            return;
        }
        qpdf.warn(
            qpdf.damagedPDF(m->file->tell(), "stream keyword followed by extraneous whitespace"));
    }
}

QPDFObjectHandle
Objects::readObjectInStream(std::shared_ptr<InputSource>& input, int obj)
{
    m->last_object_description.erase(7); // last_object_description starts with "object "
    m->last_object_description += std::to_string(obj);
    m->last_object_description += " 0";

    bool empty = false;
    auto object = QPDFParser(*input, m->last_object_description, m->tokenizer, nullptr, &qpdf, true)
                      .parse(empty, false);
    if (empty) {
        // Nothing in the PDF spec appears to allow empty objects, but they have been encountered in
        // actual PDF files and Adobe Reader appears to ignore them.
        qpdf.warn(qpdf.damagedPDF(*input, input->getLastOffset(), "empty object treated as null"));
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
Objects::recover_stream_length(
    std::shared_ptr<InputSource> input, QPDFObjGen og, qpdf_offset_t stream_offset)
{
    // Try to reconstruct stream length by looking for endstream or endobj
    qpdf.warn(qpdf.damagedPDF(*input, stream_offset, "attempting to recover stream length"));

    PatternFinder ef(qpdf, &QPDF::findEndstream);
    size_t length = 0;
    if (m->file->findFirst("end", stream_offset, 0, ef)) {
        length = toS(m->file->tell() - stream_offset);
        // Reread endstream but, if it was endobj, don't skip that.
        QPDFTokenizer::Token t = qpdf.readToken(*m->file);
        if (t.getValue() == "endobj") {
            m->file->seek(m->file->getLastOffset(), SEEK_SET);
        }
    }

    if (length) {
        // Make sure this is inside this object
        auto found = xref.at_offset(stream_offset + toO(length));
        if (found == QPDFObjGen() || found == og) {
            // If we are trying to recover an XRef stream the xref table will not contain and
            // won't contain any entries, therefore we cannot check the found length. Otherwise we
            // found endstream\endobj within the space allowed for this object, so we're probably
            // in good shape.
        } else {
            QTC::TC("qpdf", "QPDF found wrong endstream in recovery");
            length = 0;
        }
    }

    if (length == 0) {
        qpdf.warn(qpdf.damagedPDF(
            *input, stream_offset, "unable to recover stream data; treating stream as empty"));
    } else {
        qpdf.warn(qpdf.damagedPDF(
            *input, stream_offset, "recovered stream length: " + std::to_string(length)));
    }

    QTC::TC("qpdf", "QPDF recovered stream length");
    return length;
}

QPDFObjectHandle
Objects::read(
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
    qpdf.setLastObjectDescription(description, exp_og);

    if (!m->attempt_recovery) {
        try_recovery = false;
    }

    // Special case: if offset is 0, just return null.  Some PDF writers, in particular
    // "Mac OS X 10.7.5 Quartz PDFContext", may store deleted objects in the xref table as
    // "0000000000 00000 n", which is not correct, but it won't hurt anything for us to ignore
    // these.
    if (offset == 0) {
        QTC::TC("qpdf", "QPDF bogus 0 offset", 0);
        qpdf.warn(qpdf.damagedPDF(0, "object has offset 0"));
        return QPDFObjectHandle::newNull();
    }

    m->file->seek(offset, SEEK_SET);
    try {
        QPDFTokenizer::Token tobjid = qpdf.readToken(*m->file);
        bool objidok = tobjid.isInteger();
        QTC::TC("qpdf", "QPDF check objid", objidok ? 1 : 0);
        if (!objidok) {
            QTC::TC("qpdf", "QPDF expected n n obj");
            throw qpdf.damagedPDF(offset, "expected n n obj");
        }
        QPDFTokenizer::Token tgen = qpdf.readToken(*m->file);
        bool genok = tgen.isInteger();
        QTC::TC("qpdf", "QPDF check generation", genok ? 1 : 0);
        if (!genok) {
            throw qpdf.damagedPDF(offset, "expected n n obj");
        }
        QPDFTokenizer::Token tobj = qpdf.readToken(*m->file);

        bool objok = tobj.isWord("obj");
        QTC::TC("qpdf", "QPDF check obj", objok ? 1 : 0);

        if (!objok) {
            throw qpdf.damagedPDF(offset, "expected n n obj");
        }
        int objid = QUtil::string_to_int(tobjid.getValue().c_str());
        int generation = QUtil::string_to_int(tgen.getValue().c_str());
        og = QPDFObjGen(objid, generation);
        if (objid == 0) {
            QTC::TC("qpdf", "QPDF object id 0");
            throw qpdf.damagedPDF(offset, "object with ID 0");
        }
        if (check_og && (exp_og != og)) {
            QTC::TC("qpdf", "QPDF err wrong objid/generation");
            QPDFExc e = qpdf.damagedPDF(offset, "expected " + exp_og.unparse(' ') + " obj");
            if (try_recovery) {
                // Will be retried below
                throw e;
            } else {
                // We can try reading the object anyway even if the ID doesn't match.
                qpdf.warn(e);
            }
        }
    } catch (QPDFExc& e) {
        if (try_recovery) {
            // Try again after reconstructing xref table
            xref.reconstruct(e);
            if (xref.type(exp_og) == 1) {
                QTC::TC("qpdf", "QPDF recovered in readObjectAtOffset");
                return read(false, xref.offset(exp_og), description, exp_og, og, false);
            } else {
                QTC::TC("qpdf", "QPDF object gone after xref reconstruction");
                qpdf.warn(qpdf.damagedPDF(
                    "",
                    0,
                    ("object " + exp_og.unparse(' ') +
                     " not found in file after regenerating cross reference table")));
                return QPDFObjectHandle::newNull();
            }
        } else {
            throw;
        }
    }

    QPDFObjectHandle oh = read_object(description, og);

    if (unresolved(og)) {
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
                throw qpdf.damagedPDF(m->file->tell(), "EOF after endobj");
            }
        }
        qpdf_offset_t end_after_space = m->file->tell();
        if (skip_cache_if_in_xref && xref.type(og)) {
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
            xref.linearization_offsets(toS(og.getObj()), end_before_space, end_after_space);
            update_table(og, oh.getObj());
        }
    }

    return oh;
}

QPDFObject*
Objects::resolve(QPDFObjGen og)
{
    if (!unresolved(og)) {
        return table[og].object.get();
    }

    if (m->resolving.count(og)) {
        // This can happen if an object references itself directly or indirectly in some key that
        // has to be resolved during object parsing, such as stream length.
        QTC::TC("qpdf", "QPDF recursion loop in resolve");
        qpdf.warn(qpdf.damagedPDF("", "loop detected resolving object " + og.unparse(' ')));
        update_table(og, QPDF_Null::create());
        return table[og].object.get();
    }
    ResolveRecorder rr(&qpdf, og);

    try {
        switch (xref.type(og)) {
        case 0:
            break;
        case 1:
            {
                // Object stored in cache by readObjectAtOffset
                QPDFObjGen a_og;
                QPDFObjectHandle oh = read(true, xref.offset(og), "", og, a_og, false);
            }
            break;

        case 2:
            resolveObjectsInStream(xref.stream_number(og.getObj()));
            break;

        default:
            throw qpdf.damagedPDF(
                "", 0, ("object " + og.unparse('/') + " has unexpected xref entry type"));
        }
    } catch (QPDFExc& e) {
        qpdf.warn(e);
    } catch (std::exception& e) {
        qpdf.warn(qpdf.damagedPDF(
            "", 0, ("object " + og.unparse('/') + ": error reading object: " + e.what())));
    }

    if (unresolved(og)) {
        // PDF spec says unknown objects resolve to the null object.
        QTC::TC("qpdf", "QPDF resolve failure to null");
        update_table(og, QPDF_Null::create());
    }

    auto result(table[og].object);
    result->setDefaultDescription(&qpdf, og);
    return result.get();
}

void
Objects::resolveObjectsInStream(int obj_stream_number)
{
    if (m->resolved_object_streams.count(obj_stream_number)) {
        return;
    }
    m->resolved_object_streams.insert(obj_stream_number);
    // Force resolution of object stream
    QPDFObjectHandle obj_stream = get(obj_stream_number, 0);
    if (!obj_stream.isStream()) {
        throw qpdf.damagedPDF(
            "supposed object stream " + std::to_string(obj_stream_number) + " is not a stream");
    }

    QPDFObjectHandle dict = obj_stream.getDict();
    if (!dict.isDictionaryOfType("/ObjStm")) {
        QTC::TC("qpdf", "QPDF ERR object stream with wrong type");
        qpdf.warn(qpdf.damagedPDF(
            "supposed object stream " + std::to_string(obj_stream_number) + " has wrong type"));
    }

    if (!(dict.getKey("/N").isInteger() && dict.getKey("/First").isInteger())) {
        throw qpdf.damagedPDF(
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

    qpdf_offset_t last_offset = -1;
    for (int i = 0; i < n; ++i) {
        QPDFTokenizer::Token tnum = qpdf.readToken(*input);
        QPDFTokenizer::Token toffset = qpdf.readToken(*input);
        if (!(tnum.isInteger() && toffset.isInteger())) {
            throw damagedPDF(
                *input,
                m->last_object_description,
                input->getLastOffset(),
                "expected integer in object stream header");
        }

        int num = QUtil::string_to_int(tnum.getValue().c_str());
        long long offset = QUtil::string_to_int(toffset.getValue().c_str());
        if (num > xref.max_id()) {
            continue;
        }
        if (num == obj_stream_number) {
            QTC::TC("qpdf", "QPDF ignore self-referential object stream");
            qpdf.warn(damagedPDF(
                *input,
                m->last_object_description,
                input->getLastOffset(),
                "object stream claims to contain itself"));
            continue;
        }
        if (offset <= last_offset) {
            throw damagedPDF(
                *input,
                m->last_object_description,
                input->getLastOffset(),
                "expected offsets in object stream to be increasing");
        }
        last_offset = offset;

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
        if (xref.type(og) == 2 && xref.stream_number(og.getObj()) == obj_stream_number) {
            int offset = iter.second;
            input->seek(offset, SEEK_SET);
            QPDFObjectHandle oh = readObjectInStream(input, iter.first);
            update_table(og, oh.getObj());
        } else {
            QTC::TC("qpdf", "QPDF not caching overridden objstm object");
        }
    }
}

Objects::~Objects()
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

    for (auto const& iter: table) {
        iter.second.object->disconnect();
        if (iter.second.object->getTypeCode() != ::ot_null) {
            iter.second.object->destroy();
        }
    }
}

void
Objects::update_table(QPDFObjGen og, const std::shared_ptr<QPDFObject>& object)
{
    object->setObjGen(&qpdf, og);
    if (cached(og)) {
        auto& cache = table[og];
        cache.object->assign(object);
    } else {
        table[og] = Entry(object);
    }
}

bool
Objects::cached(QPDFObjGen og)
{
    return table.count(og) != 0;
}

bool
Objects::unresolved(QPDFObjGen og)
{
    return !cached(og) || table[og].object->isUnresolved();
}

QPDFObjGen
Objects::next_id()
{
    qpdf.fixDanglingReferences();
    QPDFObjGen og;
    if (!table.empty()) {
        og = (*(m->objects.table.rbegin())).first;
    }
    int max_objid = og.getObj();
    if (max_objid == std::numeric_limits<int>::max()) {
        throw std::range_error("max object id is too high to create new objects");
    }
    return QPDFObjGen(max_objid + 1, 0);
}

QPDFObjectHandle
Objects::make_indirect(std::shared_ptr<QPDFObject> const& obj)
{
    QPDFObjGen next{next_id()};
    table[next] = Entry(obj);
    return qpdf.newIndirect(next, table[next].object);
}

std::shared_ptr<QPDFObject>
Objects::get_for_parser(int id, int gen, bool parse_pdf)
{
    // This method is called by the parser and therefore must not resolve any objects.
    auto og = QPDFObjGen(id, gen);
    if (auto iter = table.find(og); iter != table.end()) {
        return iter->second.object;
    }
    if (xref.type(og) || !xref.initialized()) {
        return table.insert({og, QPDF_Unresolved::create(&qpdf, og)}).first->second.object;
    }
    if (parse_pdf) {
        return QPDF_Null::create();
    }
    return table.insert({og, QPDF_Null::create(&qpdf, og)}).first->second.object;
}

std::shared_ptr<QPDFObject>
Objects::get_for_json(int id, int gen)
{
    auto og = QPDFObjGen(id, gen);
    auto [it, inserted] = table.try_emplace(og);
    auto& obj = it->second.object;
    if (inserted) {
        obj = (xref.initialized() && !xref.type(og)) ? QPDF_Null::create(&qpdf, og)
                                                     : QPDF_Unresolved::create(&qpdf, og);
    }
    return obj;
}

void
Objects::replace(QPDFObjGen og, QPDFObjectHandle oh)
{
    if (!oh || (oh.isIndirect() && !(oh.isStream() && oh.getObjGen() == og))) {
        QTC::TC("qpdf", "QPDF replaceObject called with indirect object");
        throw std::logic_error("QPDF::replaceObject called with indirect object handle");
    }
    update_table(og, oh.getObj());
}

void
Objects::erase(QPDFObjGen og)
{
    if (auto cached = table.find(og); cached != table.end()) {
        // Take care of any object handles that may be floating around.
        cached->second.object->assign(QPDF_Null::create());
        cached->second.object->setObjGen(nullptr, QPDFObjGen());
        table.erase(cached);
    }
}

void
Objects::swap(QPDFObjGen og1, QPDFObjGen og2)
{
    // Force objects to be read from the input source if needed, then swap them in the cache.
    resolve(og1);
    resolve(og2);
    table[og1].object->swapWith(table[og2].object);
}

size_t
Objects::table_size()
{
    // If table is dense, accommodate all object in tables,else accommodate only original
    // objects.
    auto max_xref = toI(xref.size());
    if (max_xref > 0) {
        --max_xref;
    }
    auto max_obj = table.size() ? table.crbegin()->first.getObj() : 0;
    auto max_id = std::numeric_limits<int>::max() - 1;
    if (max_obj >= max_id || max_xref >= max_id) {
        // Temporary fix. Long-term solution is
        // - QPDFObjGen to enforce objgens are valid and sensible
        // - xref table and obj cache to protect against insertion of impossibly large obj ids
        qpdf.stopOnError("Impossibly large object id encountered.");
    }
    if (max_obj < 1.1 * std::max(toI(table.size()), max_xref)) {
        return toS(++max_obj);
    }
    return toS(++max_xref);
}

std::vector<QPDFObjGen>
Objects::compressible_vector()
{
    return compressible<QPDFObjGen>();
}

std::vector<bool>
Objects::compressible_set()
{
    return compressible<bool>();
}

template <typename T>
std::vector<T>
Objects::compressible()
{
    // Return a list of objects that are allowed to be in object streams.  Walk through the objects
    // by traversing the document from the root, including a traversal of the pages tree.  This
    // makes that objects that are on the same page are more likely to be in the same object stream,
    // which is slightly more efficient, particularly with linearized files.  This is better than
    // iterating through the xref table since it avoids preserving orphaned items.

    // Exclude encryption dictionary, if any
    QPDFObjectHandle encryption_dict = trailer().getKey("/Encrypt");
    QPDFObjGen encryption_dict_og = encryption_dict.getObjGen();

    const size_t max_obj = qpdf.getObjectCount();
    std::vector<bool> visited(max_obj, false);
    std::vector<QPDFObjectHandle> queue;
    queue.reserve(512);
    queue.emplace_back(trailer());
    std::vector<T> result;
    if constexpr (std::is_same_v<T, QPDFObjGen>) {
        result.reserve(table.size());
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
            auto upper = table.upper_bound(og);
            if (upper != table.end() && upper->first.getObj() == og.getObj()) {
                erase(og);
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
                    queue.push_back(value);
                }
            }
        } else if (obj.isDictionary()) {
            std::set<std::string> keys = obj.getKeys();
            for (auto iter = keys.rbegin(); iter != keys.rend(); ++iter) {
                queue.push_back(obj.getKey(*iter));
            }
        } else if (obj.isArray()) {
            int n = obj.getArrayNItems();
            for (int i = 1; i <= n; ++i) {
                queue.push_back(obj.getArrayItem(n - i));
            }
        }
    }

    return result;
}
