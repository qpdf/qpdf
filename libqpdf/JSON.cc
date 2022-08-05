#include <qpdf/JSON.hh>

#include <qpdf/BufferInputSource.hh>
#include <qpdf/Pl_Base64.hh>
#include <qpdf/Pl_Concatenate.hh>
#include <qpdf/Pl_String.hh>
#include <qpdf/QIntC.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>
#include <cstring>
#include <stdexcept>

template <typename T>
static qpdf_offset_t
toO(T const& i)
{
    return QIntC::to_offset(i);
}

JSON::Members::Members(std::shared_ptr<JSON_value> value) :
    value(value),
    start(0),
    end(0)
{
}

JSON::JSON(std::shared_ptr<JSON_value> value) :
    m(new Members(value))
{
}

void
JSON::writeClose(Pipeline* p, bool first, size_t depth, char const* delimiter)
{
    if (!first) {
        *p << "\n";
        writeIndent(p, depth);
    }
    *p << delimiter;
}

void
JSON::writeIndent(Pipeline* p, size_t depth)
{
    for (size_t i = 0; i < depth; ++i) {
        *p << "  ";
    }
}

void
JSON::writeNext(Pipeline* p, bool& first, size_t depth)
{
    if (first) {
        first = false;
    } else {
        *p << ",";
    }
    *p << "\n";
    writeIndent(p, depth);
}

void
JSON::writeDictionaryOpen(Pipeline* p, bool& first, size_t depth)
{
    *p << "{";
    first = true;
}

void
JSON::writeArrayOpen(Pipeline* p, bool& first, size_t depth)
{
    *p << "[";
    first = true;
}

void
JSON::writeDictionaryClose(Pipeline* p, bool first, size_t depth)
{
    writeClose(p, first, depth, "}");
}

void
JSON::writeArrayClose(Pipeline* p, bool first, size_t depth)
{
    writeClose(p, first, depth, "]");
}

void
JSON::writeDictionaryKey(
    Pipeline* p, bool& first, std::string const& key, size_t depth)
{
    writeNext(p, first, depth);
    *p << "\"" << key << "\": ";
}

void
JSON::writeDictionaryItem(
    Pipeline* p,
    bool& first,
    std::string const& key,
    JSON const& value,
    size_t depth)
{
    writeDictionaryKey(p, first, key, depth);
    value.write(p, depth);
}

void
JSON::writeArrayItem(
    Pipeline* p, bool& first, JSON const& element, size_t depth)
{
    writeNext(p, first, depth);
    element.write(p, depth);
}

void
JSON::JSON_dictionary::write(Pipeline* p, size_t depth) const
{
    bool first = true;
    writeDictionaryOpen(p, first, depth);
    for (auto const& iter: members) {
        writeDictionaryItem(p, first, iter.first, iter.second, 1 + depth);
    }
    writeDictionaryClose(p, first, depth);
}

void
JSON::JSON_array::write(Pipeline* p, size_t depth) const
{
    bool first = true;
    writeArrayOpen(p, first, depth);
    for (auto const& element: elements) {
        writeArrayItem(p, first, element, 1 + depth);
    }
    writeArrayClose(p, first, depth);
}

JSON::JSON_string::JSON_string(std::string const& utf8) :
    utf8(utf8),
    encoded(encode_string(utf8))
{
}

void
JSON::JSON_string::write(Pipeline* p, size_t) const
{
    *p << "\"" << encoded << "\"";
}

JSON::JSON_number::JSON_number(long long value) :
    encoded(QUtil::int_to_string(value))
{
}

JSON::JSON_number::JSON_number(double value) :
    encoded(QUtil::double_to_string(value, 6))
{
}

JSON::JSON_number::JSON_number(std::string const& value) :
    encoded(value)
{
}

void
JSON::JSON_number::write(Pipeline* p, size_t) const
{
    *p << encoded;
}

JSON::JSON_bool::JSON_bool(bool val) :
    value(val)
{
}

void
JSON::JSON_bool::write(Pipeline* p, size_t) const
{
    *p << (value ? "true" : "false");
}

void
JSON::JSON_null::write(Pipeline* p, size_t) const
{
    *p << "null";
}

JSON::JSON_blob::JSON_blob(std::function<void(Pipeline*)> fn) :
    fn(fn)
{
}

void
JSON::JSON_blob::write(Pipeline* p, size_t) const
{
    *p << "\"";
    Pl_Concatenate cat("blob concatenate", p);
    Pl_Base64 base64("blob base64", &cat, Pl_Base64::a_encode);
    fn(&base64);
    base64.finish();
    *p << "\"";
}

void
JSON::write(Pipeline* p, size_t depth) const
{
    if (nullptr == this->m->value.get()) {
        *p << "null";
    } else {
        this->m->value->write(p, depth);
    }
}

