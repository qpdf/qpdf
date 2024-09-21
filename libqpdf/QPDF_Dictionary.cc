#include <qpdf/QPDF_Dictionary.hh>

#include <qpdf/JSON_writer.hh>
#include <qpdf/QPDFObjectHandle_private.hh>
#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QPDF_Name.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>

using namespace std::literals;
using namespace qpdf;

QPDF_Dictionary::QPDF_Dictionary(std::map<std::string, QPDFObjectHandle> const& items) :
    QPDFValue(::ot_dictionary),
    items(items)
{
}

QPDF_Dictionary::QPDF_Dictionary(std::map<std::string, QPDFObjectHandle>&& items) :
    QPDFValue(::ot_dictionary),
    items(items)
{
}

std::shared_ptr<QPDFObject>
QPDF_Dictionary::create(std::map<std::string, QPDFObjectHandle> const& items)
{
    return do_create(new QPDF_Dictionary(items));
}

std::shared_ptr<QPDFObject>
QPDF_Dictionary::create(std::map<std::string, QPDFObjectHandle>&& items)
{
    return do_create(new QPDF_Dictionary(items));
}

std::shared_ptr<QPDFObject>
QPDF_Dictionary::copy(bool shallow)
{
    if (shallow) {
        return create(items);
    } else {
        std::map<std::string, QPDFObjectHandle> new_items;
        for (auto const& item: this->items) {
            auto value = item.second;
            new_items[item.first] = value.isIndirect() ? value : value.shallowCopy();
        }
        return create(new_items);
    }
}

void
QPDF_Dictionary::disconnect()
{
    for (auto& iter: this->items) {
        QPDFObjectHandle::DisconnectAccess::disconnect(iter.second);
    }
}

std::string
QPDF_Dictionary::unparse()
{
    std::string result = "<< ";
    for (auto& iter: this->items) {
        if (!iter.second.isNull()) {
            result += QPDF_Name::normalizeName(iter.first) + " " + iter.second.unparse() + " ";
        }
    }
    result += ">>";
    return result;
}

void
QPDF_Dictionary::writeJSON(int json_version, JSON::Writer& p)
{
    p.writeStart('{');
    for (auto& iter: this->items) {
        if (!iter.second.isNull()) {
            p.writeNext();
            if (json_version == 1) {
                p << "\"" << JSON::Writer::encode_string(QPDF_Name::normalizeName(iter.first))
                  << "\": ";
            } else if (auto res = QPDF_Name::analyzeJSONEncoding(iter.first); res.first) {
                if (res.second) {
                    p << "\"" << iter.first << "\": ";
                } else {
                    p << "\"" << JSON::Writer::encode_string(iter.first) << "\": ";
                }
            } else {
                p << "\"n:" << JSON::Writer::encode_string(QPDF_Name::normalizeName(iter.first))
                  << "\": ";
            }
            iter.second.writeJSON(json_version, p);
        }
    }
    p.writeEnd('}');
}

QPDF_Dictionary*
BaseDictionary::dict() const
{
    if (obj) {
        if (auto d = obj->as<QPDF_Dictionary>()) {
            return d;
        }
    }
    throw std::runtime_error("Expected a dictionary but found a non-dictionary object");
    return nullptr; // unreachable
}

bool
BaseDictionary::hasKey(std::string const& key) const
{
    auto d = dict();
    return d->items.count(key) > 0 && !d->items[key].isNull();
}

QPDFObjectHandle
BaseDictionary::getKey(std::string const& key) const
{
    auto d = dict();

    // PDF spec says fetching a non-existent key from a dictionary returns the null object.
    auto item = d->items.find(key);
    if (item != d->items.end()) {
        // May be a null object
        return item->second;
    }
    static auto constexpr msg = " -> dictionary key $VD"sv;
    return QPDF_Null::create(obj, msg, key);
}

std::set<std::string>
BaseDictionary::getKeys()
{
    std::set<std::string> result;
    for (auto& iter: dict()->items) {
        if (!iter.second.isNull()) {
            result.insert(iter.first);
        }
    }
    return result;
}

std::map<std::string, QPDFObjectHandle> const&
BaseDictionary::getAsMap() const
{
    return dict()->items;
}

void
BaseDictionary::removeKey(std::string const& key)
{
    // no-op if key does not exist
    dict()->items.erase(key);
}

void
BaseDictionary::replaceKey(std::string const& key, QPDFObjectHandle value)
{
    auto d = dict();
    if (value.isNull() && !value.isIndirect()) {
        // The PDF spec doesn't distinguish between keys with null values and missing keys.
        // Allow indirect nulls which are equivalent to a dangling reference, which is
        // permitted by the spec.
        d->items.erase(key);
    } else {
        // add or replace value
        d->items[key] = value;
    }
}
