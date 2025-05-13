#include <qpdf/QPDFObjectHandle_private.hh>

#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QTC.hh>

using namespace std::literals;
using namespace qpdf;

QPDF_Dictionary*
BaseDictionary::dict() const
{
    if (auto d = as<QPDF_Dictionary>()) {
        return d;
    }
    throw std::runtime_error("Expected a dictionary but found a non-dictionary object");
    return nullptr; // unreachable
}

bool
BaseDictionary::hasKey(std::string const& key) const
{
    auto d = dict();
    return d->items.contains(key) && !d->items[key].null();
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
