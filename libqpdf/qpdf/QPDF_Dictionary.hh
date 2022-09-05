#ifndef QPDF_DICTIONARY_HH
#define QPDF_DICTIONARY_HH

#include <qpdf/QPDFValue.hh>

#include <map>
#include <set>

#include <qpdf/QPDFObjectHandle.hh>

class QPDF_Dictionary: public QPDFValue
{
  public:
    virtual ~QPDF_Dictionary() = default;
    static std::shared_ptr<QPDFValueProxy>
    create(std::map<std::string, QPDFObjectHandle> const& items);
    virtual std::shared_ptr<QPDFValueProxy> shallowCopy();
    virtual std::string unparse();
    virtual JSON getJSON(int json_version);

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

  private:
    QPDF_Dictionary(std::map<std::string, QPDFObjectHandle> const& items);
    std::map<std::string, QPDFObjectHandle> items;
};

#endif // QPDF_DICTIONARY_HH
