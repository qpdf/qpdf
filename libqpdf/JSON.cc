#include <qpdf/JSON.hh>

#include <qpdf/JSON_writer.hh>

#include <qpdf/BufferInputSource.hh>
#include <qpdf/Pl_Base64.hh>
#include <qpdf/Pl_Concatenate.hh>
#include <qpdf/Pl_String.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/Util.hh>

#include <cstring>
#include <stdexcept>

using namespace qpdf;

JSON::Members::Members(std::unique_ptr<JSON_value> value) :
    value(std::move(value))
{
}

JSON::JSON(std::unique_ptr<JSON_value> value) :
    m(new Members(std::move(value)))
{
}

void
JSON::writeClose(Pipeline* p, bool first, size_t depth, char const* delimiter)
{
    if (first) {
        *p << delimiter;
    } else {
        std::string s{"\n"};
        s.append(2 * depth, ' ');
        *p << s + delimiter;
    }
}

void
JSON::writeNext(Pipeline* p, bool& first, size_t depth)
{
    if (first) {
        first = false;
        std::string s{"\n"};
        s.append(2 * depth, ' ');
        *p << s;
    } else {
        std::string s{",\n"};
        s.append(2 * depth, ' ');
        *p << s;
    }
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
JSON::writeDictionaryKey(Pipeline* p, bool& first, std::string const& key, size_t depth)
{
    writeNext(p, first, depth);
    *p << std::string("\"") + key + "\": ";
}

void
JSON::writeDictionaryItem(
    Pipeline* p, bool& first, std::string const& key, JSON const& value, size_t depth)
{
    writeDictionaryKey(p, first, key, depth);
    value.write(p, depth);
}

void
JSON::writeArrayItem(Pipeline* p, bool& first, JSON const& element, size_t depth)
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
    JSON_value(vt_string),
    utf8(utf8)
{
}

void
JSON::JSON_string::write(Pipeline* p, size_t) const
{
    *p << std::string("\"") + Writer::encode_string(utf8) + "\"";
}

JSON::JSON_number::JSON_number(long long value) :
    JSON_value(vt_number),
    encoded(std::to_string(value))
{
}

JSON::JSON_number::JSON_number(double value) :
    JSON_value(vt_number),
    encoded(QUtil::double_to_string(value, 6))
{
}

JSON::JSON_number::JSON_number(std::string const& value) :
    JSON_value(vt_number),
    encoded(value)
{
}

void
JSON::JSON_number::write(Pipeline* p, size_t) const
{
    *p << encoded;
}

JSON::JSON_bool::JSON_bool(bool val) :
    JSON_value(vt_bool),
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
    JSON_value(vt_blob),
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
    if (!m) {
        *p << "null";
    } else {
        m->value->write(p, depth);
    }
}

std::string
JSON::unparse() const
{
    if (!m) {
        return "null";
    }
    std::string s;
    Pl_String p("unparse", nullptr, s);
    write(&p, 0);
    return s;
}

std::string
JSON::Writer::encode_string(std::string const& str)
{
    static auto constexpr hexchars = "0123456789abcdef";

    auto begin = str.cbegin();
    auto end = str.cend();
    auto iter = begin;
    while (iter != end) {
        auto c = static_cast<unsigned char>(*iter);
        if ((c > 34 && c != '\\') || c == ' ' || c == 33) {
            // Optimistically check that no char in str requires escaping. Hopefully we can just
            // return the input str.
            ++iter;
        } else {
            // We found a char that requires escaping. Initialize result to the chars scanned so
            // far, append/replace the rest of str one char at a time, and return the result.
            std::string result{begin, iter};

            for (; iter != end; ++iter) {
                auto ch = static_cast<unsigned char>(*iter);
                if ((ch > 34 && ch != '\\') || ch == ' ' || ch == 33) {
                    // Check for most common case first.
                    result += *iter;
                } else {
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
                        result += ch < 16 ? "\\u000" : "\\u001";
                        result += hexchars[ch % 16];
                    }
                }
            }
            return result;
        }
    }
    return str;
}

JSON
JSON::makeDictionary()
{
    return {std::make_unique<JSON_dictionary>()};
}

