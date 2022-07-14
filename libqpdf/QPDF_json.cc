#include <qpdf/QPDF.hh>

#include <qpdf/FileInputSource.hh>
#include <qpdf/Pl_Base64.hh>
#include <qpdf/Pl_StdioFile.hh>
#include <qpdf/QIntC.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>
#include <algorithm>
#include <cstring>

// This chart shows an example of the state transitions that would
// occur in parsing a minimal file.

//                                | st_initial
// {                              |   -> st_top
//   "qpdf-v2": {                 |   -> st_qpdf
//     "objects": {               |   -> st_objects
//       "obj:1 0 R": {           |   -> st_object_top
//         "value": {             |   -> st_object
//           "/Pages": "2 0 R",   |   ...
//           "/Type": "/Catalog"  |   ...
//         }                      |   <- st_object_top
//       },                       |   <- st_objects
//       "obj:2 0 R": {           |   -> st_object_top
//         "value": 12            |   -> st_object
//         }                      |   <- st_object_top
//       },                       |   <- st_objects
//       "obj:4 0 R": {           |   -> st_object_top
//         "stream": {            |   -> st_stream
//           "data": "cG90YXRv",  |   ...
//           "dict": {            |   -> st_object
//             "/K": true         |   ...
//           }                    |   <- st_stream
//         }                      |   <- st_object_top
//       },                       |   <- st_objects
//       "trailer": {             |   -> st_trailer
//         "value": {             |   -> st_object
//           "/Root": "1 0 R",    |   ...
//           "/Size": 7           |   ...
//         }                      |   <- st_trailer
//       }                        |   <- st_objects
//     }                          |   <- st_qpdf
//   }                            |   <- st_top
// }                              |   <- st_initial

static char const* JSON_PDF = (
    // force line break
    "%PDF-1.3\n"
    "xref\n"
    "0 1\n"
    "0000000000 65535 f \n"
    "trailer << /Size 1 >>\n"
    "startxref\n"
    "9\n"
    "%%EOF\n");

// Validator methods -- these are much more performant than std::regex.
static bool
is_indirect_object(std::string const& v, int& obj, int& gen)
{
    char const* p = v.c_str();
    std::string o_str;
    std::string g_str;
    if (!QUtil::is_digit(*p)) {
        return false;
    }
    while (QUtil::is_digit(*p)) {
        o_str.append(1, *p++);
    }
    if (*p != ' ') {
        return false;
    }
    while (*p == ' ') {
        ++p;
    }
    if (!QUtil::is_digit(*p)) {
        return false;
    }
    while (QUtil::is_digit(*p)) {
        g_str.append(1, *p++);
    }
    if (*p != ' ') {
        return false;
    }
    while (*p == ' ') {
        ++p;
    }
    if (*p++ != 'R') {
        return false;
    }
    if (*p) {
        return false;
    }
    obj = QUtil::string_to_int(o_str.c_str());
    gen = QUtil::string_to_int(g_str.c_str());
    return true;
}

static bool
is_obj_key(std::string const& v, int& obj, int& gen)
{
    if (v.substr(0, 4) != "obj:") {
        return false;
    }
    return is_indirect_object(v.substr(4), obj, gen);
}

static bool
is_unicode_string(std::string const& v, std::string& str)
{
    if (v.substr(0, 2) == "u:") {
        str = v.substr(2);
        return true;
    }
    return false;
}

static bool
is_binary_string(std::string const& v, std::string& str)
{
    if (v.substr(0, 2) == "b:") {
        str = v.substr(2);
        int count = 0;
        for (char c: str) {
            if (!QUtil::is_hex_digit(c)) {
                return false;
            }
            ++count;
        }
        return (count % 2 == 0);
    }
    return false;
}

static bool
is_name(std::string const& v)
{
    return ((v.length() > 1) && (v.at(0) == '/'));
}