std::string
JSON::unparse() const
{
    std::string s;
    Pl_String p("unparse", nullptr, s);
    write(&p, 0);
    return s;
}

std::string
JSON::encode_string(std::string const& str)
{
    std::string result;
    size_t len = str.length();
    for (size_t i = 0; i < len; ++i) {
        unsigned char ch = static_cast<unsigned char>(str.at(i));
        switch (ch) {
        case '\\':
            result += "\\\\";
            break;
        case '\"':
            result += "\\\"";
            break;
        case '\b':
            result += "\\b";
            break;
        case '\f':
            result += "\\f";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            if (ch < 32) {
                result += "\\u" + QUtil::int_to_string_base(ch, 16, 4);
            } else {
                result.append(1, static_cast<char>(ch));
            }
        }
    }
    return result;
}

JSON
JSON::makeDictionary()
{
    return JSON(std::make_shared<JSON_dictionary>());
}

JSON
JSON::addDictionaryMember(std::string const& key, JSON const& val)
{
    JSON_dictionary* obj = dynamic_cast<JSON_dictionary*>(this->m->value.get());
    if (nullptr == obj) {
        throw std::runtime_error(
            "JSON::addDictionaryMember called on non-dictionary");
    }
    if (val.m->value.get()) {
        obj->members[encode_string(key)] = val.m->value;
    } else {
        obj->members[encode_string(key)] = std::make_shared<JSON_null>();
    }
    return obj->members[encode_string(key)];
}

bool
JSON::checkDictionaryKeySeen(std::string const& key)
{
    JSON_dictionary* obj = dynamic_cast<JSON_dictionary*>(this->m->value.get());
    if (nullptr == obj) {
        throw std::logic_error(
            "JSON::checkDictionaryKey called on non-dictionary");
    }
    if (obj->parsed_keys.count(key)) {
        return true;
    }
    obj->parsed_keys.insert(key);
    return false;
}

JSON
JSON::makeArray()
{
    return JSON(std::make_shared<JSON_array>());
}

JSON
JSON::addArrayElement(JSON const& val)
{
    JSON_array* arr = dynamic_cast<JSON_array*>(this->m->value.get());
    if (nullptr == arr) {
        throw std::runtime_error("JSON::addArrayElement called on non-array");
    }
    if (val.m->value.get()) {
        arr->elements.push_back(val.m->value);
    } else {
        arr->elements.push_back(std::make_shared<JSON_null>());
    }
    return arr->elements.back();
}

JSON
JSON::makeString(std::string const& utf8)
{
    return JSON(std::make_shared<JSON_string>(utf8));
}

JSON
JSON::makeInt(long long int value)
{
    return JSON(std::make_shared<JSON_number>(value));
}

JSON
JSON::makeReal(double value)
{
    return JSON(std::make_shared<JSON_number>(value));
}

JSON
JSON::makeNumber(std::string const& encoded)
{
    return JSON(std::make_shared<JSON_number>(encoded));
}

JSON
JSON::makeBool(bool value)
{
    return JSON(std::make_shared<JSON_bool>(value));
}

JSON
JSON::makeNull()
{
    return JSON(std::make_shared<JSON_null>());
}

JSON
JSON::makeBlob(std::function<void(Pipeline*)> fn)
{
    return JSON(std::make_shared<JSON_blob>(fn));
}

bool
JSON::isArray() const
{
    return nullptr != dynamic_cast<JSON_array const*>(this->m->value.get());
}

bool
JSON::isDictionary() const
{
    return nullptr !=
        dynamic_cast<JSON_dictionary const*>(this->m->value.get());
}

bool
JSON::getString(std::string& utf8) const
{
    auto v = dynamic_cast<JSON_string const*>(this->m->value.get());
    if (v == nullptr) {
        return false;
    }
    utf8 = v->utf8;
    return true;
}

bool
JSON::getNumber(std::string& value) const
{
    auto v = dynamic_cast<JSON_number const*>(this->m->value.get());
    if (v == nullptr) {
        return false;
    }
    value = v->encoded;
    return true;
}

bool
JSON::getBool(bool& value) const
{
    auto v = dynamic_cast<JSON_bool const*>(this->m->value.get());
    if (v == nullptr) {
        return false;
    }
    value = v->value;
    return true;
}

bool
JSON::isNull() const
{
    return dynamic_cast<JSON_null const*>(this->m->value.get()) != nullptr;
}

bool
JSON::forEachDictItem(
    std::function<void(std::string const& key, JSON value)> fn) const
{
    auto v = dynamic_cast<JSON_dictionary const*>(this->m->value.get());
    if (v == nullptr) {
        return false;
    }
    for (auto const& k: v->members) {
        fn(k.first, JSON(k.second));
    }
    return true;
}

