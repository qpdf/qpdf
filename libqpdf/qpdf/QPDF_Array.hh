#ifndef QPDF_ARRAY_HH
#define QPDF_ARRAY_HH

#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QPDFValue.hh>

#include <map>
#include <vector>

class QPDF_Array: public QPDFValue
{
  private:
    struct Sparse
    {
        int size{0};
        std::map<int, std::shared_ptr<QPDFObject>> elements;
    };

  public:
    ~QPDF_Array() override = default;
    static std::shared_ptr<QPDFObject> create(std::vector<QPDFObjectHandle> const& items);
    static std::shared_ptr<QPDFObject>
    create(std::vector<std::shared_ptr<QPDFObject>>&& items, bool sparse);
    std::shared_ptr<QPDFObject> copy(bool shallow = false) override;
    std::string unparse() override;
    void writeJSON(int json_version, JSON::Writer& p) override;
    void disconnect() override;

  private:
    friend class qpdf::Array;
    QPDF_Array();
    QPDF_Array(QPDF_Array const&);
    QPDF_Array(std::vector<QPDFObjectHandle> const& items);
    QPDF_Array(std::vector<std::shared_ptr<QPDFObject>>&& items, bool sparse);

    int
    size() const
    {
        return sp ? sp->size : int(elements.size());
    }
    void setFromVector(std::vector<QPDFObjectHandle> const& items);

    void checkOwnership(QPDFObjectHandle const& item) const;

    std::unique_ptr<Sparse> sp;
    std::vector<std::shared_ptr<QPDFObject>> elements;
};

#endif // QPDF_ARRAY_HH
