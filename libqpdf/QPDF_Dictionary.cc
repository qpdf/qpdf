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
            result += Name::normalize(iter.first) + " " + iter.second.unparse() + " ";
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
                p << "\"" << JSON::Writer::encode_string(Name::normalize(iter.first)) << "\": ";
            } else if (auto res = Name::analyzeJSONEncoding(iter.first); res.first) {
                if (res.second) {
                    p << "\"" << iter.first << "\": ";
                } else {
                    p << "\"" << JSON::Writer::encode_string(iter.first) << "\": ";
                }
            } else {
                p << "\"n:" << JSON::Writer::encode_string(Name::normalize(iter.first)) << "\": ";
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

void
QPDFObjectHandle::checkOwnership(QPDFObjectHandle const& item) const
{
    auto qpdf = getOwningQPDF();
    auto item_qpdf = item.getOwningQPDF();
    if (qpdf && item_qpdf && qpdf != item_qpdf) {
        QTC::TC("qpdf", "QPDFObjectHandle check ownership");
        throw std::logic_error(
            "Attempting to add an object from a different QPDF. Use "
            "QPDF::copyForeignObject to add objects from another file.");
    }
}

bool
QPDFObjectHandle::hasKey(std::string const& key) const
{
    auto dict = as_dictionary(strict);
    if (dict) {
        return dict.hasKey(key);
    } else {
        typeWarning("dictionary", "returning false for a key containment request");
        QTC::TC("qpdf", "QPDFObjectHandle dictionary false for hasKey");
        return false;
    }
}

QPDFObjectHandle
QPDFObjectHandle::getKey(std::string const& key) const
{
    if (auto dict = as_dictionary(strict)) {
        return dict.getKey(key);
    }
    typeWarning("dictionary", "returning null for attempted key retrieval");
    QTC::TC("qpdf", "QPDFObjectHandle dictionary null for getKey");
    static auto constexpr msg = " -> null returned from getting key $VD from non-Dictionary"sv;
    return QPDF_Null::create(obj, msg, "");
}

QPDFObjectHandle
QPDFObjectHandle::getKeyIfDict(std::string const& key) const
{
    return isNull() ? newNull() : getKey(key);
}

std::set<std::string>
QPDFObjectHandle::getKeys() const
{
    if (auto dict = as_dictionary(strict)) {
        return dict.getKeys();
    }
    typeWarning("dictionary", "treating as empty");
    QTC::TC("qpdf", "QPDFObjectHandle dictionary empty set for getKeys");
    return {};
}

std::map<std::string, QPDFObjectHandle>
QPDFObjectHandle::getDictAsMap() const
{
    if (auto dict = as_dictionary(strict)) {
        return dict.getAsMap();
    }
    typeWarning("dictionary", "treating as empty");
    QTC::TC("qpdf", "QPDFObjectHandle dictionary empty map for asMap");
    return {};
}

void
QPDFObjectHandle::replaceKey(std::string const& key, QPDFObjectHandle const& value)
{
    if (auto dict = as_dictionary(strict)) {
        checkOwnership(value);
        dict.replaceKey(key, value);
        return;
    }
    typeWarning("dictionary", "ignoring key replacement request");
    QTC::TC("qpdf", "QPDFObjectHandle dictionary ignoring replaceKey");
}

QPDFObjectHandle
QPDFObjectHandle::replaceKeyAndGetNew(std::string const& key, QPDFObjectHandle const& value)
{
    replaceKey(key, value);
    return value;
}

QPDFObjectHandle
QPDFObjectHandle::replaceKeyAndGetOld(std::string const& key, QPDFObjectHandle const& value)
{
    QPDFObjectHandle old = removeKeyAndGetOld(key);
    replaceKey(key, value);
    return old;
}

void
QPDFObjectHandle::removeKey(std::string const& key)
{
    if (auto dict = as_dictionary(strict)) {
        dict.removeKey(key);
        return;
    }
    typeWarning("dictionary", "ignoring key removal request");
    QTC::TC("qpdf", "QPDFObjectHandle dictionary ignoring removeKey");
}

QPDFObjectHandle
QPDFObjectHandle::removeKeyAndGetOld(std::string const& key)
{
    auto result = QPDFObjectHandle::newNull();
    if (auto dict = as_dictionary(strict)) {
        result = dict.getKey(key);
    }
    removeKey(key);
    return result;
}
