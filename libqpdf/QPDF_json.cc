#include <qpdf/QPDF.hh>

#include <qpdf/FileInputSource.hh>
#include <qpdf/QIntC.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>
#include <regex>

// This chart shows an example of the state transitions that would
// occur in parsing a minimal file.

//                                | st_initial
// {                              |   -> st_top
//   "qpdf": {                    |   -> st_qpdf
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

static std::regex PDF_VERSION_RE("^\\d+\\.\\d+$");
static std::regex OBJ_KEY_RE("^obj:(\\d+) (\\d+) R$");
static std::regex INDIRECT_OBJ_RE("^(\\d+) (\\d+) R$");
static std::regex UNICODE_RE("^u:(.*)$");
static std::regex BINARY_RE("^b:((?:[0-9a-fA-F]{2})*)$");
static std::regex NAME_RE("^/.*$");

QPDF::JSONReactor::JSONReactor(
    QPDF& pdf, std::string const& filename, bool must_be_complete) :
    pdf(pdf),
    filename(filename),
    must_be_complete(must_be_complete),
    errors(false),
    parse_error(false),
    saw_qpdf(false),
    saw_objects(false),
    saw_json_version(false),
    saw_pdf_version(false),
    saw_trailer(false),
    state(st_initial),
    next_state(st_top),
    saw_value(false),
    saw_stream(false),
    saw_dict(false),
    saw_data(false),
    saw_datafile(false)
{
    state_stack.push_back(st_initial);
}

void
QPDF::JSONReactor::error(size_t offset, std::string const& msg)
{
    this->errors = true;
    this->pdf.warn(
        qpdf_e_json, this->cur_object, QIntC::to_offset(offset), msg);
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
    state = state_stack.back();
    state_stack.pop_back();
    if (state == st_initial) {
        if (!this->saw_qpdf) {
            QTC::TC("qpdf", "QPDF_json missing qpdf");
            error(0, "\"qpdf\" object was not seen");
        } else {
            if (!this->saw_json_version) {
                QTC::TC("qpdf", "QPDF_json missing json version");
                error(0, "\"qpdf.jsonversion\" was not seen");
            }
            if (must_be_complete && !this->saw_pdf_version) {
                QTC::TC("qpdf", "QPDF_json missing pdf version");
                error(0, "\"qpdf.pdfversion\" was not seen");
            }
            if (!this->saw_objects) {
                QTC::TC("qpdf", "QPDF_json missing objects");
                error(0, "\"qpdf.objects\" was not seen");
            } else {
                if (must_be_complete && !this->saw_trailer) {
                    QTC::TC("qpdf", "QPDF_json missing trailer");
                    error(0, "\"qpdf.objects.trailer\" was not seen");
                }
            }
        }
    } else if (state == st_objects) {
        if (parse_error) {
            // ignore
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
            if (must_be_complete) {
                if (saw_data == saw_datafile) {
                    QTC::TC("qpdf", "QPDF_json data datafile both or neither");
                    error(
                        value.getStart(),
                        "\"stream\" must have exactly one of \"data\" or "
                        "\"datafile\"");
                }
            } else if (saw_data && saw_datafile) {
                // QXXXQ
                /// QTC::TC("qpdf", "QPDF_json data and datafile");
                error(
                    value.getStart(),
                    "\"stream\" may at most one of \"data\" or \"datafile\"");
            }
        }
    } else if ((state == st_stream) || (state == st_object)) {
        if (!parse_error) {
            object_stack.pop_back();
        }
    } else if (state == st_qpdf) {
        for (auto const& og: this->reserved) {
            // QXXXQ
            // QTC::TC("qpdf", "QPDF_json non-trivial null reserved");
            this->pdf.replaceObject(og, QPDFObjectHandle::newNull());
        }
        this->reserved.clear();
    }
}

QPDFObjectHandle
QPDF::JSONReactor::reserveObject(std::string const& obj, std::string const& gen)
{
    int o = QUtil::string_to_int(obj.c_str());
    int g = QUtil::string_to_int(gen.c_str());
    auto oh = pdf.reserveObjectIfNotExists(o, g);
    if (oh.isReserved()) {
        this->reserved.insert(QPDFObjGen(o, g));
    }
    return oh;
}

