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

QPDFObjectHandle const&
BaseHandle::operator[](std::string const& key) const
{
    if (auto d = as<QPDF_Dictionary>()) {
        auto it = d->items.find(key);
        if (it != d->items.end()) {
            return it->second;
        }
    }
    static const QPDFObjectHandle null_obj;
    return null_obj;
}

bool
BaseHandle::contains(std::string const& key) const
{
    return !(*this)[key].null();
}

std::set<std::string>
BaseDictionary::getKeys()
{
    std::set<std::string> result;
    for (auto& iter: dict()->items) {
        if (!iter.second.null()) {
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

size_t
BaseHandle::erase(const std::string& key)
{
    // no-op if key does not exist
    if (auto d = as<QPDF_Dictionary>()) {
        return d->items.erase(key);
    }
    return 0;
}

bool
BaseHandle::replace(std::string const& key, QPDFObjectHandle value)
{
    if (auto d = as<QPDF_Dictionary>()) {
        if (value.null() && !value.indirect()) {
            // The PDF spec doesn't distinguish between keys with null values and missing keys.
            // Allow indirect nulls which are equivalent to a dangling reference, which is permitted
            // by the spec.
            d->items.erase(key);
        } else {
            // add or replace value
            d->items[key] = value;
        }
        return true;
    }
    return false;
}

void
BaseDictionary::replace(std::string const& key, QPDFObjectHandle value)
{
    if (!BaseHandle::replace(key, value)) {
        (void)dict();
    }
}

Dictionary::Dictionary(std::map<std::string, QPDFObjectHandle>&& dict) :
    BaseDictionary(std::move(dict))
{
}

Dictionary::Dictionary(std::shared_ptr<QPDFObject> const& obj) :
    BaseDictionary(obj)
{
}

Dictionary
Dictionary::empty()
{
    return Dictionary(std::map<std::string, QPDFObjectHandle>());
}

void
QPDFObjectHandle::checkOwnership(QPDFObjectHandle const& item) const
{
    auto qpdf = getOwningQPDF();
    auto item_qpdf = item.getOwningQPDF();
    if (qpdf && item_qpdf && qpdf != item_qpdf) {
        throw std::logic_error(
            "Attempting to add an object from a different QPDF. Use "
            "QPDF::copyForeignObject to add objects from another file.");
    }
}

bool
QPDFObjectHandle::hasKey(std::string const& key) const
{
    if (Dictionary dict = *this) {
        return dict.contains(key);
    } else {
        typeWarning("dictionary", "returning false for a key containment request");
        QTC::TC("qpdf", "QPDFObjectHandle dictionary false for hasKey");
        return false;
    }
}

QPDFObjectHandle
QPDFObjectHandle::getKey(std::string const& key) const
{
    if (auto result = (*this)[key]) {
        return result;
    }
    if (isDictionary()) {
        static auto constexpr msg = " -> dictionary key $VD"sv;
        return QPDF_Null::create(obj, msg, key);
    }
    typeWarning("dictionary", "returning null for attempted key retrieval");
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
        dict.replace(key, value);
        return;
    }
    typeWarning("dictionary", "ignoring key replacement request");
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
    if (erase(key) || isDictionary()) {
        return;
    }
    typeWarning("dictionary", "ignoring key removal request");
}

QPDFObjectHandle
QPDFObjectHandle::removeKeyAndGetOld(std::string const& key)
{
    auto result = (*this)[key];
    erase(key);
    return result ? result : newNull();
}