bool
QPDF::test_json_validators()
{
    bool passed = true;
    auto check_fn = [&passed](char const* msg, bool expr) {
        if (!expr) {
            passed = false;
            std::cerr << msg << std::endl;
        }
    };
#define check(expr) check_fn(#expr, expr)

    int obj = 0;
    int gen = 0;
    check(!is_indirect_object("", obj, gen));
    check(!is_indirect_object("12", obj, gen));
    check(!is_indirect_object("x12 0 R", obj, gen));
    check(!is_indirect_object("12 0 Rx", obj, gen));
    check(!is_indirect_object("12 0R", obj, gen));
    check(is_indirect_object("52 1 R", obj, gen));
    check(obj == 52);
    check(gen == 1);
    check(is_indirect_object("53  20  R", obj, gen));
    check(obj == 53);
    check(gen == 20);
    check(!is_obj_key("", obj, gen));
    check(!is_obj_key("obj:x", obj, gen));
    check(!is_obj_key("obj:x", obj, gen));
    check(is_obj_key("obj:12 13 R", obj, gen));
    check(obj == 12);
    check(gen == 13);
    std::string str;
    check(!is_unicode_string("", str));
    check(!is_unicode_string("xyz", str));
    check(!is_unicode_string("x:", str));
    check(is_unicode_string("u:potato", str));
    check(str == "potato");
    check(is_unicode_string("u:", str));
    check(str == "");
    check(!is_binary_string("", str));
    check(!is_binary_string("x:", str));
    check(!is_binary_string("b:1", str));
    check(!is_binary_string("b:123", str));
    check(!is_binary_string("b:gh", str));
    check(is_binary_string("b:", str));
    check(is_binary_string("b:12", str));
    check(is_binary_string("b:123aBC", str));
    check(!is_name(""));
    check(!is_name("/"));
    check(!is_name("xyz"));
    check(is_name("/Potato"));
    check(is_name("/Potato Salad"));

    return passed;
#undef check_arg
}

static std::function<void(Pipeline*)>
provide_data(
    std::shared_ptr<InputSource> is, qpdf_offset_t start, qpdf_offset_t end)
{
    return [is, start, end](Pipeline* p) {
        Pl_Base64 decode("base64-decode", p, Pl_Base64::a_decode);
        p = &decode;
        size_t bytes = QIntC::to_size(end - start);
        char buf[8192];
        is->seek(start, SEEK_SET);
        size_t len = 0;
        while ((len = is->read(buf, std::min(bytes, sizeof(buf)))) > 0) {
            p->write(buf, len);
            bytes -= len;
            if (bytes == 0) {
                break;
            }
        }
        decode.finish();
    };
}

QPDF::JSONReactor::JSONReactor(
    QPDF& pdf, std::shared_ptr<InputSource> is, bool must_be_complete) :
    pdf(pdf),
    is(is),
    must_be_complete(must_be_complete),
    errors(false),
    parse_error(false),
    saw_qpdf(false),
    saw_objects(false),
    saw_pdf_version(false),
    saw_trailer(false),
    state(st_initial),
    next_state(st_top),
    saw_value(false),
    saw_stream(false),
    saw_dict(false),
    saw_data(false),
    saw_datafile(false),
    this_stream_needs_data(false)
{
    state_stack.push_back(st_initial);
}

void
QPDF::JSONReactor::error(qpdf_offset_t offset, std::string const& msg)
{
    this->errors = true;
    std::string object = this->cur_object;
    if (is->getName() != pdf.getFilename()) {
        object += " from " + is->getName();
    }
    this->pdf.warn(qpdf_e_json, object, offset, msg);
}

bool
QPDF::JSONReactor::anyErrors() const
{
    return this->errors;
}

void
QPDF::JSONReactor::containerStart()
{
    state_stack.push_back(state);
    state = next_state;
}

void
QPDF::JSONReactor::dictionaryStart()
{
    containerStart();
}

void
QPDF::JSONReactor::arrayStart()
{
    containerStart();
    if (state == st_top) {
        QTC::TC("qpdf", "QPDF_json top-level array");
        throw std::runtime_error("QPDF JSON must be a dictionary");
    }
}

