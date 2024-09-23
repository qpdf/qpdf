#include <qpdf/QPDF.hh>

#include <qpdf/FileInputSource.hh>
#include <qpdf/JSON_writer.hh>
#include <qpdf/Pl_Base64.hh>
#include <qpdf/Pl_StdioFile.hh>
#include <qpdf/QIntC.hh>
#include <qpdf/QPDFObjectHandle_private.hh>
#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>
#include <algorithm>
#include <cstring>

// This chart shows an example of the state transitions that would occur in parsing a minimal file.

//                                |
// {                              |   -> st_top
//   "qpdf": [                    |   -> st_qpdf
//     {                          |   -> st_qpdf_meta
//       ...                      |   ...
//     },                         |   ...
//     {                          |   -> st_objects
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
//   ]                            |   <- st_top
// }                              |

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
    return obj > 0;
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

static bool
is_pdf_name(std::string const& v)
{
    return ((v.length() > 3) && (v.substr(0, 3) == "n:/"));
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
provide_data(std::shared_ptr<InputSource> is, qpdf_offset_t start, qpdf_offset_t end)
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

class QPDF::JSONReactor: public JSON::Reactor
{
  public:
    JSONReactor(QPDF& pdf, std::shared_ptr<InputSource> is, bool must_be_complete) :
        pdf(pdf),
        is(is),
        must_be_complete(must_be_complete),
        descr(
            std::make_shared<QPDFObject::Description>(
                QPDFObject::JSON_Descr(std::make_shared<std::string>(is->getName()), "")))
    {
    }
    ~JSONReactor() override = default;
    void dictionaryStart() override;
    void arrayStart() override;
    void containerEnd(JSON const& value) override;
    void topLevelScalar() override;
    bool dictionaryItem(std::string const& key, JSON const& value) override;
    bool arrayItem(JSON const& value) override;

    bool anyErrors() const;

  private:
    enum state_e {
        st_top,
        st_qpdf,
        st_qpdf_meta,
        st_objects,
        st_trailer,
        st_object_top,
        st_stream,
        st_object,
        st_ignore,
    };

    struct StackFrame
    {
        StackFrame(state_e state) :
            state(state) {};
        StackFrame(state_e state, QPDFObjectHandle&& object) :
            state(state),
            object(object) {};
        state_e state;
        QPDFObjectHandle object;
    };

    void containerStart();
    bool setNextStateIfDictionary(std::string const& key, JSON const& value, state_e);
    void setObjectDescription(QPDFObjectHandle& oh, JSON const& value);
    QPDFObjectHandle makeObject(JSON const& value);
    void error(qpdf_offset_t offset, std::string const& message);
    void replaceObject(QPDFObjectHandle&& replacement, JSON const& value);

    QPDF& pdf;
    std::shared_ptr<InputSource> is;
    bool must_be_complete{true};
    std::shared_ptr<QPDFObject::Description> descr;
    bool errors{false};
    bool saw_qpdf{false};
    bool saw_qpdf_meta{false};
    bool saw_objects{false};
    bool saw_json_version{false};
    bool saw_pdf_version{false};
    bool saw_trailer{false};
    std::string cur_object;
    bool saw_value{false};
    bool saw_stream{false};
    bool saw_dict{false};
    bool saw_data{false};
    bool saw_datafile{false};
    bool this_stream_needs_data{false};
    std::vector<StackFrame> stack;
    QPDFObjectHandle next_obj;
    state_e next_state{st_top};
};

void
QPDF::JSONReactor::error(qpdf_offset_t offset, std::string const& msg)
{
    errors = true;
    std::string object = this->cur_object;
    if (is->getName() != pdf.getFilename()) {
        object += " from " + is->getName();
    }
    pdf.warn(qpdf_e_json, object, offset, msg);
}

bool
QPDF::JSONReactor::anyErrors() const
{
    return errors;
}

void
QPDF::JSONReactor::containerStart()
{
    if (next_obj) {
        stack.emplace_back(next_state, std::move(next_obj));
        next_obj = QPDFObjectHandle();
    } else {
        stack.emplace_back(next_state);
    }
}

void
QPDF::JSONReactor::dictionaryStart()
{
    containerStart();
}

void
QPDF::JSONReactor::arrayStart()
{
    if (stack.empty()) {
        QTC::TC("qpdf", "QPDF_json top-level array");
        throw std::runtime_error("QPDF JSON must be a dictionary");
    }
    containerStart();
}

void
QPDF::JSONReactor::containerEnd(JSON const& value)
{
    auto from_state = stack.back().state;
    stack.pop_back();
    if (stack.empty()) {
        if (!this->saw_qpdf) {
            QTC::TC("qpdf", "QPDF_json missing qpdf");
            error(0, "\"qpdf\" object was not seen");
        } else {
            if (!this->saw_json_version) {
                QTC::TC("qpdf", "QPDF_json missing json version");
                error(0, "\"qpdf[0].jsonversion\" was not seen");
            }
            if (must_be_complete && !this->saw_pdf_version) {
                QTC::TC("qpdf", "QPDF_json missing pdf version");
                error(0, "\"qpdf[0].pdfversion\" was not seen");
            }
            if (!this->saw_objects) {
                QTC::TC("qpdf", "QPDF_json missing objects");
                error(0, "\"qpdf[1]\" was not seen");
            } else {
                if (must_be_complete && !this->saw_trailer) {
                    QTC::TC("qpdf", "QPDF_json missing trailer");
                    error(0, "\"qpdf[1].trailer\" was not seen");
                }
            }
        }
    } else if (from_state == st_trailer) {
        if (!saw_value) {
            QTC::TC("qpdf", "QPDF_json trailer no value");
            error(value.getStart(), "\"trailer\" is missing \"value\"");
        }
    } else if (from_state == st_object_top) {
        if (saw_value == saw_stream) {
            QTC::TC("qpdf", "QPDF_json value stream both or neither");
            error(value.getStart(), "object must have exactly one of \"value\" or \"stream\"");
        }
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
                        "new \"stream\" must have exactly one of \"data\" or \"datafile\"");
                } else if (saw_datafile) {
                    QTC::TC("qpdf", "QPDF_json data and datafile");
                    error(
                        value.getStart(),
                        "existing \"stream\" may at most one of \"data\" or \"datafile\"");
                } else {
                    QTC::TC("qpdf", "QPDF_json no stream data in update mode");
                }
            }
        }
    }
    if (!stack.empty()) {
        auto state = stack.back().state;
        if (state == st_objects) {
            this->cur_object = "";
            this->saw_dict = false;
            this->saw_data = false;
            this->saw_datafile = false;
            this->saw_value = false;
            this->saw_stream = false;
        }
    }
}