JSON
JSON::addDictionaryMember(std::string const& key, JSON const& val)
{
    if (auto* obj = m ? dynamic_cast<JSON_dictionary*>(m->value.get()) : nullptr) {
        return obj->members[Writer::encode_string(key)] = val.m ? val : makeNull();
    } else {
        throw std::runtime_error("JSON::addDictionaryMember called on non-dictionary");
    }
}

JSON
JSON::makeArray()
{
    return {std::make_unique<JSON_array>()};
}

JSON
JSON::addArrayElement(JSON const& val)
{
    if (auto* arr = m ? dynamic_cast<JSON_array*>(m->value.get()) : nullptr) {
        if (val.m) {
            arr->elements.push_back(val);
        } else {
            arr->elements.push_back(makeNull());
        }
        return arr->elements.back();
    }
    throw std::runtime_error("JSON::addArrayElement called on non-array");
    return {}; // unreachable
}

JSON
JSON::makeString(std::string const& utf8)
{
    return {std::make_unique<JSON_string>(utf8)};
}

JSON
JSON::makeInt(long long int value)
{
    return {std::make_unique<JSON_number>(value)};
}

JSON
JSON::makeReal(double value)
{
    return {std::make_unique<JSON_number>(value)};
}

JSON
JSON::makeNumber(std::string const& encoded)
{
    return {std::make_unique<JSON_number>(encoded)};
}

JSON
JSON::makeBool(bool value)
{
    return {std::make_unique<JSON_bool>(value)};
}

JSON
JSON::makeNull()
{
    return {std::make_unique<JSON_null>()};
}

JSON
JSON::makeBlob(std::function<void(Pipeline*)> fn)
{
    return {std::make_unique<JSON_blob>(fn)};
}

bool
JSON::isArray() const
{
    return m ? m->value->type_code == vt_array : false;
}

bool
JSON::isDictionary() const
{
    return m && m->value->type_code == vt_dictionary;
}

bool
JSON::getString(std::string& utf8) const
{
    if (m && m->value->type_code == vt_string) {
        auto v = dynamic_cast<JSON_string const*>(m->value.get());
        utf8 = v->utf8;
        return true;
    }
    return false;
}

bool
JSON::getNumber(std::string& value) const
{
    if (m && m->value->type_code == vt_number) {
        auto v = dynamic_cast<JSON_number const*>(m->value.get());
        value = v->encoded;
        return true;
    }
    return false;
}

bool
JSON::getBool(bool& value) const
{
    if (m && m->value->type_code == vt_bool) {
        auto v = dynamic_cast<JSON_bool const*>(m->value.get());
        value = v->value;
        return true;
    }
    return false;
}

bool
JSON::isNull() const
{
    return m && m->value->type_code == vt_null;
}

JSON
JSON::getDictItem(std::string const& key) const
{
    if (auto v = m ? dynamic_cast<JSON_dictionary const*>(m->value.get()) : nullptr) {
        if (auto it = v->members.find(key); it != v->members.end()) {
            return it->second;
        }
    }
    return makeNull();
}

bool
JSON::forEachDictItem(std::function<void(std::string const& key, JSON value)> fn) const
{
    if (auto v = m ? dynamic_cast<JSON_dictionary const*>(m->value.get()) : nullptr) {
        for (auto const& [key, value]: v->members) {
            fn(key, value);
        }
        return true;
    }
    return false;
}

bool
JSON::forEachArrayItem(std::function<void(JSON value)> fn) const
{
    if (auto v = m ? dynamic_cast<JSON_array const*>(m->value.get()) : nullptr) {
        for (auto const& i: v->elements) {
            fn(JSON(i));
        }
        return true;
    }
    return false;
}

bool
JSON::checkSchema(JSON schema, std::list<std::string>& errors)
{
    return m && checkSchemaInternal(m->value.get(), schema.m->value.get(), 0, errors, "");
}

bool
JSON::checkSchema(JSON schema, unsigned long flags, std::list<std::string>& errors)
{
    return m && checkSchemaInternal(m->value.get(), schema.m->value.get(), flags, errors, "");
}