void
QPDF::JSONReactor::containerEnd(JSON const& value)
{
    auto from_state = state;
    state = state_stack.back();
    state_stack.pop_back();
    if (state == st_initial) {
        if (!this->saw_qpdf) {
            QTC::TC("qpdf", "QPDF_json missing qpdf");
            error(0, "\"qpdf\" object was not seen");
        } else {
            if (must_be_complete && !this->saw_pdf_version) {
                QTC::TC("qpdf", "QPDF_json missing pdf version");
                error(0, "\"qpdf-v2.pdfversion\" was not seen");
            }
            if (!this->saw_objects) {
                QTC::TC("qpdf", "QPDF_json missing objects");
                error(0, "\"qpdf-v2.objects\" was not seen");
            } else {
                if (must_be_complete && !this->saw_trailer) {
                    QTC::TC("qpdf", "QPDF_json missing trailer");
                    error(0, "\"qpdf-v2.objects.trailer\" was not seen");
                }
            }
        }
    } else if (state == st_objects) {
        if (parse_error) {
            QTC::TC("qpdf", "QPDF_json don't check object after parse error");
        } else if (cur_object == "trailer") {
            if (!saw_value) {
                QTC::TC("qpdf", "QPDF_json trailer no value");
                error(value.getStart(), "\"trailer\" is missing \"value\"");
            }
        } else if (saw_value == saw_stream) {
            QTC::TC("qpdf", "QPDF_json value stream both or neither");
            error(
                value.getStart(),
                "object must have exactly one of \"value\" or \"stream\"");
        }
        object_stack.clear();
        this->cur_object = "";
        this->saw_dict = false;
        this->saw_data = false;
        this->saw_datafile = false;
        this->saw_value = false;
        this->saw_stream = false;
    } else if (state == st_object_top) {
        if (saw_stream) {
            if (!saw_dict) {
                QTC::TC("qpdf", "QPDF_json stream no dict");
                error(value.getStart(), "\"stream\" is missing \"dict\"");
            }
            if (saw_data == saw_datafile) {
                if (this_stream_needs_data) {
                    QTC::TC("qpdf", "QPDF_json data datafile both or neither");
                    error(
                        value.getStart(),
                        "new \"stream\" must have exactly one of \"data\" or "
                        "\"datafile\"");
                } else if (saw_datafile) {
                    QTC::TC("qpdf", "QPDF_json data and datafile");
                    error(
                        value.getStart(),
                        "existing \"stream\" may at most one of \"data\" or "
                        "\"datafile\"");
                } else {
                    QTC::TC("qpdf", "QPDF_json no stream data in update mode");
                }
            }
        }
    } else if ((state == st_stream) || (state == st_object)) {
        if (!parse_error) {
            object_stack.pop_back();
        }
    } else if ((state == st_top) && (from_state == st_qpdf)) {
        for (auto const& og: this->reserved) {
            // Handle dangling indirect object references which the
            // PDF spec says to treat as nulls. It's tempting to make
            // this an error, but that would be wrong since valid
            // input files may have these.
            QTC::TC("qpdf", "QPDF_json non-trivial null reserved");
            this->pdf.replaceObject(og, QPDFObjectHandle::newNull());
        }
        this->reserved.clear();
    }
}

QPDFObjectHandle
QPDF::JSONReactor::reserveObject(int obj, int gen)
{
    auto oh = pdf.reserveObjectIfNotExists(obj, gen);
    if (oh.isReserved()) {
        this->reserved.insert(QPDFObjGen(obj, gen));
    }
    return oh;
}

void
QPDF::JSONReactor::replaceObject(
    QPDFObjectHandle to_replace,
    QPDFObjectHandle replacement,
    JSON const& value)
{
    auto og = to_replace.getObjGen();
    this->reserved.erase(og);
    this->pdf.replaceObject(og, replacement);
    auto oh = pdf.getObject(og);
    setObjectDescription(oh, value);
}

void
QPDF::JSONReactor::topLevelScalar()
{
    QTC::TC("qpdf", "QPDF_json top-level scalar");
    throw std::runtime_error("QPDF JSON must be a dictionary");
}

void
QPDF::JSONReactor::nestedState(
    std::string const& key, JSON const& value, state_e next)
{
    // Use this method when the next state is for processing a nested
    // dictionary.
    if (value.isDictionary()) {
        this->next_state = next;
    } else {
        error(value.getStart(), "\"" + key + "\" must be a dictionary");
        this->next_state = st_ignore;
        this->parse_error = true;
    }
}

