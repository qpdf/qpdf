#include <qpdf/QPDFObjectHandle_private.hh>

#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QTC.hh>
#include <qpdf/Util.hh>

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

/// Retrieves a reference to the QPDFObjectHandle associated with the given key in the
/// dictionary object contained within this instance.
///
/// If the current object is not of dictionary type, a `std::runtime_error` is thrown.
/// According to the PDF specification, missing keys in the dictionary are treated as
/// keys with a `null` value. This behavior is reflected in this function's implementation,
/// where a missing key will still return a reference to a newly inserted null value entry.
///
/// @param key The key for which the corresponding value in the dictionary is retrieved.
/// @return A reference to the QPDFObjectHandle associated with the specified key.
/// @throws std::runtime_error if the current object is not a dictionary.
QPDFObjectHandle&
BaseHandle::at(std::string const& key) const
{
    auto d = as<QPDF_Dictionary>();
    if (!d) {
        throw std::runtime_error("Expected a dictionary but found a non-dictionary object");
    }
    return d->items[key];
}

/// @brief Checks if the specified key exists in the object.
///
/// This method determines whether the given key is present in the object by verifying if the
/// associated value is non-null.
///
/// @param key The key to look for in the object.
/// @return True if the key exists and its associated value is non-null. Otherwise, returns false.
bool
BaseHandle::contains(std::string const& key) const
{
    return !(*this)[key].null();
}

/// @brief Retrieves the value associated with the given key from a dictionary.
///
/// This method attempts to find the value corresponding to the specified key for objects that can
/// be interpreted as dictionaries.
///
/// - If the object is a dictionary and the specified key exists, it returns a reference
///   to the associated value.
/// - If the object is not a dictionary or the specified key does not exist, it returns
///   a reference to a static uninitialized object handle.
///
/// @note Modifying the uninitialized object returned when the key is not found is strictly
/// prohibited.
///
/// @param key The key whose associated value should be retrieved.
/// @return A reference to the associated value if the key is found or a reference to a static
/// uninitialized object if the key is not found.
QPDFObjectHandle&
BaseHandle::find(std::string const& key) const
{
    static const QPDFObjectHandle null_obj;
    qpdf_invariant(!null_obj);
    if (auto d = as<QPDF_Dictionary>()) {
        auto it = d->items.find(key);
        if (it != d->items.end()) {
            return it->second;
        }
    }
    return const_cast<QPDFObjectHandle&>(null_obj);
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

/// @brief Replaces or removes the value associated with the given key in a dictionary.
///
/// If the current object is a dictionary, this method updates the value at the specified key.
/// If the value is a direct null object, the key is removed from the dictionary (since the PDF
/// specification doesn't distinguish between keys with null values and missing keys). Indirect
/// null values are preserved as they represent dangling references, which are permitted by the
/// specification.
///
/// @param key The key for which the value should be replaced.
/// @param value The new value to associate with the key. If this is a direct null, the key is
///              removed instead.
/// @return Returns true if the operation was performed (i.e., the current object is a dictionary).
///         Returns false if the current object is not a dictionary.
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

/// @brief Replaces or removes the value associated with the given key in a dictionary.
///
/// This method provides a stricter version of `BaseHandle::replace()` that throws an exception
/// if the current object is not a dictionary, instead of silently returning false. It delegates
/// to `BaseHandle::replace()` for the actual replacement logic.
///
/// If the value is a direct null object, the key is removed from the dictionary (since the PDF
/// specification doesn't distinguish between keys with null values and missing keys). Indirect
/// null values are preserved as they represent dangling references.
///
/// @param key The key for which the value should be replaced.
/// @param value The new value to associate with the key. If this is a direct null, the key is
///              removed instead.
/// @throws std::runtime_error if the current object is not a dictionary.
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
    if (auto result = get(key)) {
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
    auto result = get(key);
    erase(key);
    return result ? result : newNull();
}
