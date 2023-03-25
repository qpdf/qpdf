#ifndef QPDF_ARRAY_HH
#define QPDF_ARRAY_HH

#include <qpdf/QPDFValue.hh>

#include <qpdf/SparseOHArray.hh>
#include <vector>

class QPDF_Array: public QPDFValue
{
  public:
    virtual ~QPDF_Array() = default;
    static std::shared_ptr<QPDFObject>
    create(std::vector<QPDFObjectHandle> const& items);
    static std::shared_ptr<QPDFObject>
    create(std::vector<std::shared_ptr<QPDFObject>>&& items, bool sparse);
    static std::shared_ptr<QPDFObject> create(SparseOHArray const& items);
    static std::shared_ptr<QPDFObject>
    create(std::vector<std::shared_ptr<QPDFObject>> const& items);
    virtual std::shared_ptr<QPDFObject> copy(bool shallow = false);
    virtual std::string unparse();
    virtual JSON getJSON(int json_version);
    virtual void disconnect();

    int getNItems() const;
    QPDFObjectHandle getItem(int n) const;
    void getAsVector(std::vector<QPDFObjectHandle>&) const;

    void setItem(int, QPDFObjectHandle const&);
    void setFromVector(std::vector<QPDFObjectHandle> const& items);
    void setFromVector(std::vector<std::shared_ptr<QPDFObject>>&& items);
    void insertItem(int at, QPDFObjectHandle const& item);
    void appendItem(QPDFObjectHandle const& item);
    void eraseItem(int at);

  private:
    QPDF_Array(std::vector<QPDFObjectHandle> const& items);
    QPDF_Array(std::vector<std::shared_ptr<QPDFObject>>&& items, bool sparse);
    QPDF_Array(SparseOHArray const& items);
    QPDF_Array(std::vector<std::shared_ptr<QPDFObject>> const& items);
    bool sparse{false};
    SparseOHArray sp_elements;
    std::vector<std::shared_ptr<QPDFObject>> elements;
};

#endif // QPDF_ARRAY_HH
