
#ifndef __QPDF_DICTIONARY_HH__
#define __QPDF_DICTIONARY_HH__

#include <qpdf/QPDFObject.hh>

#include <set>
#include <map>

#include <qpdf/QPDFObjectHandle.hh>

class QPDF_Dictionary: public QPDFObject
{
  public:
    QPDF_Dictionary(std::map<std::string, QPDFObjectHandle> const& items);
    virtual ~QPDF_Dictionary();
    virtual std::string unparse();

    // hasKey() and getKeys() treat keys with null values as if they
    // aren't there.  getKey() returns null for the value of a
    // non-existent key.  This is as per the PDF spec.
    bool hasKey(std::string const&);
    QPDFObjectHandle getKey(std::string const&);
    std::set<std::string> getKeys();

    // Repalce value of key, adding it if it does not exist
    void replaceKey(std::string const& key, QPDFObjectHandle const&);
    // Remove key, doing nothing if key does not exist
    void removeKey(std::string const& key);

  private:
    std::map<std::string, QPDFObjectHandle> items;
};

#endif // __QPDF_DICTIONARY_HH__
