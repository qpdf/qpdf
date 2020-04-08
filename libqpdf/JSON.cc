#include <qpdf/JSON.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/QTC.hh>
#include <stdexcept>

JSON::Members::~Members()
{
}

JSON::Members::Members(PointerHolder<JSON_value> value) :
    value(value)
{
}

JSON::JSON(PointerHolder<JSON_value> value) :
    m(new Members(value))
{
}

JSON::JSON_value::~JSON_value()
{
}

JSON::JSON_dictionary::~JSON_dictionary()
{
}

std::string JSON::JSON_dictionary::unparse(size_t depth) const
{
    std::string result = "{";
    bool first = true;
    for (std::map<std::string, PointerHolder<JSON_value> >::const_iterator
             iter = members.begin();
         iter != members.end(); ++iter)
    {
        if (first)
        {
            first = false;
        }
        else
        {
            result.append(1, ',');
        }
        result.append(1, '\n');
        result.append(2 * (1 + depth), ' ');
        result += ("\"" + (*iter).first + "\": " +
                   (*iter).second->unparse(1 + depth));
    }
    if (! first)
    {
        result.append(1, '\n');
        result.append(2 * depth, ' ');
    }
    result.append(1, '}');
    return result;
}

JSON::JSON_array::~JSON_array()
{
}

std::string JSON::JSON_array::unparse(size_t depth) const
{
    std::string result = "[";
    bool first = true;
    for (std::vector<PointerHolder<JSON_value> >::const_iterator iter =
             elements.begin();
         iter != elements.end(); ++iter)
    {
        if (first)
        {
            first = false;
        }
        else
        {
            result.append(1, ',');
        }
        result.append(1, '\n');
        result.append(2 * (1 + depth), ' ');
        result += (*iter)->unparse(1 + depth);
    }
    if (! first)
    {
        result.append(1, '\n');
        result.append(2 * depth, ' ');
    }
    result.append(1, ']');
    return result;
}

JSON::JSON_string::JSON_string(std::string const& utf8) :
    encoded(encode_string(utf8))
{
}

JSON::JSON_string::~JSON_string()
{
}