bool
JSON::forEachArrayItem(std::function<void(JSON value)> fn) const
{
    auto v = dynamic_cast<JSON_array const*>(this->m->value.get());
    if (v == nullptr) {
        return false;
    }
    for (auto const& i: v->elements) {
        fn(JSON(i));
    }
    return true;
}

bool
JSON::checkSchema(JSON schema, std::list<std::string>& errors)
{
    return checkSchemaInternal(
        this->m->value.get(), schema.m->value.get(), 0, errors, "");
}

bool
JSON::checkSchema(
    JSON schema, unsigned long flags, std::list<std::string>& errors)
{
    return checkSchemaInternal(
        this->m->value.get(), schema.m->value.get(), flags, errors, "");
}

bool
JSON::checkSchemaInternal(
    JSON_value* this_v,
    JSON_value* sch_v,
    unsigned long flags,
    std::list<std::string>& errors,
    std::string prefix)
{
    JSON_array* this_arr = dynamic_cast<JSON_array*>(this_v);
    JSON_dictionary* this_dict = dynamic_cast<JSON_dictionary*>(this_v);

    JSON_array* sch_arr = dynamic_cast<JSON_array*>(sch_v);
    JSON_dictionary* sch_dict = dynamic_cast<JSON_dictionary*>(sch_v);

    JSON_string* sch_str = dynamic_cast<JSON_string*>(sch_v);

    std::string err_prefix;
    if (prefix.empty()) {
        err_prefix = "top-level object";
    } else {
        err_prefix = "json key \"" + prefix + "\"";
    }

    std::string pattern_key;
    if (sch_dict) {
        if (!this_dict) {
            QTC::TC("libtests", "JSON wanted dictionary");
            errors.push_back(err_prefix + " is supposed to be a dictionary");
            return false;
        }
        auto members = sch_dict->members;
        std::string key;
        if ((members.size() == 1) &&
            ((key = members.begin()->first, key.length() > 2) &&
             (key.at(0) == '<') && (key.at(key.length() - 1) == '>'))) {
            pattern_key = key;
        }
    }

    if (sch_dict && (!pattern_key.empty())) {
        auto pattern_schema = sch_dict->members[pattern_key].get();
        for (auto const& iter: this_dict->members) {
            std::string const& key = iter.first;
            checkSchemaInternal(
                this_dict->members[key].get(),
                pattern_schema,
                flags,
                errors,
                prefix + "." + key);
        }
    } else if (sch_dict) {
        for (auto& iter: sch_dict->members) {
            std::string const& key = iter.first;
            if (this_dict->members.count(key)) {
                checkSchemaInternal(
                    this_dict->members[key].get(),
                    iter.second.get(),
                    flags,
                    errors,
                    prefix + "." + key);
            } else {
                if (flags & f_optional) {
                    QTC::TC("libtests", "JSON optional key");
                } else {
                    QTC::TC("libtests", "JSON key missing in object");
                    errors.push_back(
                        err_prefix + ": key \"" + key +
                        "\" is present in schema but missing in object");
                }
            }
        }
        for (auto const& iter: this_dict->members) {
            std::string const& key = iter.first;
            if (sch_dict->members.count(key) == 0) {
                QTC::TC("libtests", "JSON key extra in object");
                errors.push_back(
                    err_prefix + ": key \"" + key +
                    "\" is not present in schema but appears in object");
            }
        }
    } else if (sch_arr) {
        auto n_elements = sch_arr->elements.size();
        if (n_elements == 1) {
            // A single-element array in the schema allows a single
            // element in the object or a variable-length array, each
            // of whose items must conform to the single element of
            // the schema array. This doesn't apply to arrays of
            // arrays -- we fall back to the behavior of allowing a
            // single item only when the object is not an array.
            if (this_arr) {
                int i = 0;
                for (auto const& element: this_arr->elements) {
                    checkSchemaInternal(
                        element.get(),
                        sch_arr->elements.at(0).get(),
                        flags,
                        errors,
                        prefix + "." + QUtil::int_to_string(i));
                    ++i;
                }
            } else {
                QTC::TC("libtests", "JSON schema array for single item");
                checkSchemaInternal(
                    this_v,
                    sch_arr->elements.at(0).get(),
                    flags,
                    errors,
                    prefix);
            }
        } else if (!this_arr || (this_arr->elements.size() != n_elements)) {
            QTC::TC("libtests", "JSON schema array length mismatch");
            errors.push_back(
                err_prefix + " is supposed to be an array of length " +
                QUtil::uint_to_string(n_elements));
            return false;
        } else {
            // A multi-element array in the schema must correspond to
            // an element of the same length in the object. Each
            // element in the object is validated against the
            // corresponding element in the schema.
            size_t i = 0;
            for (auto const& element: this_arr->elements) {
                checkSchemaInternal(
                    element.get(),
                    sch_arr->elements.at(i).get(),
                    flags,
                    errors,
                    prefix + "." + QUtil::uint_to_string(i));
                ++i;
            }
        }
    } else if (!sch_str) {
        QTC::TC("libtests", "JSON schema other type");
        errors.push_back(
            err_prefix + " schema value is not dictionary, array, or string");
        return false;
    }

    return errors.empty();
}