void
QPDF::JSONReactor::replaceObject(
    QPDFObjectHandle to_replace, QPDFObjectHandle replacement)
{
    auto og = to_replace.getObjGen();
    this->reserved.erase(og);
    this->pdf.replaceObject(og, replacement);
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
        if (key == "qpdf") {
            this->saw_qpdf = true;
            nestedState(key, value, st_qpdf);
        } else {
            // Ignore all other fields for forward compatibility.
            // Don't use nestedState since this can be any type.
            // QXXXQ QTC
            next_state = st_ignore;
        }
    } else if (state == st_qpdf) {
        if (key == "jsonversion") {
            this->saw_json_version = true;
            std::string v;
            if (!(value.getNumber(v) && (v == "2"))) {
                QTC::TC("qpdf", "QPDF_json bad json version");
                error(value.getStart(), "only JSON version 2 is supported");
            }
        } else if (key == "pdfversion") {
            this->saw_pdf_version = true;
            bool version_okay = false;
            std::string v;
            if (value.getString(v)) {
                std::smatch m;
                if (std::regex_match(v, m, PDF_VERSION_RE)) {
                    version_okay = true;
                    this->pdf.m->pdf_version = v;
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
            // ignore unknown keys for forward compatibility
            // QXXXQ QTC
            next_state = st_ignore;
        }
    } else if (state == st_objects) {
        std::smatch m;
        if (key == "trailer") {
            this->saw_trailer = true;
            nestedState(key, value, st_trailer);
            this->cur_object = "trailer";
        } else if (std::regex_match(key, m, OBJ_KEY_RE)) {
            // QXXXQ remember to handle null for delete
            object_stack.push_back(reserveObject(m[1].str(), m[2].str()));
            nestedState(key, value, st_object_top);
            this->cur_object = key;
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
            replaceObject(tos, replacement);
        } else if (key == "stream") {
            this->saw_stream = true;
            nestedState(key, value, st_stream);
            if (tos.isStream()) {
                // QXXXQ reusing -- need QTC
            } else {
                replacement =
                    pdf.reserveStream(tos.getObjectID(), tos.getGeneration());
                replaceObject(tos, replacement);
                replacement.replaceStreamData(
                    "", "<<>>"_qpdf, "<<>>"_qpdf); // QXXXQ
            }
        } else {
            // Ignore unknown keys for forward compatibility
            // QXXXQ QTC
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
        } else if (key == "stream") {
            // Don't need to set saw_stream here since there's already
            // an error.
            QTC::TC("qpdf", "QPDF_json trailer stream");
            error(value.getStart(), "the trailer may not be a stream");
            next_state = st_ignore;
            parse_error = true;
        } else {
            // Ignore unknown keys for forward compatibility
            // QXXXQ QTC
            next_state = st_ignore;
        }
    } else if (state == st_stream) {
        if (object_stack.size() == 0) {
            throw std::logic_error("no object on stack in st_stream");
        }
        auto tos = object_stack.back();
        if (!tos.isStream()) {
            // QXXXQ QTC in update mode
            error(value.getStart(), "this object is not a stream");
            parse_error = true;
        } else if (key == "dict") {
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
            // QXXXQ
        } else if (key == "datafile") {
            this->saw_datafile = true;
            // QXXXQ
        } else {
            // Ignore unknown keys for forward compatibility.
            // QXXXQ QTC
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

QPDFObjectHandle
QPDF::JSONReactor::makeObject(JSON const& value)
{
    QPDFObjectHandle result;
    std::string str_v;
    bool bool_v = false;
    std::smatch m;
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
        if (std::regex_match(str_v, m, INDIRECT_OBJ_RE)) {
            result = reserveObject(m[1].str(), m[2].str());
        } else if (std::regex_match(str_v, m, UNICODE_RE)) {
            result = QPDFObjectHandle::newUnicodeString(m[1].str());
        } else if (std::regex_match(str_v, m, BINARY_RE)) {
            result = QPDFObjectHandle::newString(QUtil::hex_decode(m[1].str()));
        } else if (std::regex_match(str_v, m, NAME_RE)) {
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

    // QXXXQ include object number in description
    result.setObjectDescription(
        &this->pdf,
        this->filename + " offset " + QUtil::uint_to_string(value.getStart()));
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
    JSONReactor reactor(*this, is->getName(), must_be_complete);
    try {
        JSON::parse(*is, &reactor);
    } catch (std::runtime_error& e) {
        throw std::runtime_error(is->getName() + ": " + e.what());
    }
    if (reactor.anyErrors()) {
        throw std::runtime_error(is->getName() + ": errors found in JSON");
    }
    // QXXXQ
    // std::cout << "trailer:\n" << getTrailer().unparse() << std::endl;
    // for (auto& oh: getAllObjects()) {
    //     std::cout << oh.unparse() << ":" << std::endl;
    //     std::cout << oh.unparseResolved() << std::endl;
    // }
}
