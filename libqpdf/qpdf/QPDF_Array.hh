#ifndef QPDF_ARRAY_HH
#define QPDF_ARRAY_HH

#include <qpdf/QPDFValue.hh>

#include <list>
#include <map>
#include <vector>

class QPDF_Array: public QPDFValue
{
  public:
    virtual ~QPDF_Array() = default;
    static std::shared_ptr<QPDFObject>
    create(std::vector<QPDFObjectHandle> const& items);
    static std::shared_ptr<QPDFObject>
    create(std::vector<std::shared_ptr<QPDFObject>>&& items);
    static std::shared_ptr<QPDFObject> create(QPDF_Array const& items);
    static std::shared_ptr<QPDFObject> create(QPDF_Array&& items);
    virtual std::shared_ptr<QPDFObject> copy(bool shallow = false);
    virtual std::string unparse();
    virtual JSON getJSON(int json_version);
    virtual void disconnect();

    // Array specific methods

    virtual int size() const;
    virtual QPDFObjectHandle at(int n) const;
    virtual bool setAt(int n, QPDFObjectHandle const& oh);
    inline void clear();
    virtual bool erase(int at);
    virtual bool insert(int at, QPDFObjectHandle const& item);
    virtual bool push_back(QPDFObjectHandle const& item);
    std::vector<QPDFObjectHandle> getAsVector() const;
    void setFromVector(std::vector<QPDFObjectHandle> const& items);
    void setFromVector(std::vector<std::shared_ptr<QPDFObject>>&& items);

    // Helper methods for QPDF and QPDFObjectHandle -- these are
    // public methods since the whole class is not part of the public
    // API. Otherwise, these would be wrapped in accessor classes.
    void addExplicitElementsToList(std::list<QPDFObjectHandle>&) const;

  private:
    QPDF_Array();
    QPDF_Array(std::vector<QPDFObjectHandle> const& items);
    QPDF_Array(std::vector<std::shared_ptr<QPDFObject>>&& items);
    QPDF_Array(QPDF_Array const& items);
    QPDF_Array(QPDF_Array&& items);

    QPDFObjectHandle
    unchecked_at(int n) const
    {
        auto const& iter = elements.find(n);
        if (iter != elements.end()) {
            return (*iter).second;
        } else {
            return {};
        }
    }

    std::map<int, QPDFObjectHandle> elements{};
    int n_elements{0};
};

inline void
QPDF_Array::clear()
{
    elements.clear();
    n_elements = 0;
}

#endif // QPDF_ARRAY_HH