namespace
{
    class JSONParser
    {
      public:
        JSONParser(InputSource& is, JSON::Reactor* reactor) :
            is(is),
            reactor(reactor),
            lex_state(ls_top),
            number_before_point(0),
            number_after_point(0),
            number_after_e(0),
            number_saw_point(false),
            number_saw_e(false),
            bytes(0),
            p(buf),
            u_count(0),
            offset(0),
            done(false),
            parser_state(ps_top),
            dict_key_offset(0)
        {
        }

        std::shared_ptr<JSON> parse();

      private:
        void getToken();
        void handleToken();
        static std::string
        decode_string(std::string const& json, qpdf_offset_t offset);
        static void handle_u_code(
            char const* s,
            qpdf_offset_t offset,
            qpdf_offset_t i,
            unsigned long& high_surrogate,
            qpdf_offset_t& high_offset,
            std::string& result);

        enum parser_state_e {
            ps_top,
            ps_dict_begin,
            ps_dict_after_key,
            ps_dict_after_colon,
            ps_dict_after_item,
            ps_dict_after_comma,
            ps_array_begin,
            ps_array_after_item,
            ps_array_after_comma,
            ps_done,
        };

        enum lex_state_e {
            ls_top,
            ls_number,
            ls_alpha,
            ls_string,
            ls_backslash,
            ls_u4,
        };

        InputSource& is;
        JSON::Reactor* reactor;
        lex_state_e lex_state;
        size_t number_before_point;
        size_t number_after_point;
        size_t number_after_e;
        bool number_saw_point;
        bool number_saw_e;
        char buf[16384];
        size_t bytes;
        char const* p;
        qpdf_offset_t u_count;
        qpdf_offset_t offset;
        bool done;
        std::string token;
        parser_state_e parser_state;
        std::vector<std::shared_ptr<JSON>> stack;
        std::vector<parser_state_e> ps_stack;
        std::string dict_key;
        qpdf_offset_t dict_key_offset;
    };
} // namespace

void
JSONParser::handle_u_code(
    char const* s,
    qpdf_offset_t offset,
    qpdf_offset_t i,
    unsigned long& high_surrogate,
    qpdf_offset_t& high_offset,
    std::string& result)
{
    std::string hex = QUtil::hex_decode(std::string(s + i + 1, s + i + 5));
    unsigned char high = static_cast<unsigned char>(hex.at(0));
    unsigned char low = static_cast<unsigned char>(hex.at(1));
    unsigned long codepoint = high;
    codepoint <<= 8;
    codepoint += low;
    if ((codepoint & 0xFC00) == 0xD800) {
        // high surrogate
        qpdf_offset_t new_high_offset = offset + i;
        if (high_offset) {
            QTC::TC("libtests", "JSON 16 high high");
            throw std::runtime_error(
                "JSON: offset " + QUtil::int_to_string(new_high_offset) +
                ": UTF-16 high surrogate found after previous high surrogate"
                " at offset " +
                QUtil::int_to_string(high_offset));
        }
        high_offset = new_high_offset;
        high_surrogate = codepoint;
    } else if ((codepoint & 0xFC00) == 0xDC00) {
        // low surrogate
        if (offset + i != (high_offset + 6)) {
            QTC::TC("libtests", "JSON 16 low not after high");
            throw std::runtime_error(
                "JSON: offset " + QUtil::int_to_string(offset + i) +
                ": UTF-16 low surrogate found not immediately after high"
                " surrogate");
        }
        high_offset = 0;
        codepoint =
            0x10000U + ((high_surrogate & 0x3FFU) << 10U) + (codepoint & 0x3FF);
        result += QUtil::toUTF8(codepoint);
    } else {
        result += QUtil::toUTF8(codepoint);
    }
}