bool
QPDF::JSONReactor::dictionaryItem(std::string const& key, JSON const& value)
{
    if (state == st_ignore) {
        QTC::TC("qpdf", "QPDF_json ignoring in st_ignore");
        // ignore
    } else if (state == st_top) {
        if (key == "qpdf-v2") {
            this->saw_qpdf = true;
            nestedState(key, value, st_qpdf);
        } else {
            // Ignore all other fields. We explicitly allow people to
            // add other top-level keys for their own use.
            QTC::TC("qpdf", "QPDF_json ignoring unknown top-level key");
            next_state = st_ignore;
        }
    } else if (state == st_qpdf) {
        if (key == "pdfversion") {
            this->saw_pdf_version = true;
            bool version_okay = false;
            std::string v;
            if (value.getString(v)) {
                std::string version;
                char const* p = v.c_str();
                if (QPDF::validatePDFVersion(p, version) && (*p == '\0')) {
                    version_okay = true;
                    this->pdf.m->pdf_version = version;
                }
            }
            if (!version_okay) {
                QTC::TC("qpdf", "QPDF_json bad pdf version");
                error(value.getStart(), "invalid PDF version (must be x.y)");
            }
        } else if (key == "objects") {
            this->saw_objects = true;
            nestedState(key, value, st_objects);
        } else {
            // ignore unknown keys for forward compatibility and to
            // skip keys we don't care about like "maxobjectid".
            QTC::TC("qpdf", "QPDF_json ignore second-level key");
            next_state = st_ignore;
        }
    } else if (state == st_objects) {
        int obj = 0;
        int gen = 0;
        if (key == "trailer") {
            this->saw_trailer = true;
            nestedState(key, value, st_trailer);
            this->cur_object = "trailer";
        } else if (is_obj_key(key, obj, gen)) {
            this->cur_object = key;
            auto oh = reserveObject(obj, gen);
            object_stack.push_back(oh);
            nestedState(key, value, st_object_top);
        } else {
            QTC::TC("qpdf", "QPDF_json bad object key");
            error(
                value.getStart(),
                "object key should be \"trailer\" or \"obj:n n R\"");
            next_state = st_ignore;
            parse_error = true;
        }
    } else if (state == st_object_top) {
        if (object_stack.size() == 0) {
            throw std::logic_error("no object on stack in st_object_top");
        }
        auto tos = object_stack.back();
        QPDFObjectHandle replacement;
        if (key == "value") {
            // Don't use nestedState since this can have any type.
            this->saw_value = true;
            next_state = st_object;
            replacement = makeObject(value);
            replaceObject(tos, replacement, value);
        } else if (key == "stream") {
            this->saw_stream = true;
            nestedState(key, value, st_stream);
            this->this_stream_needs_data = false;
            if (tos.isStream()) {
                QTC::TC("qpdf", "QPDF_json updating existing stream");
            } else {
                this->this_stream_needs_data = true;
                replacement =
                    pdf.reserveStream(tos.getObjectID(), tos.getGeneration());
                replaceObject(tos, replacement, value);
            }
        } else {
            // Ignore unknown keys for forward compatibility
            QTC::TC("qpdf", "QPDF_json ignore unknown key in object_top");
            next_state = st_ignore;
        }
        if (replacement.isInitialized()) {
            object_stack.pop_back();
            object_stack.push_back(replacement);
        }
    } else if (state == st_trailer) {
        if (key == "value") {
            this->saw_value = true;
            // The trailer must be a dictionary, so we can use nestedState.
            nestedState("trailer.value", value, st_object);
            this->pdf.m->trailer = makeObject(value);
            setObjectDescription(this->pdf.m->trailer, value);
        } else if (key == "stream") {
            // Don't need to set saw_stream here since there's already
            // an error.
            QTC::TC("qpdf", "QPDF_json trailer stream");
            error(value.getStart(), "the trailer may not be a stream");
            next_state = st_ignore;
            parse_error = true;
        } else {
            // Ignore unknown keys for forward compatibility
            QTC::TC("qpdf", "QPDF_json ignore unknown key in trailer");
            next_state = st_ignore;
        }
    } else if (state == st_stream) {
        if (object_stack.size() == 0) {
            throw std::logic_error("no object on stack in st_stream");
        }
        auto tos = object_stack.back();
        if (!tos.isStream()) {
            throw std::logic_error("top of stack is not stream in st_stream");
        }
        auto uninitialized = QPDFObjectHandle();
        if (key == "dict") {
            this->saw_dict = true;
            // Since a stream dictionary must be a dictionary, we can
            // use nestedState to transition to st_value.
            nestedState("stream.dict", value, st_object);
            auto dict = makeObject(value);
            if (dict.isDictionary()) {
                tos.replaceDict(dict);
            } else {
                // An error had already been given by nestedState
                QTC::TC("qpdf", "QPDF_json stream dict not dict");
                parse_error = true;
            }
        } else if (key == "data") {
            this->saw_data = true;
            std::string v;
            if (!value.getString(v)) {
                error(value.getStart(), "\"stream.data\" must be a string");
            } else {
                // The range includes the quotes.
                auto start = value.getStart() + 1;
                auto end = value.getEnd() - 1;
                if (end < start) {
                    throw std::logic_error("QPDF_json: JSON string length < 0");
                }
                tos.replaceStreamData(
                    provide_data(is, start, end), uninitialized, uninitialized);
            }
        } else if (key == "datafile") {
            this->saw_datafile = true;
            std::string filename;
            if (value.getString(filename)) {
                tos.replaceStreamData(
                    QUtil::file_provider(filename),
                    uninitialized,
                    uninitialized);
            } else {
                error(
                    value.getStart(),
                    "\"stream.datafile\" must be a string containing a file "
                    "name");
            }
        } else {
            // Ignore unknown keys for forward compatibility.
            QTC::TC("qpdf", "QPDF_json ignore unknown key in stream");
            next_state = st_ignore;
        }
    } else if (state == st_object) {
        if (!parse_error) {
            auto dict = object_stack.back();
            if (dict.isStream()) {
                dict = dict.getDict();
            }
            dict.replaceKey(key, makeObject(value));
        }
    } else {
        throw std::logic_error(
            "QPDF_json: unknown state " + QUtil::int_to_string(state));
    }
    return true;
}