void
QPDF::JSONReactor::replaceObject(QPDFObjectHandle&& replacement, JSON const& value)
{
    auto& tos = stack.back();
    auto og = tos.object.getObjGen();
    if (replacement.isIndirect() && !(replacement.isStream() && replacement.getObjGen() == og)) {
        error(
            replacement.getParsedOffset(),
            "the value of an object may not be an indirect object reference");
        return;
    }
    pdf.replaceObject(og, replacement);
    next_obj = pdf.getObject(og);
    setObjectDescription(tos.object, value);
}

void
QPDF::JSONReactor::topLevelScalar()
{
    QTC::TC("qpdf", "QPDF_json top-level scalar");
    throw std::runtime_error("QPDF JSON must be a dictionary");
}

bool
QPDF::JSONReactor::setNextStateIfDictionary(std::string const& key, JSON const& value, state_e next)
{
    // Use this method when the next state is for processing a nested dictionary.
    if (value.isDictionary()) {
        this->next_state = next;
        return true;
    }
    error(value.getStart(), "\"" + key + "\" must be a dictionary");
    return false;
}

bool
QPDF::JSONReactor::dictionaryItem(std::string const& key, JSON const& value)
{
    if (stack.empty()) {
        throw std::logic_error("stack is empty in dictionaryItem");
    }
    next_state = st_ignore;
    auto state = stack.back().state;
    if (state == st_ignore) {
        QTC::TC("qpdf", "QPDF_json ignoring in st_ignore");
        // ignore
    } else if (state == st_top) {
        if (key == "qpdf") {
            this->saw_qpdf = true;
            if (!value.isArray()) {
                QTC::TC("qpdf", "QPDF_json qpdf not array");
                error(value.getStart(), "\"qpdf\" must be an array");
            } else {
                next_state = st_qpdf;
            }
        } else {
            // Ignore all other fields.
            QTC::TC("qpdf", "QPDF_json ignoring unknown top-level key");
        }
    } else if (state == st_qpdf_meta) {
        if (key == "pdfversion") {
            this->saw_pdf_version = true;
            std::string v;
            bool okay = false;
            if (value.getString(v)) {
                std::string version;
                char const* p = v.c_str();
                if (QPDF::validatePDFVersion(p, version) && (*p == '\0')) {
                    this->pdf.m->pdf_version = version;
                    okay = true;
                }
            }
            if (!okay) {
                QTC::TC("qpdf", "QPDF_json bad pdf version");
                error(value.getStart(), "invalid PDF version (must be \"x.y\")");
            }
        } else if (key == "jsonversion") {
            this->saw_json_version = true;
            std::string v;
            bool okay = false;
            if (value.getNumber(v)) {
                std::string version;
                if (QUtil::string_to_int(v.c_str()) == 2) {
                    okay = true;
                }
            }
            if (!okay) {
                QTC::TC("qpdf", "QPDF_json bad json version");
                error(value.getStart(), "invalid JSON version (must be numeric value 2)");
            }
        } else if (key == "pushedinheritedpageresources") {
            bool v;
            if (value.getBool(v)) {
                if (!this->must_be_complete && v) {
                    this->pdf.pushInheritedAttributesToPage();
                }
            } else {
                QTC::TC("qpdf", "QPDF_json bad pushedinheritedpageresources");
                error(value.getStart(), "pushedinheritedpageresources must be a boolean");
            }
        } else if (key == "calledgetallpages") {
            bool v;
            if (value.getBool(v)) {
                if (!this->must_be_complete && v) {
                    this->pdf.getAllPages();
                }
            } else {
                QTC::TC("qpdf", "QPDF_json bad calledgetallpages");
                error(value.getStart(), "calledgetallpages must be a boolean");
            }
        } else {
            // ignore unknown keys for forward compatibility and to skip keys we don't care about
            // like "maxobjectid".
            QTC::TC("qpdf", "QPDF_json ignore second-level key");
        }
    } else if (state == st_objects) {
        int obj = 0;
        int gen = 0;
        if (key == "trailer") {
            this->saw_trailer = true;
            this->cur_object = "trailer";
            setNextStateIfDictionary(key, value, st_trailer);
        } else if (is_obj_key(key, obj, gen)) {
            this->cur_object = key;
            if (setNextStateIfDictionary(key, value, st_object_top)) {
                next_obj = pdf.getObjectForJSON(obj, gen);
            }
        } else {
            QTC::TC("qpdf", "QPDF_json bad object key");
            error(value.getStart(), "object key should be \"trailer\" or \"obj:n n R\"");
        }
    } else if (state == st_object_top) {
        if (stack.empty()) {
            throw std::logic_error("stack empty in st_object_top");
        }
        auto& tos = stack.back();
        if (!tos.object) {
            throw std::logic_error("current object uninitialized in st_object_top");
        }
        if (key == "value") {
            // Don't use setNextStateIfDictionary since this can have any type.
            this->saw_value = true;
            replaceObject(makeObject(value), value);
            next_state = st_object;
        } else if (key == "stream") {
            this->saw_stream = true;
            if (setNextStateIfDictionary(key, value, st_stream)) {
                this->this_stream_needs_data = false;
                if (tos.object.isStream()) {
                    QTC::TC("qpdf", "QPDF_json updating existing stream");
                } else {
                    this_stream_needs_data = true;
                    replaceObject(
                        qpdf::Stream(
                            pdf, tos.object.getObjGen(), QPDFObjectHandle::newDictionary(), 0, 0),
                        value);
                }
                next_obj = tos.object;
            } else {
                // Error message already given above
                QTC::TC("qpdf", "QPDF_json stream not a dictionary");
            }
        } else {
            // Ignore unknown keys for forward compatibility
            QTC::TC("qpdf", "QPDF_json ignore unknown key in object_top");
        }
    } else if (state == st_trailer) {
        if (key == "value") {
            this->saw_value = true;
            // The trailer must be a dictionary, so we can use setNextStateIfDictionary.
            if (setNextStateIfDictionary("trailer.value", value, st_object)) {
                this->pdf.m->trailer = makeObject(value);
                setObjectDescription(this->pdf.m->trailer, value);
            }
        } else if (key == "stream") {
            // Don't need to set saw_stream here since there's already an error.
            QTC::TC("qpdf", "QPDF_json trailer stream");
            error(value.getStart(), "the trailer may not be a stream");
        } else {
            // Ignore unknown keys for forward compatibility
            QTC::TC("qpdf", "QPDF_json ignore unknown key in trailer");
        }
    } else if (state == st_stream) {
        if (stack.empty()) {
            throw std::logic_error("stack empty in st_stream");
        }
        auto& tos = stack.back();
        if (!tos.object.isStream()) {
            throw std::logic_error("current object is not stream in st_stream");
        }
        auto uninitialized = QPDFObjectHandle();
        if (key == "dict") {
            this->saw_dict = true;
            if (setNextStateIfDictionary("stream.dict", value, st_object)) {
                tos.object.replaceDict(makeObject(value));
            } else {
                // An error had already been given by setNextStateIfDictionary
                QTC::TC("qpdf", "QPDF_json stream dict not dict");
            }
        } else if (key == "data") {
            this->saw_data = true;
            std::string v;
            if (!value.getString(v)) {
                QTC::TC("qpdf", "QPDF_json stream data not string");
                error(value.getStart(), "\"stream.data\" must be a string");
                tos.object.replaceStreamData("", uninitialized, uninitialized);
            } else {
                // The range includes the quotes.
                auto start = value.getStart() + 1;
                auto end = value.getEnd() - 1;
                if (end < start) {
                    throw std::logic_error("QPDF_json: JSON string length < 0");
                }
                tos.object.replaceStreamData(
                    provide_data(is, start, end), uninitialized, uninitialized);
            }
        } else if (key == "datafile") {
            this->saw_datafile = true;
            std::string filename;
            if (!value.getString(filename)) {
                QTC::TC("qpdf", "QPDF_json stream datafile not string");
                error(
                    value.getStart(),
                    "\"stream.datafile\" must be a string containing a file name");
                tos.object.replaceStreamData("", uninitialized, uninitialized);
            } else {
                tos.object.replaceStreamData(
                    QUtil::file_provider(filename), uninitialized, uninitialized);
            }
        } else {
            // Ignore unknown keys for forward compatibility.
            QTC::TC("qpdf", "QPDF_json ignore unknown key in stream");
        }
    } else if (state == st_object) {
        if (stack.empty()) {
            throw std::logic_error("stack empty in st_object");
        }
        auto& tos = stack.back();
        auto dict = tos.object;
        if (dict.isStream()) {
            dict = dict.getDict();
        }
        if (!dict.isDictionary()) {
            throw std::logic_error(
                "current object is not stream or dictionary in st_object dictionary item");
        }
        dict.replaceKey(
            is_pdf_name(key) ? QPDFObjectHandle::parse(key.substr(2)).getName() : key,
            makeObject(value));
    } else {
        throw std::logic_error("QPDF_json: unknown state " + std::to_string(state));
    }
    return true;
}