std::string
JSONParser::decode_string(std::string const& str, qpdf_offset_t offset)
{
    // The string has already been validated when this private method
    // is called, so errors are logic errors instead of runtime
    // errors.
    size_t len = str.length();
    if ((len < 2) || (str.at(0) != '"') || (str.at(len - 1) != '"')) {
        throw std::logic_error(
            "JSON Parse: decode_string called with other than \"...\"");
    }
    char const* s = str.c_str();
    // Move inside the quotation marks
    ++s;
    len -= 2;
    // Keep track of UTF-16 surrogate pairs.
    unsigned long high_surrogate = 0;
    qpdf_offset_t high_offset = 0;
    std::string result;
    qpdf_offset_t olen = toO(len);
    for (qpdf_offset_t i = 0; i < olen; ++i) {
        if (s[i] == '\\') {
            if (i + 1 >= olen) {
                throw std::logic_error("JSON parse: nothing after \\");
            }
            char ch = s[++i];
            switch (ch) {
            case '\\':
            case '\"':
            case '/':
                // \/ is allowed in json input, but so is /, so we
                // don't map / to \/ in output.
                result.append(1, ch);
                break;
            case 'b':
                result.append(1, '\b');
                break;
            case 'f':
                result.append(1, '\f');
                break;
            case 'n':
                result.append(1, '\n');
                break;
            case 'r':
                result.append(1, '\r');
                break;
            case 't':
                result.append(1, '\t');
                break;
            case 'u':
                if (i + 4 >= olen) {
                    throw std::logic_error(
                        "JSON parse: not enough characters after \\u");
                }
                handle_u_code(
                    s, offset, i, high_surrogate, high_offset, result);
                i += 4;
                break;
            default:
                throw std::logic_error("JSON parse: bad character after \\");
                break;
            }
        } else {
            result.append(1, s[i]);
        }
    }
    if (high_offset) {
        QTC::TC("libtests", "JSON 16 dangling high");
        throw std::runtime_error(
            "JSON: offset " + QUtil::int_to_string(high_offset) +
            ": UTF-16 high surrogate not followed by low surrogate");
    }
    return result;
}