bool
QPDF::JSONReactor::arrayItem(JSON const& value)
{
    if (state == st_object) {
        if (!parse_error) {
            auto tos = object_stack.back();
            tos.appendItem(makeObject(value));
        }
    }
    return true;
}

void
QPDF::JSONReactor::setObjectDescription(QPDFObjectHandle& oh, JSON const& value)
{
    std::string description = this->is->getName();
    if (!this->cur_object.empty()) {
        description += ", " + this->cur_object;
    }
    description += " at offset " + QUtil::int_to_string(value.getStart());
    oh.setObjectDescription(&this->pdf, description);
}

QPDFObjectHandle
QPDF::JSONReactor::makeObject(JSON const& value)
{
    QPDFObjectHandle result;
    std::string str_v;
    bool bool_v = false;
    if (value.isDictionary()) {
        result = QPDFObjectHandle::newDictionary();
        object_stack.push_back(result);
    } else if (value.isArray()) {
        result = QPDFObjectHandle::newArray();
        object_stack.push_back(result);
    } else if (value.isNull()) {
        result = QPDFObjectHandle::newNull();
    } else if (value.getBool(bool_v)) {
        result = QPDFObjectHandle::newBool(bool_v);
    } else if (value.getNumber(str_v)) {
        if (QUtil::is_long_long(str_v.c_str())) {
            result = QPDFObjectHandle::newInteger(
                QUtil::string_to_ll(str_v.c_str()));
        } else {
            result = QPDFObjectHandle::newReal(str_v);
        }
    } else if (value.getString(str_v)) {
        int obj = 0;
        int gen = 0;
        std::string str;
        if (is_indirect_object(str_v, obj, gen)) {
            result = reserveObject(obj, gen);
        } else if (is_unicode_string(str_v, str)) {
            result = QPDFObjectHandle::newUnicodeString(str);
        } else if (is_binary_string(str_v, str)) {
            result = QPDFObjectHandle::newString(QUtil::hex_decode(str));
        } else if (is_name(str_v)) {
            result = QPDFObjectHandle::newName(str_v);
        } else {
            QTC::TC("qpdf", "QPDF_json unrecognized string value");
            error(value.getStart(), "unrecognized string value");
            result = QPDFObjectHandle::newNull();
        }
    }
    if (!result.isInitialized()) {
        throw std::logic_error(
            "JSONReactor::makeObject didn't initialize the object");
    }

    if (!result.hasObjectDescription()) {
        setObjectDescription(result, value);
    }
    return result;
}

void
QPDF::createFromJSON(std::string const& json_file)
{
    createFromJSON(std::make_shared<FileInputSource>(json_file.c_str()));
}

void
QPDF::createFromJSON(std::shared_ptr<InputSource> is)
{
    processMemoryFile(is->getName().c_str(), JSON_PDF, strlen(JSON_PDF));
    importJSON(is, true);
}

