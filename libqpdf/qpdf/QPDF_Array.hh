#ifndef __QPDF_ARRAY_HH__
#define __QPDF_ARRAY_HH__

#include <qpdf/QPDFObject.hh>

#include <vector>
#include <qpdf/QPDFObjectHandle.hh>

class QPDF_Array: public QPDFObject
{
  public:
    QPDF_Array(std::vector<QPDFObjectHandle> const& items);
    virtual ~QPDF_Array();
    virtual std::string unparse();
    virtual QPDFObject::object_type_e getTypeCode() const;
    virtual char const* getTypeName() const;

    int getNItems() const;
    QPDFObjectHandle getItem(int n) const;
    std::vector<QPDFObjectHandle> const& getAsVector() const;

    void setItem(int, QPDFObjectHandle const&);
    void setFromVector(std::vector<QPDFObjectHandle> const& items);
    void insertItem(int at, QPDFObjectHandle const& item);
    void appendItem(QPDFObjectHandle const& item);
    void eraseItem(int at);

  protected:
    virtual void releaseResolved();

  private:
    std::vector<QPDFObjectHandle> items;
};

#endif // __QPDF_ARRAY_HH__