bool
JSON::checkSchemaInternal(
    JSON_value* this_v,
    JSON_value* sch_v,
    unsigned long flags,
    std::list<std::string>& errors,
    std::string prefix)
{
    auto* this_arr = dynamic_cast<JSON_array*>(this_v);
    auto* this_dict = dynamic_cast<JSON_dictionary*>(this_v);

    auto* sch_arr = dynamic_cast<JSON_array*>(sch_v);
    auto* sch_dict = dynamic_cast<JSON_dictionary*>(sch_v);

    auto* sch_str = dynamic_cast<JSON_string*>(sch_v);

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
            ((key = members.begin()->first, key.length() > 2) && (key.at(0) == '<') &&
             (key.at(key.length() - 1) == '>'))) {
            pattern_key = key;
        }
    }

    if (sch_dict && !pattern_key.empty()) {
        auto pattern_schema = sch_dict->members[pattern_key].m->value.get();
        for (auto const& [key, val]: this_dict->members) {
            checkSchemaInternal(
                val.m->value.get(),
                pattern_schema,
                flags,
                errors,
                prefix + "." + key);
        }
    } else if (sch_dict) {
        for (auto& [key, val]: sch_dict->members) {
            if (this_dict->members.contains(key)) {
                checkSchemaInternal(
                    this_dict->members[key].m->value.get(),
                    val.m->value.get(),
                    flags,
                    errors,
                    prefix + "." + key);
            } else {
                if (flags & f_optional) {
                    QTC::TC("libtests", "JSON optional key");
                } else {
                    QTC::TC("libtests", "JSON key missing in object");
                    errors.emplace_back(
                        err_prefix + ": key \"" + key +
                        "\" is present in schema but missing in object");
                }
            }
        }
        for (auto const& [key, val]: this_dict->members) {
            if (!sch_dict->members.contains(key)) {
                QTC::TC("libtests", "JSON key extra in object");
                errors.emplace_back(
                    err_prefix + ": key \"" + key +
                    "\" is not present in schema but appears in object");
            }
        }
    } else if (sch_arr) {
        auto n_elements = sch_arr->elements.size();
        if (n_elements == 1) {
            // A single-element array in the schema allows a single element in the object or a
            // variable-length array, each of whose items must conform to the single element of the
            // schema array. This doesn't apply to arrays of arrays -- we fall back to the behavior
            // of allowing a single item only when the object is not an array.
            if (this_arr) {
                int i = 0;
                for (auto const& element: this_arr->elements) {
                    checkSchemaInternal(
                        element.m->value.get(),
                        sch_arr->elements.at(0).m->value.get(),
                        flags,
                        errors,
                        prefix + "." + std::to_string(i));
                    ++i;
                }
            } else {
                QTC::TC("libtests", "JSON schema array for single item");
                checkSchemaInternal(
                    this_v, sch_arr->elements.at(0).m->value.get(), flags, errors, prefix);
            }
        } else if (!this_arr || this_arr->elements.size() != n_elements) {
            QTC::TC("libtests", "JSON schema array length mismatch");
            errors.emplace_back(
                err_prefix + " is supposed to be an array of length " + std::to_string(n_elements));
            return false;
        } else {
            // A multi-element array in the schema must correspond to an element of the same length
            // in the object. Each element in the object is validated against the corresponding
            // element in the schema.
            size_t i = 0;
            for (auto const& element: this_arr->elements) {
                checkSchemaInternal(
                    element.m->value.get(),
                    sch_arr->elements.at(i).m->value.get(),
                    flags,
                    errors,
                    prefix + "." + std::to_string(i));
                ++i;
            }
        }
    } else if (!sch_str) {
        QTC::TC("libtests", "JSON schema other type");
        errors.emplace_back(err_prefix + " schema value is not dictionary, array, or string");
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
            p(buf)
        {
        }

        JSON parse();

      private:
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
            ls_number_minus,
            ls_number_leading_zero,
            ls_number_before_point,
            ls_number_point,
            ls_number_after_point,
            ls_number_e,
            ls_number_e_sign,
            ls_alpha,
            ls_string,
            ls_after_string,
            ls_backslash,
            ls_u4,
            ls_begin_array,
            ls_end_array,
            ls_begin_dict,
            ls_end_dict,
            ls_colon,
            ls_comma,
        };

        struct StackFrame
        {
            StackFrame(parser_state_e state, JSON& item) :
                state(state),
                item(item)
            {
            }

            parser_state_e state;
            JSON item;
        };

        void getToken();
        void handleToken();
        void tokenError();
        static void handle_u_code(
            unsigned long codepoint,
            qpdf_offset_t offset,
            unsigned long& high_surrogate,
            qpdf_offset_t& high_offset,
            std::string& result);
        inline void append();
        inline void append(lex_state_e);
        inline void ignore();
        inline void ignore(lex_state_e);

        InputSource& is;
        JSON::Reactor* reactor;
        lex_state_e lex_state{ls_top};
        char buf[16384];
        size_t bytes{0};
        char const* p;
        qpdf_offset_t u_count{0};
        unsigned long u_value{0};
        qpdf_offset_t offset{0};
        bool done{false};
        std::string token;
        qpdf_offset_t token_start{0};
        parser_state_e parser_state{ps_top};
        std::vector<StackFrame> stack;
        std::string dict_key;
        qpdf_offset_t dict_key_offset{0};
    };
} // namespace