std::string JSON::JSON_string::unparse(size_t) const
{
    return "\"" + encoded + "\"";
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

JSON::JSON_number::~JSON_number()
{
}

std::string JSON::JSON_number::unparse(size_t) const
{
    return encoded;
}

JSON::JSON_bool::JSON_bool(bool val) :
    value(val)
{
}

JSON::JSON_bool::~JSON_bool()
{
}

std::string JSON::JSON_bool::unparse(size_t) const
{
    return value ? "true" : "false";
}

JSON::JSON_null::~JSON_null()
{
}

std::string JSON::JSON_null::unparse(size_t) const
{
    return "null";
}

std::string
JSON::unparse() const
{
    if (0 == this->m->value.getPointer())
    {
        return "null";
    }
    else
    {
        return this->m->value->unparse(0);
    }
}

std::string
JSON::encode_string(std::string const& str)
{
    std::string result;
    size_t len = str.length();
    for (size_t i = 0; i < len; ++i)
    {
        unsigned char ch = static_cast<unsigned char>(str.at(i));
        switch (ch)
        {
          case '\\':
            result += "\\\\";
            break;
          case '\"':
            result += "\\\"";
            break;
          case '\b':
            result += "\\b";
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
            if (ch < 32)
            {
                result += "\\u" + QUtil::int_to_string_base(ch, 16, 4);
            }
            else
            {
                result.append(1, static_cast<char>(ch));
            }
        }
    }
    return result;
}

JSON
JSON::makeDictionary()
{
    return JSON(new JSON_dictionary());
}

JSON
JSON::addDictionaryMember(std::string const& key, JSON const& val)
{
    JSON_dictionary* obj = dynamic_cast<JSON_dictionary*>(
        this->m->value.getPointer());
    if (0 == obj)
    {
        throw std::runtime_error(
            "JSON::addDictionaryMember called on non-dictionary");
    }
    if (val.m->value.getPointer())
    {
        obj->members[encode_string(key)] = val.m->value;
    }
    else
    {
        obj->members[encode_string(key)] = new JSON_null();
    }
    return obj->members[encode_string(key)];
}

JSON
JSON::makeArray()
{
    return JSON(new JSON_array());
}

JSON
JSON::addArrayElement(JSON const& val)
{
    JSON_array* arr = dynamic_cast<JSON_array*>(
        this->m->value.getPointer());
    if (0 == arr)
    {
        throw std::runtime_error("JSON::addArrayElement called on non-array");
    }
    if (val.m->value.getPointer())
    {
        arr->elements.push_back(val.m->value);
    }
    else
    {
        arr->elements.push_back(new JSON_null());
    }
    return arr->elements.back();
}

JSON
JSON::makeString(std::string const& utf8)
{
    return JSON(new JSON_string(utf8));
}

JSON
JSON::makeInt(long long int value)
{
    return JSON(new JSON_number(value));
}

JSON
JSON::makeReal(double value)
{
    return JSON(new JSON_number(value));
}

JSON
JSON::makeNumber(std::string const& encoded)
{
    return JSON(new JSON_number(encoded));
}

JSON
JSON::makeBool(bool value)
{
    return JSON(new JSON_bool(value));
}

JSON
JSON::makeNull()
{
    return JSON(new JSON_null());
}

bool
JSON::checkSchema(JSON schema, std::list<std::string>& errors)
{
    return checkSchemaInternal(this->m->value.getPointer(),
                               schema.m->value.getPointer(),
                               errors, "");
}


bool
JSON::checkSchemaInternal(JSON_value* this_v, JSON_value* sch_v,
                          std::list<std::string>& errors,
                          std::string prefix)
{
    JSON_array* this_arr = dynamic_cast<JSON_array*>(this_v);
    JSON_dictionary* this_dict = dynamic_cast<JSON_dictionary*>(this_v);

    JSON_array* sch_arr = dynamic_cast<JSON_array*>(sch_v);
    JSON_dictionary* sch_dict = dynamic_cast<JSON_dictionary*>(sch_v);

    std::string err_prefix;
    if (prefix.empty())
    {
        err_prefix = "top-level object";
    }
    else
    {
        err_prefix = "json key \"" + prefix + "\"";
    }

    std::string pattern_key;
    if (sch_dict)
    {
        if (! this_dict)
        {
            QTC::TC("libtests", "JSON wanted dictionary");
            errors.push_back(err_prefix + " is supposed to be a dictionary");
            return false;
        }
        auto members = sch_dict->members;
        std::string key;
        if ((members.size() == 1) &&
            ((key = members.begin()->first, key.length() > 2) &&
             (key.at(0) == '<') &&
             (key.at(key.length() - 1) == '>')))
        {
            pattern_key = key;
        }
    }

    if (sch_dict && (! pattern_key.empty()))
    {
        auto pattern_schema = sch_dict->members[pattern_key].getPointer();
        for (const auto& iter: this_dict->members)
        {
            std::string const& key = iter.first;
            checkSchemaInternal(
                this_dict->members[key].getPointer(),
                pattern_schema,
                errors, prefix + "." + key);
        }
    }
    else if (sch_dict)
    {
        for (std::map<std::string, PointerHolder<JSON_value> >::iterator iter =
                 sch_dict->members.begin();
             iter != sch_dict->members.end(); ++iter)
        {
            std::string const& key = (*iter).first;
            if (this_dict->members.count(key))
            {
                checkSchemaInternal(
                    this_dict->members[key].getPointer(),
                    (*iter).second.getPointer(),
                    errors, prefix + "." + key);
            }
            else
            {
                QTC::TC("libtests", "JSON key missing in object");
                errors.push_back(
                    err_prefix + ": key \"" + key +
                    "\" is present in schema but missing in object");
            }
        }
        for (std::map<std::string, PointerHolder<JSON_value> >::iterator iter =
                 this_dict->members.begin();
             iter != this_dict->members.end(); ++iter)
        {
            std::string const& key = (*iter).first;
            if (sch_dict->members.count(key) == 0)
            {
                QTC::TC("libtests", "JSON key extra in object");
                errors.push_back(
                    err_prefix + ": key \"" + key +
                    "\" is not present in schema but appears in object");
            }
        }
    }
    else if (sch_arr)
    {
        if (! this_arr)
        {
            QTC::TC("libtests", "JSON wanted array");
            errors.push_back(err_prefix + " is supposed to be an array");
            return false;
        }
        if (sch_arr->elements.size() != 1)
        {
            QTC::TC("libtests", "JSON schema array error");
            errors.push_back(err_prefix +
                             " schema array contains other than one item");
            return false;
        }
        int i = 0;
        for (std::vector<PointerHolder<JSON_value> >::iterator iter =
                 this_arr->elements.begin();
             iter != this_arr->elements.end(); ++iter, ++i)
        {
            checkSchemaInternal(
                (*iter).getPointer(),
                sch_arr->elements.at(0).getPointer(),
                errors, prefix + "." + QUtil::int_to_string(i));
        }
    }

    return errors.empty();
}