bool
QPDF::JSONReactor::arrayItem(JSON const& value)
{
    if (stack.empty()) {
        throw std::logic_error("stack is empty in arrayItem");
    }
    next_state = st_ignore;
    auto state = stack.back().state;
    if (state == st_qpdf) {
        if (!this->saw_qpdf_meta) {
            this->saw_qpdf_meta = true;
            setNextStateIfDictionary("qpdf[0]", value, st_qpdf_meta);
        } else if (!this->saw_objects) {
            this->saw_objects = true;
            setNextStateIfDictionary("qpdf[1]", value, st_objects);
        } else {
            QTC::TC("qpdf", "QPDF_json more than two qpdf elements");
            error(value.getStart(), "\"qpdf\" must have two elements");
        }
    } else if (state == st_object) {
        stack.back().object.appendItem(makeObject(value));
    }
    return true;
}

void
QPDF::JSONReactor::setObjectDescription(QPDFObjectHandle& oh, JSON const& value)
{
    auto j_descr = std::get<QPDFObject::JSON_Descr>(*descr);
    if (j_descr.object != cur_object) {
        descr = std::make_shared<QPDFObject::Description>(
            QPDFObject::JSON_Descr(j_descr.input, cur_object));
    }

    oh.getObjectPtr()->setDescription(&pdf, descr, value.getStart());
}