void
JSONParser::handle_u_code(
    unsigned long codepoint,
    qpdf_offset_t offset,
    unsigned long& high_surrogate,
    qpdf_offset_t& high_offset,
    std::string& result)
{
    if ((codepoint & 0xFC00) == 0xD800) {
        // high surrogate
        qpdf_offset_t new_high_offset = offset;
        if (high_offset) {
            QTC::TC("libtests", "JSON 16 high high");
            throw std::runtime_error(
                "JSON: offset " + std::to_string(new_high_offset) +
                ": UTF-16 high surrogate found after previous high surrogate at offset " +
                std::to_string(high_offset));
        }
        high_offset = new_high_offset;
        high_surrogate = codepoint;
    } else if ((codepoint & 0xFC00) == 0xDC00) {
        // low surrogate
        if (offset != (high_offset + 6)) {
            QTC::TC("libtests", "JSON 16 low not after high");
            throw std::runtime_error(
                "JSON: offset " + std::to_string(offset) +
                ": UTF-16 low surrogate found not immediately after high surrogate");
        }
        high_offset = 0;
        codepoint = 0x10000U + ((high_surrogate & 0x3FFU) << 10U) + (codepoint & 0x3FF);
        result += QUtil::toUTF8(codepoint);
    } else {
        result += QUtil::toUTF8(codepoint);
    }
}

void
JSONParser::tokenError()
{
    if (done) {
        QTC::TC("libtests", "JSON parse ls premature end of input");
        throw std::runtime_error("JSON: premature end of input");
    }

    if (lex_state == ls_u4) {
        QTC::TC("libtests", "JSON parse bad hex after u");
        throw std::runtime_error(
            "JSON: offset " + std::to_string(offset - u_count - 1) +
            ": \\u must be followed by four hex digits");
    } else if (lex_state == ls_alpha) {
        QTC::TC("libtests", "JSON parse keyword bad character");
        throw std::runtime_error(
            "JSON: offset " + std::to_string(offset) + ": keyword: unexpected character " +
            std::string(p, 1));
    } else if (lex_state == ls_string) {
        QTC::TC("libtests", "JSON parse control char in string");
        throw std::runtime_error(
            "JSON: offset " + std::to_string(offset) +
            ": control character in string (missing \"?)");
    } else if (lex_state == ls_backslash) {
        QTC::TC("libtests", "JSON parse backslash bad character");
        throw std::runtime_error(
            "JSON: offset " + std::to_string(offset) +
            ": invalid character after backslash: " + std::string(p, 1));
    }

    if (*p == '.') {
        if (lex_state == ls_number || lex_state == ls_number_e || lex_state == ls_number_e_sign) {
            QTC::TC("libtests", "JSON parse point after e");
            throw std::runtime_error(
                "JSON: offset " + std::to_string(offset) +
                ": numeric literal: decimal point after e");
        } else {
            QTC::TC("libtests", "JSON parse duplicate point");
            throw std::runtime_error(
                "JSON: offset " + std::to_string(offset) +
                ": numeric literal: decimal point already seen");
        }
    } else if (*p == 'e' || *p == 'E') {
        QTC::TC("libtests", "JSON parse duplicate e");
        throw std::runtime_error(
            "JSON: offset " + std::to_string(offset) + ": numeric literal: e already seen");
    } else if ((*p == '+') || (*p == '-')) {
        QTC::TC("libtests", "JSON parse unexpected sign");
        throw std::runtime_error(
            "JSON: offset " + std::to_string(offset) + ": numeric literal: unexpected sign");
    } else if (util::is_space(*p) || strchr("{}[]:,", *p)) {
        QTC::TC("libtests", "JSON parse incomplete number");
        throw std::runtime_error(
            "JSON: offset " + std::to_string(offset) + ": numeric literal: incomplete number");

    } else {
        QTC::TC("libtests", "JSON parse numeric bad character");
        throw std::runtime_error(
            "JSON: offset " + std::to_string(offset) + ": numeric literal: unexpected character " +
            std::string(p, 1));
    }
    throw std::logic_error("JSON::tokenError : unhandled error");
}

