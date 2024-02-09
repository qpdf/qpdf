#ifndef QPDF_ARRAY_HH
#define QPDF_ARRAY_HH

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
    JSON getJSON(int json_version) override;
    void writeJSON(int json_version, JSON::Writer& p) override;
    void disconnect() override;

    int
    size() const noexcept
    {
        return sp ? sp->size : int(elements.size());
    }
    QPDFObjectHandle at(int n) const noexcept;
    bool setAt(int n, QPDFObjectHandle const& oh);
    std::vector<QPDFObjectHandle> getAsVector() const;
    void setFromVector(std::vector<QPDFObjectHandle> const& items);
    bool insert(int at, QPDFObjectHandle const& item);
    void push_back(QPDFObjectHandle const& item);
    bool erase(int at);

  private:
    QPDF_Array();
    QPDF_Array(QPDF_Array const&);
    QPDF_Array(std::vector<QPDFObjectHandle> const& items);
    QPDF_Array(std::vector<std::shared_ptr<QPDFObject>>&& items, bool sparse);

    void checkOwnership(QPDFObjectHandle const& item) const;

    std::unique_ptr<Sparse> sp;
    std::vector<std::shared_ptr<QPDFObject>> elements;
};

#endif // QPDF_ARRAY_HH
