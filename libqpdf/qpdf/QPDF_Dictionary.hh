#ifndef QPDF_DICTIONARY_HH
#define QPDF_DICTIONARY_HH

#include <qpdf/QPDFValue.hh>

#include <map>
#include <set>

#include <qpdf/QPDFObjectHandle.hh>

class QPDF_Dictionary: public QPDFValue
{
    friend class QPDFObjectHandle::QPDFDictItems::iterator;

  public:
    virtual ~QPDF_Dictionary() = default;
    static std::shared_ptr<QPDFObject>
    create(std::map<std::string, QPDFObjectHandle> const& items);
    static std::shared_ptr<QPDFObject>
    create(std::map<std::string, QPDFObjectHandle>&& items);
    virtual std::shared_ptr<QPDFObject> copy(bool shallow = false);
    virtual std::string unparse();
    virtual JSON getJSON(int json_version);
    virtual void disconnect();

    // hasKey() and getKeys() treat keys with null values as if they
    // aren't there.  getKey() returns null for the value of a
    // non-existent key.  This is as per the PDF spec.
    bool hasKey(std::string const&);
    QPDFObjectHandle getKey(std::string const&);
    std::set<std::string> getKeys();
    std::map<std::string, QPDFObjectHandle> const& getAsMap() const;

    // If value is null, remove key; otherwise, replace the value of
    // key, adding it if it does not exist.
    void replaceKey(std::string const& key, QPDFObjectHandle value);
    // Remove key, doing nothing if key does not exist
    void removeKey(std::string const& key);

    // Methods to support iteration over dictionaries, skipping null elements.
    // These methods use pairs of map iterators, where the first element of the
    // pair is an iterator pointing to the current (non-null) element, and the
    // second element is the end iterator. The increment method increments the
    // iterator pair to the next non-null map element.
    using diter = std::map<std::string, QPDFObjectHandle>::iterator;

    std::pair<diter, diter>
    getBegin()
    {
        auto begin = items.begin();
        auto end = items.end();
        while (begin != end && begin->second.isNull()) {
            ++begin;
        }
        return {begin, end};
    }
    std::pair<diter, diter>
    getEnd()
    {
        auto end = items.end();
        return {end, end};
    }
    static void
    increment(std::pair<diter, diter>& iter)
    {
        if (iter.first != iter.second) {
            ++iter.first;
            while (iter.first != iter.second && iter.first->second.isNull()) {
                ++iter.first;
            }
        }
    }

  private:
    QPDF_Dictionary(std::map<std::string, QPDFObjectHandle> const& items);
    QPDF_Dictionary(std::map<std::string, QPDFObjectHandle>&& items);
    std::map<std::string, QPDFObjectHandle> items;
};

#endif // QPDF_DICTIONARY_HH
