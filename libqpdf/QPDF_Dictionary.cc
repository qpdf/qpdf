#include <qpdf/QPDF_Dictionary.hh>

#include <qpdf/QPDF_Name.hh>

QPDF_Dictionary::QPDF_Dictionary(
    std::map<std::string, QPDFObjectHandle> const& items) :
    QPDFValue(::ot_dictionary, "dictionary"),
    items(items)
{
}

QPDF_Dictionary::QPDF_Dictionary(
    std::map<std::string, QPDFObjectHandle>&& items) :
    QPDFValue(::ot_dictionary, "dictionary"),
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
            new_items[item.first] =
                value.isIndirect() ? value : value.shallowCopy();
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
            result += QPDF_Name::normalizeName(iter.first) + " " +
                iter.second.unparse() + " ";
        }
    }
    result += ">>";
    return result;
}

JSON
QPDF_Dictionary::getJSON(int json_version)
{
    JSON j = JSON::makeDictionary();
    for (auto& iter: this->items) {
        if (!iter.second.isNull()) {
            std::string key =
                (json_version == 1 ? QPDF_Name::normalizeName(iter.first)
                                   : iter.first);
            j.addDictionaryMember(key, iter.second.getJSON(json_version));
        }
    }
    return j;
}

bool
QPDF_Dictionary::hasKey(std::string const& key)
{
    return ((this->items.count(key) > 0) && (!this->items[key].isNull()));
}

QPDFObjectHandle
QPDF_Dictionary::getKey(std::string const& key)
{
    // PDF spec says fetching a non-existent key from a dictionary
    // returns the null object.
    auto item = this->items.find(key);
    if (item != this->items.end()) {
        // May be a null object
        return item->second;
    } else {
        auto null = QPDFObjectHandle::newNull();
        if (qpdf != nullptr) {
            null.setObjectDescription(
                qpdf, getDescription() + " -> dictionary key " + key);
        }
        return null;
    }
}

std::set<std::string>
QPDF_Dictionary::getKeys()
{
    std::set<std::string> result;
    for (auto& iter: this->items) {
        if (!iter.second.isNull()) {
            result.insert(iter.first);
        }
    }
    return result;
}

std::map<std::string, QPDFObjectHandle> const&
QPDF_Dictionary::getAsMap() const
{
    return this->items;
}

void
QPDF_Dictionary::replaceKey(std::string const& key, QPDFObjectHandle value)
{
    if (value.isNull()) {
        // The PDF spec doesn't distinguish between keys with null
        // values and missing keys.
        removeKey(key);
    } else {
        // add or replace value
        this->items[key] = value;
    }
}

void
QPDF_Dictionary::removeKey(std::string const& key)
{
    // no-op if key does not exist
    this->items.erase(key);
}
