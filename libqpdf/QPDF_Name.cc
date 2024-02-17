#include <qpdf/QPDF_Name.hh>

#include <qpdf/JSON_writer.hh>
#include <qpdf/QUtil.hh>

#include <string_view>

QPDF_Name::QPDF_Name(std::string const& name) :
    QPDFValue(::ot_name, "name"),
    name(name)
{
}

std::shared_ptr<QPDFObject>
QPDF_Name::create(std::string const& name)
{
    return do_create(new QPDF_Name(name));
}

std::shared_ptr<QPDFObject>
QPDF_Name::copy(bool shallow)
{
    return create(name);
}

std::string
QPDF_Name::normalizeName(std::string const& name)
{
    if (name.empty()) {
        return name;
    }
    std::string result;
    result += name.at(0);
    for (size_t i = 1; i < name.length(); ++i) {
        char ch = name.at(i);
        // Don't use locale/ctype here; follow PDF spec guidelines.
        if (ch == '\0') {
            // QPDFTokenizer embeds a null character to encode an invalid #.
            result += "#";
        } else if (
            ch < 33 || ch == '#' || ch == '/' || ch == '(' || ch == ')' || ch == '{' || ch == '}' ||
            ch == '<' || ch == '>' || ch == '[' || ch == ']' || ch == '%' || ch > 126) {
            result += QUtil::hex_encode_char(ch);
        } else {
            result += ch;
        }
    }
    return result;
}

std::string
QPDF_Name::unparse()
{
    return normalizeName(this->name);
}

std::pair<bool, bool>
QPDF_Name::analyzeJSONEncoding(const std::string& name)
{
    std::basic_string_view<unsigned char> view{
        reinterpret_cast<const unsigned char*>(name.data()), name.size()};

    int tail = 0;       // Number of continuation characters expected.
    bool tail2 = false; // Potential overlong 3 octet utf-8.
    bool tail3 = false; // potential overlong 4 octet
    bool needs_escaping = false;
    for (auto const& c: view) {
        if (tail) {
            if ((c & 0xc0) != 0x80) {
                return {false, false};
            }
            if (tail2) {
                if ((c & 0xe0) == 0x80) {
                    return {false, false};
                }
                tail2 = false;
            } else if (tail3) {
                if ((c & 0xf0) == 0x80) {
                    return {false, false};
                }
                tail3 = false;
            }
            tail--;
        } else if (c < 0x80) {
            if (!needs_escaping) {
                needs_escaping = !((c > 34 && c != '\\') || c == ' ' || c == 33);
            }
        } else if ((c & 0xe0) == 0xc0) {
            if ((c & 0xfe) == 0xc0) {
                return {false, false};
            }
            tail = 1;
        } else if ((c & 0xf0) == 0xe0) {
            tail2 = (c == 0xe0);
            tail = 2;
        } else if ((c & 0xf8) == 0xf0) {
            tail3 = (c == 0xf0);
            tail = 3;
        } else {
            return {false, false};
        }
    }
    return {tail == 0, !needs_escaping};
}

void
QPDF_Name::writeJSON(int json_version, JSON::Writer& p)
{
    // For performance reasons this code is duplicated in QPDF_Dictionary::writeJSON. When updating
    // this method make sure QPDF_Dictionary is also update.
    if (json_version == 1) {
        p << "\"" << JSON::Writer::encode_string(normalizeName(name)) << "\"";
    } else {
        if (auto res = analyzeJSONEncoding(name); res.first) {
            if (res.second) {
                p << "\"" << name << "\"";
            } else {
                p << "\"" << JSON::Writer::encode_string(name) << "\"";
            }
        } else {
            p << "\"n:" << JSON::Writer::encode_string(normalizeName(name)) << "\"";
        }
    }
}
