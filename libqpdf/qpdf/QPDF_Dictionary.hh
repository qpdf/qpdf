#ifndef QPDF_DICTIONARY_HH
#define QPDF_DICTIONARY_HH

#include <qpdf/QPDFValue.hh>

#include <map>
#include <set>

#include <qpdf/QPDFExc.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QPDF_Null.hh>

class QPDF_Dictionary: public QPDFValue
{
  public:
    ~QPDF_Dictionary() override = default;
    static std::shared_ptr<QPDFObject> create(std::map<std::string, QPDFObjectHandle> const& items);
    static std::shared_ptr<QPDFObject> create(std::map<std::string, QPDFObjectHandle>&& items);
    std::shared_ptr<QPDFObject> copy(bool shallow = false) override;
    std::string unparse() override;
    void writeJSON(int json_version, JSON::Writer& p) override;
    void disconnect() override;

  private:
    friend class qpdf::BaseDictionary;

    QPDF_Dictionary(std::map<std::string, QPDFObjectHandle> const& items);
    QPDF_Dictionary(std::map<std::string, QPDFObjectHandle>&& items);

    std::map<std::string, QPDFObjectHandle> items;
};

#endif // QPDF_DICTIONARY_HH