// Append current character to token and advance to next input character.
inline void
JSONParser::append()
{
    token += *p;
    ++p;
    ++offset;
}

// Append current character to token, advance to next input character and transition to 'next' lexer
// state.
inline void
JSONParser::append(lex_state_e next)
{
    lex_state = next;
    token += *p;
    ++p;
    ++offset;
}

// Advance to next input character without appending the current character to token.
inline void
JSONParser::ignore()
{
    ++p;
    ++offset;
}

// Advance to next input character without appending the current character to token and transition
// to 'next' lexer state.
inline void
JSONParser::ignore(lex_state_e next)
{
    lex_state = next;
    ++p;
    ++offset;
}

void
JSONParser::getToken()
{
    token.clear();

    // Keep track of UTF-16 surrogate pairs.
    unsigned long high_surrogate = 0;
    qpdf_offset_t high_offset = 0;

    while (true) {
        if (p == (buf + bytes)) {
            p = buf;
            bytes = is.read(buf, sizeof(buf));
            if (bytes == 0) {
                done = true;
                break;
            }
        }

        if ((*p < 32 && *p >= 0)) {
            if (*p == '\t' || *p == '\n' || *p == '\r') {
                // Legal white space not permitted in strings. This will always end the current
                // token (unless we are still before the start of the token).
                if (lex_state == ls_top) {
                    ignore();
                } else {
                    break;
                }

            } else {
                QTC::TC("libtests", "JSON parse null character");
                throw std::runtime_error(
                    "JSON: control or null character at offset " + std::to_string(offset));
            }
        } else if (*p == ',') {
            if (lex_state == ls_top) {
                ignore(ls_comma);
                return;
            } else if (lex_state == ls_string) {
                append();
            } else {
                break;
            }
        } else if (*p == ':') {
            if (lex_state == ls_top) {
                ignore(ls_colon);
                return;
            } else if (lex_state == ls_string) {
                append();
            } else {
                break;
            }
        } else if (*p == ' ') {
            if (lex_state == ls_top) {
                ignore();
            } else if (lex_state == ls_string) {
                append();
            } else {
                break;
            }
        } else if (*p == '{') {
            if (lex_state == ls_top) {
                token_start = offset;
                ignore(ls_begin_dict);
                return;
            } else if (lex_state == ls_string) {
                append();
            } else {
                break;
            }
        } else if (*p == '}') {
            if (lex_state == ls_top) {
                ignore(ls_end_dict);
                return;
            } else if (lex_state == ls_string) {
                append();
            } else {
                break;
            }
        } else if (*p == '[') {
            if (lex_state == ls_top) {
                token_start = offset;
                ignore(ls_begin_array);
                return;
            } else if (lex_state == ls_string) {
                append();
            } else {
                break;
            }
        } else if (*p == ']') {
            if (lex_state == ls_top) {
                ignore(ls_end_array);
                return;
            } else if (lex_state == ls_string) {
                append();
            } else {
                break;
            }
        } else {
            switch (lex_state) {
            case ls_top:
                token_start = offset;
                if (*p == '"') {
                    ignore(ls_string);
                } else if ((*p >= 'a') && (*p <= 'z')) {
                    append(ls_alpha);
                } else if (*p == '-') {
                    append(ls_number_minus);
                } else if ((*p >= '1') && (*p <= '9')) {
                    append(ls_number_before_point);
                } else if (*p == '0') {
                    append(ls_number_leading_zero);
                } else {
                    QTC::TC("libtests", "JSON parse bad character");
                    throw std::runtime_error(
                        "JSON: offset " + std::to_string(offset) + ": unexpected character " +
                        std::string(p, 1));
                }
                break;

            case ls_number_minus:
                if ((*p >= '1') && (*p <= '9')) {
                    append(ls_number_before_point);
                } else if (*p == '0') {
                    append(ls_number_leading_zero);
                } else {
                    QTC::TC("libtests", "JSON parse number minus no digits");
                    throw std::runtime_error(
                        "JSON: offset " + std::to_string(offset) +
                        ": numeric literal: no digit after minus sign");
                }
                break;

            case ls_number_leading_zero:
                if (*p == '.') {
                    append(ls_number_point);
                } else if (*p == 'e' || *p == 'E') {
                    append(ls_number_e);
                } else {
                    QTC::TC("libtests", "JSON parse leading zero");
                    throw std::runtime_error(
                        "JSON: offset " + std::to_string(offset) + ": number with leading zero");
                }
                break;

            case ls_number_before_point:
                if ((*p >= '0') && (*p <= '9')) {
                    append();
                } else if (*p == '.') {
                    append(ls_number_point);
                } else if (*p == 'e' || *p == 'E') {
                    append(ls_number_e);
                } else {
                    tokenError();
                }
                break;

            case ls_number_point:
                if ((*p >= '0') && (*p <= '9')) {
                    append(ls_number_after_point);
                } else {
                    tokenError();
                }
                break;

            case ls_number_after_point:
                if ((*p >= '0') && (*p <= '9')) {
                    append();
                } else if (*p == 'e' || *p == 'E') {
                    append(ls_number_e);
                } else {
                    tokenError();
                }
                break;

            case ls_number_e:
                if ((*p >= '0') && (*p <= '9')) {
                    append(ls_number);
                } else if ((*p == '+') || (*p == '-')) {
                    append(ls_number_e_sign);
                } else {
                    tokenError();
                }
                break;

            case ls_number_e_sign:
                if ((*p >= '0') && (*p <= '9')) {
                    append(ls_number);
                } else {
                    tokenError();
                }
                break;

            case ls_number:
                // We only get here after we have seen an exponent.
                if ((*p >= '0') && (*p <= '9')) {
                    append();
                } else {
                    tokenError();
                }
                break;

            case ls_alpha:
                if ((*p >= 'a') && (*p <= 'z')) {
                    append();
                } else {
                    tokenError();
                }
                break;

            case ls_string:
                if (*p == '"') {
                    if (high_offset) {
                        QTC::TC("libtests", "JSON 16 dangling high");
                        throw std::runtime_error(
                            "JSON: offset " + std::to_string(high_offset) +
                            ": UTF-16 high surrogate not followed by low surrogate");
                    }
                    ignore(ls_after_string);
                    return;
                } else if (*p == '\\') {
                    ignore(ls_backslash);
                } else {
                    append();
                }
                break;

            case ls_backslash:
                lex_state = ls_string;
                switch (*p) {
                case '\\':
                case '\"':
                case '/':
                    // \/ is allowed in json input, but so is /, so we don't map / to \/ in output.
                    token += *p;
                    break;
                case 'b':
                    token += '\b';
                    break;
                case 'f':
                    token += '\f';
                    break;
                case 'n':
                    token += '\n';
                    break;
                case 'r':
                    token += '\r';
                    break;
                case 't':
                    token += '\t';
                    break;
                case 'u':
                    lex_state = ls_u4;
                    u_count = 0;
                    u_value = 0;
                    break;
                default:
                    lex_state = ls_backslash;
                    tokenError();
                }
                ignore();
                break;

            case ls_u4:
                using ui = unsigned int;
                if (ui val = ui(util::hex_decode_char(*p)); val < 16) {
                    u_value = 16 * u_value + val;
                } else {
                    tokenError();
                }
                if (++u_count == 4) {
                    handle_u_code(u_value, offset - 5, high_surrogate, high_offset, token);
                    lex_state = ls_string;
                }
                ignore();
                break;

            default:
                throw std::logic_error("JSONParser::getToken : trying to handle delimiter state");
            }
        }
    }

    // We only get here if on end of input or if the last character was a control character or other
    // delimiter.

    if (!token.empty()) {
        switch (lex_state) {
        case ls_top:
            // Can't happen
            throw std::logic_error("tok_start set in ls_top while parsing");
            break;

        case ls_number_leading_zero:
        case ls_number_before_point:
        case ls_number_after_point:
            lex_state = ls_number;
            break;

        case ls_number:
        case ls_alpha:
            // terminal state
            break;

        default:
            tokenError();
        }
    }
}