void
JSONParser::getToken()
{
    enum { append, ignore, reread } action = append;
    bool ready = false;
    token.clear();
    while (!done) {
        if (p == (buf + bytes)) {
            p = buf;
            bytes = is.read(buf, sizeof(buf));
            if (bytes == 0) {
                done = true;
                break;
            }
        }

        if (*p == 0) {
            QTC::TC("libtests", "JSON parse null character");
            throw std::runtime_error(
                "JSON: null character at offset " +
                QUtil::int_to_string(offset));
        }
        action = append;
        switch (lex_state) {
        case ls_top:
            if (*p == '"') {
                lex_state = ls_string;
            } else if (QUtil::is_space(*p)) {
                action = ignore;
            } else if ((*p >= 'a') && (*p <= 'z')) {
                lex_state = ls_alpha;
            } else if (*p == '-') {
                lex_state = ls_number;
                number_before_point = 0;
                number_after_point = 0;
                number_after_e = 0;
                number_saw_point = false;
                number_saw_e = false;
            } else if ((*p >= '0') && (*p <= '9')) {
                lex_state = ls_number;
                number_before_point = 1;
                number_after_point = 0;
                number_after_e = 0;
                number_saw_point = false;
                number_saw_e = false;
            } else if (*p == '.') {
                lex_state = ls_number;
                number_before_point = 0;
                number_after_point = 0;
                number_after_e = 0;
                number_saw_point = true;
                number_saw_e = false;
            } else if (strchr("{}[]:,", *p)) {
                ready = true;
            } else {
                QTC::TC("libtests", "JSON parse bad character");
                throw std::runtime_error(
                    "JSON: offset " + QUtil::int_to_string(offset) +
                    ": unexpected character " + std::string(p, 1));
            }
            break;

        case ls_number:
            if ((*p >= '0') && (*p <= '9')) {
                if (number_saw_e) {
                    ++number_after_e;
                } else if (number_saw_point) {
                    ++number_after_point;
                } else {
                    ++number_before_point;
                }
            } else if (*p == '.') {
                if (number_saw_e) {
                    QTC::TC("libtests", "JSON parse point after e");
                    throw std::runtime_error(
                        "JSON: offset " + QUtil::int_to_string(offset) +
                        ": numeric literal: decimal point after e");
                } else if (number_saw_point) {
                    QTC::TC("libtests", "JSON parse duplicate point");
                    throw std::runtime_error(
                        "JSON: offset " + QUtil::int_to_string(offset) +
                        ": numeric literal: decimal point already seen");
                } else {
                    number_saw_point = true;
                }
            } else if (*p == 'e') {
                if (number_saw_e) {
                    QTC::TC("libtests", "JSON parse duplicate e");
                    throw std::runtime_error(
                        "JSON: offset " + QUtil::int_to_string(offset) +
                        ": numeric literal: e already seen");
                } else {
                    number_saw_e = true;
                }
            } else if ((*p == '+') || (*p == '-')) {
                if (number_saw_e && (number_after_e == 0)) {
                    // okay
                } else {
                    QTC::TC("libtests", "JSON parse unexpected sign");
                    throw std::runtime_error(
                        "JSON: offset " + QUtil::int_to_string(offset) +
                        ": numeric literal: unexpected sign");
                }
            } else if (QUtil::is_space(*p)) {
                action = ignore;
                ready = true;
            } else if (strchr("{}[]:,", *p)) {
                action = reread;
                ready = true;
            } else {
                QTC::TC("libtests", "JSON parse numeric bad character");
                throw std::runtime_error(
                    "JSON: offset " + QUtil::int_to_string(offset) +
                    ": numeric literal: unexpected character " +
                    std::string(p, 1));
            }
            break;

        case ls_alpha:
            if ((*p >= 'a') && (*p <= 'z')) {
                // okay
            } else if (QUtil::is_space(*p)) {
                action = ignore;
                ready = true;
            } else if (strchr("{}[]:,", *p)) {
                action = reread;
                ready = true;
            } else {
                QTC::TC("libtests", "JSON parse keyword bad character");
                throw std::runtime_error(
                    "JSON: offset " + QUtil::int_to_string(offset) +
                    ": keyword: unexpected character " + std::string(p, 1));
            }
            break;

        case ls_string:
            if (*p == '"') {
                ready = true;
            } else if (*p == '\\') {
                lex_state = ls_backslash;
            }
            break;

        case ls_backslash:
            /* cSpell: ignore bfnrt */
            if (strchr("\\\"/bfnrt", *p)) {
                lex_state = ls_string;
            } else if (*p == 'u') {
                lex_state = ls_u4;
                u_count = 0;
            } else {
                QTC::TC("libtests", "JSON parse backslash bad character");
                throw std::runtime_error(
                    "JSON: offset " + QUtil::int_to_string(offset) +
                    ": invalid character after backslash: " +
                    std::string(p, 1));
            }
            break;

        case ls_u4:
            if (!QUtil::is_hex_digit(*p)) {
                QTC::TC("libtests", "JSON parse bad hex after u");
                throw std::runtime_error(
                    "JSON: offset " +
                    QUtil::int_to_string(offset - u_count - 1) +
                    ": \\u must be followed by four hex digits");
            }
            if (++u_count == 4) {
                lex_state = ls_string;
            }
            break;
        }
        switch (action) {
        case reread:
            break;
        case append:
            token.append(1, *p);
            // fall through
        case ignore:
            ++p;
            ++offset;
            break;
        }
        if (ready) {
            break;
        }
    }
    if (done) {
        if ((!token.empty()) && (!ready)) {
            switch (lex_state) {
            case ls_top:
                // Can't happen
                throw std::logic_error("tok_start set in ls_top while parsing");
                break;

            case ls_number:
            case ls_alpha:
                // okay
                break;

            case ls_u4:
                QTC::TC("libtests", "JSON parse premature end of u");
                throw std::runtime_error(
                    "JSON: offset " +
                    QUtil::int_to_string(offset - u_count - 1) +
                    ": \\u must be followed by four characters");

            case ls_string:
            case ls_backslash:
                QTC::TC("libtests", "JSON parse unterminated string");
                throw std::runtime_error(
                    "JSON: offset " + QUtil::int_to_string(offset) +
                    ": unterminated string");
                break;
            }
        }
    }
}