void
QPDF::updateFromJSON(std::string const& json_file)
{
    updateFromJSON(std::make_shared<FileInputSource>(json_file.c_str()));
}

void
QPDF::updateFromJSON(std::shared_ptr<InputSource> is)
{
    importJSON(is, false);
}

void
QPDF::importJSON(std::shared_ptr<InputSource> is, bool must_be_complete)
{
    JSONReactor reactor(*this, is, must_be_complete);
    try {
        JSON::parse(*is, &reactor);
    } catch (std::runtime_error& e) {
        throw std::runtime_error(is->getName() + ": " + e.what());
    }
    if (reactor.anyErrors()) {
        throw std::runtime_error(is->getName() + ": errors found in JSON");
    }
}

void
QPDF::writeJSONStream(
    int version,
    Pipeline* p,
    bool& first,
    std::string const& key,
    QPDFObjectHandle& obj,
    qpdf_stream_decode_level_e decode_level,
    qpdf_json_stream_data_e json_stream_data,
    std::string const& file_prefix)
{
    Pipeline* stream_p = nullptr;
    FILE* f = nullptr;
    std::shared_ptr<Pl_StdioFile> f_pl;
    std::string filename;
    if (json_stream_data == qpdf_sj_file) {
        filename = file_prefix + "-" + QUtil::int_to_string(obj.getObjectID());
        f = QUtil::safe_fopen(filename.c_str(), "wb");
        f_pl = std::make_shared<Pl_StdioFile>("stream data", f);
        stream_p = f_pl.get();
    }
    auto j = JSON::makeDictionary();
    j.addDictionaryMember(
        "stream",
        obj.getStreamJSON(
            version, json_stream_data, decode_level, stream_p, filename));

    JSON::writeDictionaryItem(p, first, key, j, 2);
    if (f) {
        f_pl->finish();
        f_pl = nullptr;
        fclose(f);
    }
}

void
QPDF::writeJSONObject(
    int version,
    Pipeline* p,
    bool& first,
    std::string const& key,
    QPDFObjectHandle& obj)
{
    auto j = JSON::makeDictionary();
    j.addDictionaryMember("value", obj.getJSON(version, true));
    JSON::writeDictionaryItem(p, first, key, j, 2);
}

void
QPDF::writeJSON(
    int version,
    Pipeline* p,
    qpdf_stream_decode_level_e decode_level,
    qpdf_json_stream_data_e json_stream_data,
    std::string const& file_prefix,
    std::set<std::string> wanted_objects)
{
    if (version != 2) {
        throw std::runtime_error(
            "QPDF::writeJSON: only version 2 is supported");
    }
    bool first = true;
    JSON::writeDictionaryOpen(p, first, 0);
    JSON::writeDictionaryKey(p, first, "qpdf-v2", 0);
    bool first_qpdf = true;
    JSON::writeDictionaryOpen(p, first_qpdf, 1);
    JSON::writeDictionaryItem(
        p, first_qpdf, "pdfversion", JSON::makeString(getPDFVersion()), 1);
    JSON::writeDictionaryItem(
        p,
        first_qpdf,
        "maxobjectid",
        JSON::makeInt(QIntC::to_longlong(getObjectCount())),
        1);
    JSON::writeDictionaryKey(p, first_qpdf, "objects", 1);
    bool first_object = true;
    JSON::writeDictionaryOpen(p, first_object, 2);
    bool all_objects = wanted_objects.empty();
    for (auto& obj: getAllObjects()) {
        std::string key = "obj:" + obj.unparse();
        if (all_objects || wanted_objects.count(key)) {
            if (obj.isStream()) {
                writeJSONStream(
                    version,
                    p,
                    first_object,
                    key,
                    obj,
                    decode_level,
                    json_stream_data,
                    file_prefix);
            } else {
                writeJSONObject(version, p, first_object, key, obj);
            }
        }
    }
    if (all_objects || wanted_objects.count("trailer")) {
        auto trailer = getTrailer();
        writeJSONObject(version, p, first_object, "trailer", trailer);
    }
    JSON::writeDictionaryClose(p, first_object, 2);
    JSON::writeDictionaryClose(p, first_qpdf, 1);
    JSON::writeDictionaryClose(p, first, 0);
    *p << "\n";
    p->finish();
}