void
JSONParser::handleToken()
{
    if (lex_state == ls_top) {
        return;
    }

    if (parser_state == ps_done) {
        QTC::TC("libtests", "JSON parse junk after object");
        throw std::runtime_error(
            "JSON: offset " + std::to_string(offset) +
            ": material follows end of object: " + token);
    }

    const static JSON null_item = JSON::makeNull();
    JSON item;
    auto tos = stack.empty() ? null_item : stack.back().item;
    auto ls = lex_state;
    lex_state = ls_top;

    switch (ls) {
    case ls_begin_dict:
        item = JSON::makeDictionary();
        break;

    case ls_begin_array:
        item = JSON::makeArray();
        break;

    case ls_colon:
        if (parser_state != ps_dict_after_key) {
            QTC::TC("libtests", "JSON parse unexpected :");
            throw std::runtime_error(
                "JSON: offset " + std::to_string(offset) + ": unexpected colon");
        }
        parser_state = ps_dict_after_colon;
        return;

    case ls_comma:
        if (!((parser_state == ps_dict_after_item) || (parser_state == ps_array_after_item))) {
            QTC::TC("libtests", "JSON parse unexpected ,");
            throw std::runtime_error(
                "JSON: offset " + std::to_string(offset) + ": unexpected comma");
        }
        if (parser_state == ps_dict_after_item) {
            parser_state = ps_dict_after_comma;
        } else if (parser_state == ps_array_after_item) {
            parser_state = ps_array_after_comma;
        } else {
            throw std::logic_error("JSONParser::handleToken: unexpected parser state for comma");
        }
        return;

    case ls_end_array:
        if (!(parser_state == ps_array_begin || parser_state == ps_array_after_item)) {
            QTC::TC("libtests", "JSON parse unexpected ]");
            throw std::runtime_error(
                "JSON: offset " + std::to_string(offset) + ": unexpected array end delimiter");
        }
        parser_state = stack.back().state;
        tos.setEnd(offset);
        if (reactor) {
            reactor->containerEnd(tos);
        }
        if (parser_state != ps_done) {
            stack.pop_back();
        }
        return;

    case ls_end_dict:
        if (!((parser_state == ps_dict_begin) || (parser_state == ps_dict_after_item))) {
            QTC::TC("libtests", "JSON parse unexpected }");
            throw std::runtime_error(
                "JSON: offset " + std::to_string(offset) + ": unexpected dictionary end delimiter");
        }
        parser_state = stack.back().state;
        tos.setEnd(offset);
        if (reactor) {
            reactor->containerEnd(tos);
        }
        if (parser_state != ps_done) {
            stack.pop_back();
        }
        return;

    case ls_number:
        item = JSON::makeNumber(token);
        break;

    case ls_alpha:
        if (token == "true") {
            item = JSON::makeBool(true);
        } else if (token == "false") {
            item = JSON::makeBool(false);
        } else if (token == "null") {
            item = JSON::makeNull();
        } else {
            QTC::TC("libtests", "JSON parse invalid keyword");
            throw std::runtime_error(
                "JSON: offset " + std::to_string(offset) + ": invalid keyword " + token);
        }
        break;

    case ls_after_string:
        if (parser_state == ps_dict_begin || parser_state == ps_dict_after_comma) {
            dict_key = token;
            dict_key_offset = token_start;
            parser_state = ps_dict_after_key;
            return;
        } else {
            item = JSON::makeString(token);
        }
        break;

    default:
        throw std::runtime_error(
            "JSON: offset " + std::to_string(offset) + ": premature end of input");
        break;
    }

    item.setStart(token_start);
    item.setEnd(offset);

    switch (parser_state) {
    case ps_dict_begin:
    case ps_dict_after_comma:
        QTC::TC("libtests", "JSON parse string as dict key");
        throw std::runtime_error(
            "JSON: offset " + std::to_string(offset) + ": expect string as dictionary key");
        break;

    case ps_dict_after_colon:
        if (!reactor || !reactor->dictionaryItem(dict_key, item)) {
            tos.addDictionaryMember(dict_key, item);
        }
        parser_state = ps_dict_after_item;
        break;

    case ps_array_begin:
    case ps_array_after_comma:
        if (!reactor || !reactor->arrayItem(item)) {
            tos.addArrayElement(item);
        }
        parser_state = ps_array_after_item;
        break;

    case ps_top:
        if (!(item.isDictionary() || item.isArray())) {
            stack.emplace_back(ps_done, item);
            parser_state = ps_done;
            return;
        }
        parser_state = ps_done;
        break;

    case ps_dict_after_key:
        QTC::TC("libtests", "JSON parse expected colon");
        throw std::runtime_error("JSON: offset " + std::to_string(offset) + ": expected ':'");
        break;

    case ps_dict_after_item:
        QTC::TC("libtests", "JSON parse expected , or }");
        throw std::runtime_error(
            "JSON: offset " + std::to_string(offset) + ": expected ',' or '}'");
        break;

    case ps_array_after_item:
        QTC::TC("libtests", "JSON parse expected, or ]");
        throw std::runtime_error(
            "JSON: offset " + std::to_string(offset) + ": expected ',' or ']'");
        break;

    case ps_done:
        throw std::logic_error("JSONParser::handleToken: unexpected parser state");
    }

    if (item.isDictionary() || item.isArray()) {
        stack.emplace_back(parser_state, item);
        // Calling container start method is postponed until after adding the containers to their
        // parent containers, if any. This makes it much easier to keep track of the current nesting
        // level.
        if (item.isDictionary()) {
            if (reactor) {
                reactor->dictionaryStart();
            }
            parser_state = ps_dict_begin;
        } else if (item.isArray()) {
            if (reactor) {
                reactor->arrayStart();
            }
            parser_state = ps_array_begin;
        }

        if (stack.size() > 500) {
            throw std::runtime_error(
                "JSON: offset " + std::to_string(offset) + ": maximum object depth exceeded");
        }
    }
}

JSON
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
    auto const& tos = stack.back().item;
    if (reactor && !(tos.isArray() || tos.isDictionary())) {
        reactor->topLevelScalar();
    }
    return tos;
}

JSON
JSON::parse(InputSource& is, Reactor* reactor)
{
    JSONParser jp(is, reactor);
    return jp.parse();
}

JSON
JSON::parse(std::string const& s)
{
    BufferInputSource bis("json input", s);
    JSONParser jp(bis, nullptr);
    return jp.parse();
}

void
JSON::setStart(qpdf_offset_t start)
{
    if (m) {
        m->start = start;
    }
}

void
JSON::setEnd(qpdf_offset_t end)
{
    if (m) {
        m->end = end;
    }
}

qpdf_offset_t
JSON::getStart() const
{
    return m ? m->start : 0;
}

qpdf_offset_t
JSON::getEnd() const
{
    return m ? m->end : 0;
}