void
JSONParser::handleToken()
{
    if (token.empty()) {
        return;
    }

    if (parser_state == ps_done) {
        QTC::TC("libtests", "JSON parse junk after object");
        throw std::runtime_error(
            "JSON: offset " + QUtil::int_to_string(offset) +
            ": material follows end of object: " + token);
    }

    // Git string value
    std::string s_value;
    if (lex_state == ls_string) {
        // Token includes the quotation marks
        if (token.length() < 2) {
            throw std::logic_error("JSON string length < 2");
        }
        s_value = decode_string(token, offset - toO(token.length()));
    }
    // Based on the lexical state and value, figure out whether we are
    // looking at an item or a delimiter. It will always be exactly
    // one of those two or an error condition.

    std::shared_ptr<JSON> item;
    char delimiter = '\0';
    // Already verified that token is not empty
    char first_char = token.at(0);
    switch (lex_state) {
    case ls_top:
        switch (first_char) {
        case '{':
            item = std::make_shared<JSON>(JSON::makeDictionary());
            item->setStart(offset - toO(token.length()));
            break;

        case '[':
            item = std::make_shared<JSON>(JSON::makeArray());
            item->setStart(offset - toO(token.length()));
            break;

        default:
            delimiter = first_char;
            break;
        }
        break;

    case ls_number:
        if (number_saw_point && (number_after_point == 0)) {
            QTC::TC("libtests", "JSON parse decimal with no digits");
            throw std::runtime_error(
                "JSON: offset " + QUtil::int_to_string(offset) +
                ": decimal point with no digits");
        }
        if ((number_before_point > 1) &&
            ((first_char == '0') ||
             ((first_char == '-') && (token.at(1) == '0')))) {
            QTC::TC("libtests", "JSON parse leading zero");
            throw std::runtime_error(
                "JSON: offset " + QUtil::int_to_string(offset) +
                ": number with leading zero");
        }
        if ((number_before_point == 0) && (number_after_point == 0)) {
            QTC::TC("libtests", "JSON parse number no digits");
            throw std::runtime_error(
                "JSON: offset " + QUtil::int_to_string(offset) +
                ": number with no digits");
        }
        item = std::make_shared<JSON>(JSON::makeNumber(token));
        break;

    case ls_alpha:
        if (token == "true") {
            item = std::make_shared<JSON>(JSON::makeBool(true));
        } else if (token == "false") {
            item = std::make_shared<JSON>(JSON::makeBool(false));
        } else if (token == "null") {
            item = std::make_shared<JSON>(JSON::makeNull());
        } else {
            QTC::TC("libtests", "JSON parse invalid keyword");
            throw std::runtime_error(
                "JSON: offset " + QUtil::int_to_string(offset) +
                ": invalid keyword " + token);
        }
        break;

    case ls_string:
        item = std::make_shared<JSON>(JSON::makeString(s_value));
        break;

    case ls_backslash:
    case ls_u4:
        throw std::logic_error(
            "tok_end is set while state = ls_backslash or ls_u4");
        break;
    }

    if ((item.get() == nullptr) == (delimiter == '\0')) {
        throw std::logic_error(
            "JSONParser::handleToken: logic error: exactly one of item"
            " or delimiter must be set");
    }

    // See whether what we have is allowed at this point.

    if (item.get()) {
        switch (parser_state) {
        case ps_done:
            throw std::logic_error("can't happen; ps_done already handled");
            break;

        case ps_dict_after_key:
            QTC::TC("libtests", "JSON parse expected colon");
            throw std::runtime_error(
                "JSON: offset " + QUtil::int_to_string(offset) +
                ": expected ':'");
            break;

        case ps_dict_after_item:
            QTC::TC("libtests", "JSON parse expected , or }");
            throw std::runtime_error(
                "JSON: offset " + QUtil::int_to_string(offset) +
                ": expected ',' or '}'");
            break;

        case ps_array_after_item:
            QTC::TC("libtests", "JSON parse expected, or ]");
            throw std::runtime_error(
                "JSON: offset " + QUtil::int_to_string(offset) +
                ": expected ',' or ']'");
            break;

        case ps_dict_begin:
        case ps_dict_after_comma:
            if (lex_state != ls_string) {
                QTC::TC("libtests", "JSON parse string as dict key");
                throw std::runtime_error(
                    "JSON: offset " + QUtil::int_to_string(offset) +
                    ": expect string as dictionary key");
            }
            break;

        case ps_top:
        case ps_dict_after_colon:
        case ps_array_begin:
        case ps_array_after_comma:
            break;
            // okay
        }
    } else if (delimiter == '}') {
        if ((parser_state != ps_dict_begin) &&
            (parser_state != ps_dict_after_item))

        {
            QTC::TC("libtests", "JSON parse unexpected }");
            throw std::runtime_error(
                "JSON: offset " + QUtil::int_to_string(offset) +
                ": unexpected dictionary end delimiter");
        }
    } else if (delimiter == ']') {
        if ((parser_state != ps_array_begin) &&
            (parser_state != ps_array_after_item))

        {
            QTC::TC("libtests", "JSON parse unexpected ]");
            throw std::runtime_error(
                "JSON: offset " + QUtil::int_to_string(offset) +
                ": unexpected array end delimiter");
        }
    } else if (delimiter == ':') {
        if (parser_state != ps_dict_after_key) {
            QTC::TC("libtests", "JSON parse unexpected :");
            throw std::runtime_error(
                "JSON: offset " + QUtil::int_to_string(offset) +
                ": unexpected colon");
        }
    } else if (delimiter == ',') {
        if ((parser_state != ps_dict_after_item) &&
            (parser_state != ps_array_after_item)) {
            QTC::TC("libtests", "JSON parse unexpected ,");
            throw std::runtime_error(
                "JSON: offset " + QUtil::int_to_string(offset) +
                ": unexpected comma");
        }
    } else if (delimiter != '\0') {
        throw std::logic_error("JSONParser::handleToken: bad delimiter");
    }

    // Now we know we have a delimiter or item that is allowed. Do
    // whatever we need to do with it.

    parser_state_e next_state = ps_top;
    if (delimiter == ':') {
        next_state = ps_dict_after_colon;
    } else if (delimiter == ',') {
        if (parser_state == ps_dict_after_item) {
            next_state = ps_dict_after_comma;
        } else if (parser_state == ps_array_after_item) {
            next_state = ps_array_after_comma;
        } else {
            throw std::logic_error("JSONParser::handleToken: unexpected parser"
                                   " state for comma");
        }
    } else if ((delimiter == '}') || (delimiter == ']')) {
        next_state = ps_stack.back();
        ps_stack.pop_back();
        auto tos = stack.back();
        tos->setEnd(offset);
        if (reactor) {
            reactor->containerEnd(*tos);
        }
        if (next_state != ps_done) {
            stack.pop_back();
        }
    } else if (delimiter != '\0') {
        throw std::logic_error(
            "JSONParser::handleToken: unexpected delimiter in transition");
    } else if (item.get()) {
        if (!(item->isArray() || item->isDictionary())) {
            item->setStart(offset - toO(token.length()));
            item->setEnd(offset);
        }

        std::shared_ptr<JSON> tos;
        if (!stack.empty()) {
            tos = stack.back();
        }
        switch (parser_state) {
        case ps_dict_begin:
        case ps_dict_after_comma:
            this->dict_key = s_value;
            this->dict_key_offset = item->getStart();
            item = nullptr;
            next_state = ps_dict_after_key;
            break;

        case ps_dict_after_colon:
            if (tos->checkDictionaryKeySeen(dict_key)) {
                QTC::TC("libtests", "JSON parse duplicate key");
                throw std::runtime_error(
                    "JSON: offset " + QUtil::int_to_string(dict_key_offset) +
                    ": duplicated dictionary key");
            }
            if (!reactor || !reactor->dictionaryItem(dict_key, *item)) {
                tos->addDictionaryMember(dict_key, *item);
            }
            next_state = ps_dict_after_item;
            break;

        case ps_array_begin:
        case ps_array_after_comma:
            if (!reactor || !reactor->arrayItem(*item)) {
                tos->addArrayElement(*item);
            }
            next_state = ps_array_after_item;
            break;

        case ps_top:
            next_state = ps_done;
            break;

        case ps_dict_after_key:
        case ps_dict_after_item:
        case ps_array_after_item:
        case ps_done:
            throw std::logic_error(
                "JSONParser::handleToken: unexpected parser state");
        }
    } else {
        throw std::logic_error(
            "JSONParser::handleToken: unexpected null item in transition");
    }

    if (reactor && item.get()) {
        // Calling container start method is postponed until after
        // adding the containers to their parent containers, if any.
        // This makes it much easier to keep track of the current
        // nesting level.
        if (item->isDictionary()) {
            reactor->dictionaryStart();
        } else if (item->isArray()) {
            reactor->arrayStart();
        }
    }

    // Prepare for next token
    if (item.get()) {
        if (item->isDictionary()) {
            stack.push_back(item);
            ps_stack.push_back(next_state);
            next_state = ps_dict_begin;
        } else if (item->isArray()) {
            stack.push_back(item);
            ps_stack.push_back(next_state);
            next_state = ps_array_begin;
        } else if (parser_state == ps_top) {
            stack.push_back(item);
        }
    }
    if (ps_stack.size() > 500) {
        throw std::runtime_error(
            "JSON: offset " + QUtil::int_to_string(offset) +
            ": maximum object depth exceeded");
    }
    parser_state = next_state;
    lex_state = ls_top;
}

std::shared_ptr<JSON>
JSONParser::parse()
{
    while (!done) {
        getToken();
        handleToken();
    }
    if (parser_state != ps_done) {
        QTC::TC("libtests", "JSON parse premature EOF");
        throw std::runtime_error("JSON: premature end of input");
    }
    auto const& tos = stack.back();
    if (reactor && tos.get() && !(tos->isArray() || tos->isDictionary())) {
        reactor->topLevelScalar();
    }
    return tos;
}

JSON
JSON::parse(InputSource& is, Reactor* reactor)
{
    JSONParser jp(is, reactor);
    return *jp.parse();
}

JSON
JSON::parse(std::string const& s)
{
    BufferInputSource bis("json input", s);
    JSONParser jp(bis, nullptr);
    return *jp.parse();
}

void
JSON::setStart(qpdf_offset_t start)
{
    this->m->start = start;
}

void
JSON::setEnd(qpdf_offset_t end)
{
    this->m->end = end;
}

qpdf_offset_t
JSON::getStart() const
{
    return this->m->start;
}

qpdf_offset_t
JSON::getEnd() const
{
    return this->m->end;
}