QPDFObjectHandle
QPDF::JSONReactor::makeObject(JSON const& value)
{
    QPDFObjectHandle result;
    std::string str_v;
    bool bool_v = false;
    if (value.isDictionary()) {
        result = QPDFObjectHandle::newDictionary();
        next_obj = result;
        next_state = st_object;
    } else if (value.isArray()) {
        result = QPDFObjectHandle::newArray();
        next_obj = result;
        next_state = st_object;
    } else if (value.isNull()) {
        result = QPDFObjectHandle::newNull();
    } else if (value.getBool(bool_v)) {
        result = QPDFObjectHandle::newBool(bool_v);
    } else if (value.getNumber(str_v)) {
        if (QUtil::is_long_long(str_v.c_str())) {
            result = QPDFObjectHandle::newInteger(QUtil::string_to_ll(str_v.c_str()));
        } else {
            // JSON allows scientific notation, but PDF does not.
            if (str_v.find('e') != std::string::npos || str_v.find('E') != std::string::npos) {
                try {
                    auto v = std::stod(str_v);
                    str_v = QUtil::double_to_string(v);
                } catch (std::exception&) {
                    // Keep it as it was
                }
            }
            result = QPDFObjectHandle::newReal(str_v);
        }
    } else if (value.getString(str_v)) {
        int obj = 0;
        int gen = 0;
        std::string str;
        if (is_indirect_object(str_v, obj, gen)) {
            result = pdf.getObjectForJSON(obj, gen);
        } else if (is_unicode_string(str_v, str)) {
            result = QPDFObjectHandle::newUnicodeString(str);
        } else if (is_binary_string(str_v, str)) {
            result = QPDFObjectHandle::newString(QUtil::hex_decode(str));
        } else if (is_name(str_v)) {
            result = QPDFObjectHandle::newName(str_v);
        } else if (is_pdf_name(str_v)) {
            result = QPDFObjectHandle::parse(str_v.substr(2));
        } else {
            QTC::TC("qpdf", "QPDF_json unrecognized string value");
            error(value.getStart(), "unrecognized string value");
            result = QPDFObjectHandle::newNull();
        }
    }
    if (!result) {
        throw std::logic_error("JSONReactor::makeObject didn't initialize the object");
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
writeJSONStreamFile(
    int version,
    JSON::Writer& jw,
    qpdf::Stream& stream,
    int id,
    qpdf_stream_decode_level_e decode_level,
    std::string const& file_prefix)
{
    auto filename = file_prefix + "-" + std::to_string(id);
    auto* f = QUtil::safe_fopen(filename.c_str(), "wb");
    Pl_StdioFile f_pl{"stream data", f};
    stream.writeStreamJSON(version, jw, qpdf_sj_file, decode_level, &f_pl, filename);
    f_pl.finish();
    fclose(f);
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
    bool first = true;
    writeJSON(version, p, true, first, decode_level, json_stream_data, file_prefix, wanted_objects);
}

void
QPDF::writeJSON(
    int version,
    Pipeline* p,
    bool complete,
    bool& first_key,
    qpdf_stream_decode_level_e decode_level,
    qpdf_json_stream_data_e json_stream_data,
    std::string const& file_prefix,
    std::set<std::string> wanted_objects)
{
    if (version != 2) {
        throw std::runtime_error("QPDF::writeJSON: only version 2 is supported");
    }
    JSON::Writer jw{p, 4};
    if (complete) {
        jw << "{";
    } else if (!first_key) {
        jw << ",";
    }
    first_key = false;

    /* clang-format off */
    jw << "\n"
          "  \"qpdf\": [\n"
          "    {\n"
          "      \"jsonversion\": " << std::to_string(version) << ",\n"
          "      \"pdfversion\": \"" << getPDFVersion() << "\",\n"
          "      \"pushedinheritedpageresources\": " <<  (everPushedInheritedAttributesToPages() ? "true" : "false") << ",\n"
          "      \"calledgetallpages\": " <<  (everCalledGetAllPages() ? "true" : "false") << ",\n"
          "      \"maxobjectid\": " <<  std::to_string(getObjectCount()) << "\n"
          "    },\n"
          "    {";
    /* clang-format on */

    bool all_objects = wanted_objects.empty();
    bool first = true;
    for (auto& obj: getAllObjects()) {
        auto const og = obj.getObjGen();
        std::string key = "obj:" + og.unparse(' ') + " R";
        if (all_objects || wanted_objects.count(key)) {
            if (first) {
                jw << "\n      \"" << key;
                first = false;
            } else {
                jw << "\n      },\n      \"" << key;
            }
            if (auto stream = obj.as_stream()) {
                jw << "\": {\n        \"stream\": ";
                if (json_stream_data == qpdf_sj_file) {
                    writeJSONStreamFile(
                        version, jw, stream, og.getObj(), decode_level, file_prefix);
                } else {
                    stream.writeStreamJSON(
                        version, jw, json_stream_data, decode_level, nullptr, "");
                }
            } else {
                jw << "\": {\n        \"value\": ";
                obj.writeJSON(version, jw, true);
            }
        }
    }
    if (all_objects || wanted_objects.count("trailer")) {
        if (!first) {
            jw << "\n      },";
        }
        jw << "\n      \"trailer\": {\n        \"value\": ";
        getTrailer().writeJSON(version, jw, true);
        first = false;
    }
    if (!first) {
        jw << "\n      }";
    }
    /* clang-format off */
    jw << "\n"
          "    }\n"
          "  ]";
    /* clang-format on */
    if (complete) {
        jw << "\n}\n";
        p->finish();
    }
}
